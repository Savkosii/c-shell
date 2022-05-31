#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../api/entry.h"

static int make_directory_once(const struct entry *entry, mode_t mode_bits, const int option[]) {
    if (!is_directory(entry->previous)) {
        log_error("mkdir: cannot create directory '%s': No such file or directory", entry->received_path);
        return -1;
    }

    if (!is_directory_write_permitted(entry->previous)) {
        log_error("mkdir: cannot create directory '%s': Permission denied", entry->received_path);
        return -1;
    }

    return mkdir(entry->real_path, mode_bits) | chmod(entry->real_path, mode_bits);
}

static int make_directory_recursively(const struct entry *entry, mode_t mode_bits, const int option[]) {
    struct entry *subdirectory;
    char subdirectory_path[MAX_LEN];
    char *p, *pathdup, *token;
    const char *q;
    size_t offset, subdirectory_path_len = 0;
    int retval = 0;

    pathdup = strdup(entry->received_path);

    p = subdirectory_path;
    
    if (*pathdup == '/') {
        *p++ = '/';
    }

    q = strtok_r(pathdup, "/", &token);

    if (q == NULL) {
        log_error("mkdir: cannot create directory '/': File exists");
        free(pathdup);
        return -1;
    }

    for (; q != NULL; q = strtok_r(NULL, "/", &token)) {
        if ((offset = snprintf(p, MAX_LEN, "%s/", q)) > MAX_LEN) {
            die("mkdir: Error: buffer overflowed");
        }
        if (subdirectory_path_len += offset > MAX_LEN) {
            die("mkdir: Error: buffer overflowed");
        }
        p += offset;

        if (strcmp("..", q) == 0 || strcmp(".", q) == 0) {
            continue;

        } else {
            subdirectory = get_entries_chain(subdirectory_path);
            if (is_entry_located(subdirectory)) {
                if (is_directory(subdirectory)) {
                    continue;

                } else {
                    die("mkdir: cannot create directory '%s': Not a directory", subdirectory->received_path);
                }
            }
            retval |= make_directory_once(subdirectory, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH, option);
            free_entry(subdirectory);
        }
    }
    
    free(pathdup);

    chmod(entry->received_path, mode_bits);

    return retval;
}

static int make_directory(const struct entry *entry, const mode_t mode_bits, const int option[]) {
    if (!is_entry_located(entry)) {
        if (is_entry_located(entry->previous)) {
            return make_directory_once(entry, mode_bits, option);

        } else if (option['p'] == 0) {
            log_error("mkdir: cannot create directory '%s': No such file or directory", entry->received_path);
            return -1;

        } else {
            return make_directory_recursively(entry, mode_bits, option);
        }

    } else {
        log_error("mkdir: cannot create directory '%s': File exists", entry->received_path);
        return -1;
    }
}


static int make_directories(char *paths[], size_t path_nums, mode_t mode_bits, const int option[]) {
    struct entry *entry;
    int retval = 0;

    for (size_t i = 0; i < path_nums; i++) {
        entry = get_entries_chain(paths[i]);
        retval |= make_directory(entry, mode_bits, option);
        free_entry(entry);
    }

    return retval;
}

static bool is_nums_sequence(const char *string) {
    while (*string) {
        if (!isdigit(*string++)) return false;
    }
    return true;
}

static bool is_rwx_sequence(const char *string) {
    for (;*string; string++) {
        if (*string != 'r' && *string != 'w' && *string != 'x') return false;
    }
    return true;
}

