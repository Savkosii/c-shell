#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "api/entry.h"

static int copy_file(const struct entry *source, const struct entry *destination) {
    char buffer[MAX_LEN];
    int input_stream_fd, output_stream_fd;
    ssize_t nbytes;
    input_stream_fd = open(source->real_path, O_RDONLY);
    output_stream_fd = open(destination->real_path, O_CREAT | O_WRONLY, source->attribute->st_mode);
    while ((nbytes = read(input_stream_fd, buffer, sizeof buffer)) && write(output_stream_fd, buffer, nbytes));
    close(input_stream_fd);
    close(output_stream_fd);
    return 0;
}

static int overwrite_file(const struct entry *source, const struct entry *destination, const int option[]) {
    if (option['i'] == 1) {
        fprintf(stdout, "cp: overwrite '%s'? ", destination->received_path);
        int c = fgetc(stdin);
        fgetc(stdin);
        if (c != 'y') return 0;
    }
    return unlink(destination->real_path) | copy_file(source, destination);
}

static int operate_file_once(const struct entry *file, const struct entry *destination, const int option[]) {
    if (!is_entry_located(destination)) {
        if (!is_directory_write_permitted(destination->previous)) {
            log_error("cp: cannot access '%s': Permission denied", destination->previous->received_path);
            return -1;
        }
        return copy_file(file, destination);

    } else if (!is_file(destination)) {
        log_error("cp: cannot overwrite directory '%s' with non-directory '%s'", destination->received_path, file->received_path);
        return -1;

    } else {
        if (!is_file_write_permitted(destination)) {
            log_error("cp: cannot access '%s': Permission denied", destination->received_path);
            return -1;
        }
        return overwrite_file(file, destination, option);
    }
}

static int copy_empty_directory(const struct entry *source, struct entry *destination) {
    if (!is_directory_write_permitted(destination->previous)) {
        log_error("cp: cannot access '%s': Permission denied", destination->previous->received_path);
        return -1;
    }

    int retval = mkdir(destination->real_path, source->attribute->st_mode);
    destination->attribute = (struct stat*)malloc(sizeof (struct stat));
    stat(destination->real_path, destination->attribute);
    
    return retval;
}

static int copy_directory_recursively(const struct entry *source, struct entry *destination, const int option[]) {
    if (!is_directory_write_permitted(destination->previous)) {
        log_error("cp: cannot access '%s': Permission denied", destination->previous->received_path);
        return -1;
    }
    
    int retval = 0;
    DIR *stream;
    struct dirent *element;

    if ((stream = opendir(source->real_path)) == NULL) {
        return -1;
    }

    while ((element = readdir(stream)) != NULL) {
        if (strcmp(element->d_name, ".") == 0 || strcmp(element->d_name, "..") == 0) {
            continue;
        }

        struct entry *entry, *terminal;
        entry = get_joint_entry(element->d_name, source);
        terminal = get_real_destination(element->d_name, destination);

        if (is_file(entry)) {
            if (!is_file_read_permitted(entry)) {
                log_error("cp: cannot access '%s': Permission denied", entry->received_path);
                retval |= -1;

            } else {
                retval |= operate_file_once(entry, terminal, option);
            }

        } else if (!is_directory_read_permitted(entry)) {
            log_error("cp: cannot access '%s': Permission denied", entry->received_path);
            retval |= -1;

        } else if (is_empty_directory(entry)) {
            if (!is_entry_located(terminal)) {
                retval |= copy_empty_directory(entry, terminal);

            } else if (!is_directory(terminal)) {
                log_error("cp: cannot overwrite non-directory '%s' with directory '%s'", terminal->received_path, entry->received_path);
                retval |= -1;
                break;
            }
            
        } else {
            retval |= copy_directory_recursively(entry, terminal, option);
        }

        free_entry(entry);
        free_entry(terminal);
    }

    closedir(stream);

    return retval;
}

static int operate_directory_once(const struct entry *directory, struct entry *destination, const int option[]) {
    if (!is_entry_located(destination)) {
        if (is_empty_directory(directory)) {
            return copy_empty_directory(directory, destination);

        } else {
            int retval = 0;
            retval |= mkdir(destination->real_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            destination->attribute = (struct stat*)malloc(sizeof (struct stat));
            stat(destination->real_path, destination->attribute);
            retval |= copy_directory_recursively(directory, destination, option);
            retval |= chmod(destination->real_path, directory->attribute->st_mode);
            return retval;
        }

    } else if (!is_directory(destination)) {
        log_error("cp: cannot overwrite non-directory '%s' with directory '%s'", destination->received_path, directory->received_path);
        return -1;

    } else if (!is_empty_directory(destination)) {
        return copy_directory_recursively(directory, destination, option);

    } else {
        return 0;
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
        log_error("cp: '%s' and '%s' are the same file", source->received_path, destination->received_path);
        retval = -1;

    } else if (is_file(source)) {
        if (!is_file_read_permitted(source)) {
            log_error("cp: cannot access '%s': Permission denied", source->received_path);
            retval = -1;

        } else {
            retval = operate_file_once(source, destination, option);
        }

    } else if (is_directory(source)) {
        if (option['r'] == 0) {
            log_error("cp: -r not specified; omitting directory '%s'", source->received_path);
            retval = -1;

        } else if (!is_directory_read_permitted(source)) {
            log_error("cp: cannot access '%s': Permission denied", source->received_path);
            retval = -1;

        } else if (is_subdirectory(destination, source)) {
            log_error("cp: cannot copy a directory, '%s', into itself, '%s'", source->received_path, destination->received_path);
            retval = -1;

        } else {
            retval = operate_directory_once(source, destination, option);
        }
    }

    free_entry(destination);

    return retval;
}

static int operate_entries(char *paths[], size_t paths_nums, const int option[]) {
    int retval = 0;

    struct entry *target;
    target = get_entries_chain(paths[paths_nums - 1]);

    for (size_t i = 0; i < paths_nums - 1; i++) {
        struct entry *entry = get_entries_chain(paths[i]);
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

            } else if (strcmp(p, "recursively") == 0) {
                option_buf['r'] = 1;

            } else {
                die("cp: unknown options '--%s'", p);
            }

        } else {
            p = arg + 1;
            while (*p) {
                if (*p == 'i') {
                    option_buf['i'] = 1;

                } else if (*p == 'r') {
                    option_buf['r'] = 1;

                } else {
                    die("cp: unknown options -- '%c'", *p);
                }
                p++;
            }
        }
        return true;
    } 
    return false;
}

static void parse(int argc, char *argv[], int *option_buf, char *paths_buf[], size_t *paths_nums_buf) {
    size_t paths_nums = 0;

    for (int i = 1; i < argc; i++) {
        if (!try_match_option(argv[i], option_buf)) {
            paths_buf[paths_nums++] = argv[i];
        }
    }

    if (paths_nums == 0) {
        die("cp: missing operand");
    }

    if (paths_nums == 1) {
        die("cp: missing destination file operand after '%s'", paths_buf[0]);
    }

    *paths_nums_buf = paths_nums;
}

int main(int argc, char *argv[], char *envp[]) {
    int option[128];
    char *paths[MAX_SIZE];
    size_t paths_nums;
    puts_program_name(argv[0]);
    memset(option, 0, sizeof(option));
    setbuf(stdout, NULL);
    parse(argc, argv, option, paths, &paths_nums);
    return operate_entries(paths, paths_nums, option);
}