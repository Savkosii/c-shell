#include <unistd.h>
#include <dirent.h>

#include "api/entry.h"

static int remove_file(const struct entry *file, const int option[]) {
    if (option['i'] == 1) {
        fprintf(stdout, "rm: remove regular file '%s'? ", file->received_path);
        int c = fgetc(stdin);
        fgetc(stdin);
        if (c != 'y') return 0;
    }
    return unlink(file->real_path);
}

static int remove_empty_directory(const struct entry *directory, const int option[]) {
    if (option['i'] == 1) {
        fprintf(stdout, "rm: remove directory '%s'? ", directory->received_path);
        int c = fgetc(stdin);
        fgetc(stdin);
        if (c != 'y') return 0;
    }
    return rmdir(directory->real_path);
}

static int remove_directory_recursively(const struct entry *directory, const int option[]) {
    if (option['i'] == 1) {
        fprintf(stdout, "rm: descend into directory '%s'? ", directory->received_path);
        int c = fgetc(stdin);
        fgetc(stdin);
        if (c != 'y') return 0;
    }
    int retval = 0;
    DIR *stream;
    struct dirent *element;
    struct entry *entry;

    if ((stream = opendir(directory->real_path)) == NULL) {
        return -1;
    }

    while ((element = readdir(stream)) != NULL) {
        if (strcmp(element->d_name, ".") == 0 || strcmp(element->d_name, "..") == 0) {
            continue;
        }

        entry = get_joint_entry(element->d_name, directory);

        if (is_file(entry)) {
            retval |= remove_file(entry, option);

        } else {
            if (!is_directory_write_permitted(entry)) {
                log_error("rm: cannot remove '%s': Permission denied", entry->received_path);
                retval = -1;
                
            } else if (is_empty_directory(entry)) {
                retval = remove_empty_directory(entry, option);

            } else {
                retval = remove_directory_recursively(entry, option);
            }
        }
        free_entry(entry);
    }

    closedir(stream);

    if (retval == -1) {
        log_error("rm: cannot descend into directory '%s'", directory->received_path);
        return -1;

    } else {
        return remove_empty_directory(directory, option);
    }
}

static int remove_directory(const struct entry *entry, const int option[]) {
    int retval;
    struct entry *entry_cwd = get_entries_chain(getcwd(NULL, 0));

    if (option['d'] == 0 && option['r'] == 0) {
        log_error("rm: cannot remove '%s': Is a directory", entry->received_path);
        retval = -1;

    } else if (option['d'] == 1 && !is_empty_directory(entry)) {
        log_error("rm: cannot remove '%s': Directory not empty", entry->received_path);
        retval = -1;

    } else if (is_same_entry(entry_cwd, entry) || is_subdirectory(entry_cwd, entry)) {
        log_error("rm: cannot remove '%s': Device or resource busy", entry->received_path);
        retval = -1;

    } else {
        if (!is_directory_write_permitted(entry)) {
            log_error("rm: cannot remove '%s': Permission denied", entry->received_path);
            retval = -1;

        } else if (is_empty_directory(entry)) {
            retval = remove_empty_directory(entry, option);

        } else {
            retval = remove_directory_recursively(entry, option);
        }
    }

    free_entry(entry_cwd);
    return retval;
}

static int remove_entry(const struct entry *entry, const int option[]) {
    if (!is_entry_located(entry)) {
        if (option['f'] == 1) {
            return 0;

        } else {
            log_error("rm: cannot access '%s': No such file or directory", entry->received_path);
            return -1;
        }

    } else if (is_file(entry)) {
        if (!is_file_write_permitted(entry)) {
            log_error("rm: cannot remove file '%s': Permission denied", entry->received_path);
            return -1;
        }
        return remove_file(entry, option);

    } else {
        return remove_directory(entry, option);
    }

    return -1;
}

static int remove_entries(char *paths[], size_t paths_nums, const int option[]) {
    struct entry *entry;
    int retval = 0;

    for (size_t i = 0; i < paths_nums; i++) {
        entry = get_entries_chain(paths[i]);
        retval |= remove_entry(entry, option);
        free_entry(entry);
    }
    
    return retval;
}

static bool try_match_option(const char *arg, int *option_buf) {
    const char *p;
    
    if (*arg == '-' && *(arg + 1) != '\0') {
        if (*(arg + 1) == '-') {
            p = arg + 2;
            if (strcmp(p, "recursive") == 0) {
                option_buf['r'] = 1;

            } else if (strcmp(p, "dir")) {
                option_buf['d'] = 1;

            } else if (strcmp(p, "interactive") == 0) {
                option_buf['i'] = 1;
                option_buf['f'] = 0;

            } else if (strcmp(p, "force") == 0) {
                option_buf['f'] = 1;
                option_buf['i'] = 0;

            } else {
                die("rm: unknown options '--%s'", p);
            }

        } else {
            p = arg + 1;
            while (*p) {
                if (*p == 'r' || *p == 'R') {
                    option_buf['r'] = 1;

                } else if (*p == 'd') {
                    option_buf['d'] = 1;

                } else if (*p == 'f') {
                    option_buf['f'] = 1;
                    option_buf['i'] = 0;

                } else if (*p == 'i') {
                    option_buf['i'] = 1;
                    option_buf['f'] = 0;

                } else {
                    die("rm: unknown options -- '%c'", *p);
                }

                p++;
            }
        }
        return true;
    } 
    else return false;
}

static void parse(int argc, char *argv[], int *option_buf, char *paths_buf[], size_t *paths_nums_buf) {
    size_t paths_nums = 0;

    for (int i = 1; i < argc; i++) {
        if (!try_match_option(argv[i], option_buf)) {
            paths_buf[paths_nums++] = argv[i];
        }
    }

    if (paths_nums == 0) {
        die("rm: missing operand");
    }

    *paths_nums_buf = paths_nums;
}

int main(int argc, char *argv[]) {
    int option[128];
    char *paths[MAX_SIZE];
    size_t paths_nums;
    puts_program_name(argv[0]);
    memset(option, 0, sizeof(option));
    setbuf(stdout, NULL);
    parse(argc, argv, option, paths, &paths_nums);
    return remove_entries(paths, paths_nums, option);
}