static mode_t analyze_nums_mode_bits(const char *string){
    char buffer[3];
    mode_t mode_bits = 0;
    size_t len = strlen(string);
    memset(buffer, 0, sizeof (buffer));

    if (len > 3) {
        die("mkdir: invalid mode: '%s'", string);
    }

    if (len == 1) {
        buffer[2] = *string;

    } else if (len == 2) {
        buffer[1] = *string;
        buffer[2] = *(string + 1);

    } else {
        memcpy(buffer, string, sizeof (buffer));
    }

    int n = *buffer - '0';
    if (n == 1) mode_bits |= S_IXUSR;
    if (n == 2) mode_bits |= S_IWUSR;
    if (n == 3) mode_bits |= S_IXUSR | S_IWUSR;
    if (n == 4) mode_bits |= S_IRUSR;
    if (n == 5) mode_bits |= S_IXUSR | S_IRUSR;
    if (n == 6) mode_bits |= S_IRUSR | S_IWUSR;
    if (n == 7) mode_bits |= S_IRUSR | S_IWUSR | S_IXUSR;

    n = *(buffer + 1) - '0';
    if (n == 1) mode_bits |= S_IXGRP;
    if (n == 2) mode_bits |= S_IWGRP;
    if (n == 3) mode_bits |= S_IXGRP | S_IWGRP;
    if (n == 4) mode_bits |= S_IRGRP;
    if (n == 5) mode_bits |= S_IXGRP | S_IRGRP;
    if (n == 6) mode_bits |= S_IRGRP | S_IWGRP;
    if (n == 7) mode_bits |= S_IRGRP | S_IWGRP | S_IXGRP;

    n = *(buffer + 2) - '0';
    if (n == 1) mode_bits |= S_IXOTH;
    if (n == 2) mode_bits |= S_IWOTH;
    if (n == 3) mode_bits |= S_IXOTH | S_IWOTH;
    if (n == 4) mode_bits |= S_IROTH;
    if (n == 5) mode_bits |= S_IXOTH | S_IROTH;
    if (n == 6) mode_bits |= S_IROTH | S_IWOTH;
    if (n == 7) mode_bits |= S_IROTH | S_IWOTH | S_IXOTH;
    return mode_bits;
}

static bool try_match_option(const char *arg, int *option_buf, mode_t *mode_bits_buf) {
    const char *p;
    mode_t mode_bits = 0;
    
    if (*arg == '-' && *(arg + 1) != '\0') {
        if (*(arg + 1) == '-') {
            p = arg + 2;
            if (strcmp(p, "parent") == 0) {
                option_buf['p'] = 1;
                return true;

            } else if (strncmp(p, "mode", 4) == 0) {
                if (*(p + 4) == '=') {
                    p += 5;

                } else {
                    die("mkdir: option requires an argument '--mode'");
                }

            } else {
                if (*p != '\0') {
                    die("mkdir: unknown options '--%s'", p);
                }
            }

        } else {
            p = arg + 1;

            if (*p == 'p') {
                option_buf['p'] = 1;
                return true;

            } else if (*p == 'm') {
                if (*(p + 1) == '=') {
                    p += 2;
                } else {
                    die("mkdir: option requires an argument -- 'm'");
                }

            } else {
                die("mkdir: unknown options -- '%s'", p);
            }
        }

        if (is_rwx_sequence(p)) {
            for (const char *q = p; *q; q++) {
                if (*q == 'r') {
                    mode_bits |= S_IRUSR | S_IRGRP | S_IROTH;

                } else if (*q == 'w') {
                    mode_bits |= S_IWUSR | S_IWGRP;

                } else {
                        mode_bits |= S_IXUSR | S_IXGRP | S_IXOTH;
                }
            }

        } else if (is_nums_sequence(p)){
            mode_bits = analyze_nums_mode_bits(p);

        } else {
            die("mkdir: invalid mode '%s'", p);
        }

        *mode_bits_buf = mode_bits;

        return true;
    }
    else return false;
}


static void parse(int argc, char *argv[], mode_t *mode_bits_buf, int *option_buf, char *paths_buf[], size_t *path_nums_buf) {
    mode_t mode_bits = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    size_t paths_nums = 0;

    for (int i = 1; i < argc; i++) {
        if (!try_match_option(argv[i], option_buf, &mode_bits)) {
            paths_buf[paths_nums++] = argv[i];
        }
    }

    if (paths_nums == 0) {
        die("mkdir: missing operand");
    }

    *mode_bits_buf = mode_bits;
    *path_nums_buf = paths_nums;
}

int main(int argc, char *argv[]) {
    char *paths[MAX_SIZE];
    int option[128];
    size_t path_nums;
    mode_t mode_bits;
    puts_program_name(argv[0]);
    memset(option, 0, sizeof option);
    parse(argc, argv, &mode_bits, option, paths, &path_nums);
    return make_directories(paths, path_nums, mode_bits, option);
}