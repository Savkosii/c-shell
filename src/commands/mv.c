#include <unistd.h>

#include "../api/entry.h"

static int move_entry(const struct entry *source, const struct entry *destination) {
	return rename(source->real_path, destination->real_path);
}

static int overwrite_entry(const struct entry *source, const struct entry *destination, const int option[]) {
    if (option['i'] == 1) {
        fprintf(stdout, "mv: overwrite '%s'? ", destination->received_path);
        int c = fgetc(stdin);
        fgetc(stdin);
        if (c != 'y') return 0;
    }
    return unlink(destination->real_path) | move_entry(source, destination);
}

static int operate_file_once(const struct entry *file, const struct entry *destination, const int option[]) {
    if (!is_entry_located(destination)) {
        if (!is_directory_write_permitted(destination->previous)) {
            log_error("mv: cannot access '%s': Permission denied", destination->previous->received_path);
            return -1;
        }
        return move_entry(file, destination);

    } else if (!is_file(destination)) {
        log_error("mv: cannot overwrite directory '%s' with non-directory '%s'", destination->received_path, file->received_path);
        return -1;

    } else {
        if (!is_file_write_permitted(destination)) {
            log_error("mv: cannot access '%s': Permission denied", destination->received_path);
            return -1;
        }
        return overwrite_entry(file, destination, option);
    }
}

static int operate_directory_once(const struct entry *directory, struct entry *destination, const int option[]) {
    if (!is_directory_write_permitted(destination->previous)) {
        log_error("mv: cannot access '%s': Permission denied", destination->previous->received_path);
        return -1;
    }

    if (!is_entry_located(destination)) {
        return move_entry(directory, destination);

    } else if (!is_directory(destination)) {
        log_error("mv: cannot overwrite non-directory '%s' with directory '%s'", destination->received_path, directory->received_path);
        return -1;

    } else if (!is_empty_directory(destination)) {
        log_error("mv: cannot move '%s' to '%s': Directory not empty", directory->received_path, destination->received_path);
        return -1;

    } else {
        return overwrite_entry(directory, destination, option);
    }
}

static int operate_entry_once(const struct entry *source, const struct entry *target, const int option[]) {
    if (!is_entry_located(source)) {
        log_error("cp: cannot access '%s': No such a file or directory", source->received_path);
        return -1;
    }

    int retval = -1;
    struct entry *destination;

    if ((destination = get_real_destination(source->filename, target)) == NULL) {
        retval = -1;

    } else if (is_same_entry(source, destination)) {
        log_error("mv: '%s' and '%s' are the same file", source->received_path, destination->received_path);
        retval = -1;

    } else if (is_file(source)) {
        if (!is_file_write_permitted(source)) {
            log_error("mv: cannot access '%s': Permission denied", source->received_path);
            retval = -1;

        } else {
            retval = operate_file_once(source, destination, option);
        }

    } else if (is_directory(source)) {
        struct entry *entry_cwd = get_entries_chain(getcwd(NULL, 0));

        if (!is_directory_write_permitted(source)) {
            log_error("mv: cannot access '%s': Permission denied", source->received_path);
            retval = -1;

        } else if (is_subdirectory(destination, source)) {
            log_error("mv: cannot move a directory, '%s', into itself, '%s'", source->received_path, destination->received_path);
            retval = -1;

        } else if (is_same_entry(entry_cwd, source) || is_subdirectory(entry_cwd, source)) {
            log_error("mv: cannot move '%s': Device or resource busy", source->received_path);
            retval = -1;

        } else {
            retval = operate_directory_once(source, destination, option);
        }

        free_entry(entry_cwd);
    }

    free_entry(destination);

    return retval;
}

static int operate_entries(char *paths[], const int option[], size_t paths_nums) {
    int retval = 0;

    struct entry *target;
    target = get_entries_chain(paths[paths_nums - 1]);

    for (size_t i = 0; i < paths_nums - 1; i++) {
        struct entry *entry;
        entry = get_entries_chain(paths[i]);
        retval |= operate_entry_once(entry, target, option);
        free_entry(entry);
    }

    free_entry(target);

    return retval;
}

static bool try_match_option(const char *arg, int *option_buf) {
    const char *p;
    
    if (*arg == '-' && *(arg + 1) != '\0') {
        if (*(arg + 1) == '-') {
            p = arg + 2;
            if (strcmp(p, "interactive") == 0) {
                option_buf['i'] = 1;
                option_buf['f'] = 0;

            } else if (strcmp(p, "force") == 0) {
                option_buf['f'] = 1;
                option_buf['i'] = 0;

            } else {
                die("mv: unknown options '--%s'", p);
            }

        } else {
            p = arg + 1;
            while (*p) {
                if (*p == 'f') {
                    option_buf['f'] = 1;
                    option_buf['i'] = 0;

                } else if (*p == 'i') {
                    option_buf['i'] = 1;
                    option_buf['f'] = 0;

                } else {
                    die("mv: unknown options -- '%c'", *p);
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

    if (paths_nums < 2) {
        die("mv: missing operand");
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
    return operate_entries(paths, option, paths_nums);
}