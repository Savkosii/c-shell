#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "api/entry.h"

static bool is_nums_sequence(const char *string) {
    while (*string) {
        if (!isdigit(*string++)) return false;
    }
    return true;
}

static mode_t analyze_mode_bits(char *string){
    char buffer[3];
    mode_t mode_bits = 0;
    size_t len = strlen(string);
    memset(buffer, 0, sizeof (buffer));

    if (len > 3 || !is_nums_sequence(string)) {
        die("chmod: invalid mode: '%s'", string);
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

static int change_file_mode(struct entry *entry, mode_t mode_bits) {
    if (!is_entry_located(entry)) {
        log_error("chmod: cannot access '%s': No such file or directory", entry->received_path);
        return -1;
    }

    if (!is_directory_read_permitted(entry->previous)) {
        log_error("chmod: cannot access '%s': Permission denied", entry->received_path);
        return -1;
    }

    return chmod(entry->real_path, mode_bits);
}

static int change_files_mode(char *paths[], size_t path_nums, mode_t mode_bits) {
    int retval = 0;

    for (size_t i = 0; i < path_nums; i++) {
        struct entry *entry = get_entries_chain(paths[i]);
        retval |= change_file_mode(entry, mode_bits);
        free_entry(entry);
    }
    
    return retval;
}

static bool try_match_option(const char *arg, int *option_buf, mode_t *mode_bits_buf) {
    const char *p;
    mode_t mode_bits = 0;

    if (*arg == '-' && *(arg + 1) != '\0') {
        p = arg + 1;
        if (*p == 'u') {
            if (*(p + 1) == '=') {
                p += 2;

            } else {
                die("chmod: option requires an argument -- 'm'");
            }

        } else {
            die("chmod: unknown options -- '%s'", p);
        }

        for (const char *q = p; *q; q++) {
            if (*q == 'r') {
                mode_bits |= S_IRUSR | S_IRGRP | S_IROTH;

            } else if (*q == 'w') {
                mode_bits |= S_IWUSR | S_IWGRP;

            } else if (*q == 'x') {
                mode_bits |= S_IXUSR | S_IXGRP | S_IXOTH;

            } else {
                die("chmod: invalid mode '%s'", p);
            }
        }

        *mode_bits_buf = mode_bits;
        return true;
    }
    else return false;
}

static void parse(int argc, char *argv[], mode_t *mode_bits_buf, int *option_buf, char *paths_buf[], size_t *path_nums_buf) {
    int flag = 0;
    mode_t mode_bits = 0;
    size_t paths_nums = 0;

    if (argc < 3) {
        die("chmod: missing operand");
    }

    for (int i = 1; i < argc; i++) {
        if (try_match_option(argv[i], option_buf, &mode_bits)) {
            flag = 1;
        }
    }

    if (flag == 1) {
        for (int i = 1; i < argc; i++) {
            if (!try_match_option(argv[i], option_buf, &mode_bits)) {
                paths_buf[paths_nums++] = argv[i];
            }
        }

    } else {
        mode_bits = analyze_mode_bits(argv[1]);
        for (int i = 2; i < argc; i++) {
            paths_buf[paths_nums++] = argv[i];
        }
    }

    if (paths_nums == 0) {
        die("chmod: missing operand");
    }

    *mode_bits_buf = mode_bits;
    *path_nums_buf = paths_nums;
}


int main (int argc, char *argv[]){
    char *paths[MAX_SIZE];
    int option[128];
    size_t path_nums;
    mode_t mode_bits;
    puts_program_name(argv[0]);
    memset(option, 0, sizeof option);
    parse(argc, argv, &mode_bits, option, paths, &path_nums);
    return change_files_mode(paths, path_nums, mode_bits);
}