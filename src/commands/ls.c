#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../api/entry.h"

static int entry_priority_compare(const struct dirent **a, const struct dirent **b) {
    const char *p = (*a)->d_name;
    const char *q = (*b)->d_name;

    while ((*p == *q) && *p && *q) {
        p++;
        q++;
    }

    if (*p == '\0' && *q == '\0') {
        return 0;

    } else if (*p < *q) {
        return -1;

    } else {
        return 1;
    }
}

static void load_entry_type(char *char_buffer, mode_t mode_bits) {
    if (S_ISDIR(mode_bits)) *char_buffer = 'd';
    if (S_ISREG(mode_bits)) *char_buffer = '-';
    if (S_ISCHR(mode_bits)) *char_buffer = 'c';
    if (S_ISBLK(mode_bits)) *char_buffer = 'b';
}

static void load_permission_info(char *string_buffer, mode_t mode_bits) {
    char *p = string_buffer;

    while (p <= string_buffer + 8) *p++ = '-';
    *p = '\0';

    if (mode_bits & S_IRUSR) string_buffer[0] = 'r';
    if (mode_bits & S_IWUSR) string_buffer[1] = 'w';
    if (mode_bits & S_IXUSR) string_buffer[2] = 'x';
    if (mode_bits & S_IRGRP) string_buffer[3] = 'r';
    if (mode_bits & S_IWGRP) string_buffer[4] = 'w';
    if (mode_bits & S_IXGRP) string_buffer[5] = 'x';
    if (mode_bits & S_IROTH) string_buffer[6] = 'r';
    if (mode_bits & S_IWOTH) string_buffer[7] = 'w';
    if (mode_bits & S_IXOTH) string_buffer[8] = 'x';
}

static void print_blocks_numbers(struct dirent **entries, struct entry *directory, size_t entry_nums, const int option[]) {
    struct entry *entry;
    off_t block_size = sysconf(_SC_PAGE_SIZE), total_block_nums = 0;

    for (size_t i = 0; i < entry_nums; i++) {
        if (*(entries[i]->d_name) == '.' && option['a'] == 0) {
            continue;
        }

        entry = get_joint_entry(entries[i]->d_name, directory);

        off_t block_nums = (entry->attribute->st_size / block_size) + ((entry->attribute->st_size % block_size) ? 1 : 0);
        
        total_block_nums += block_nums;

        free_entry(entry);
    }

    fprintf(stdout, "total %ld\n", 4 * total_block_nums);
}

static int list_entry_attribute(struct entry *entry) {
    char entry_type, permission_info[10], date[32];
    char *username, *groupname;
    off_t size;
    mode_t mode_bits;
    nlink_t nlink;

    mode_bits = entry->attribute->st_mode;

    load_entry_type(&entry_type, mode_bits);
    load_permission_info(permission_info, mode_bits);

    nlink = entry->attribute->st_nlink;

    username = strdup(getpwuid(entry->attribute->st_uid)->pw_name);
    groupname = strdup(getgrgid(entry->attribute->st_gid)->gr_name);

    size = entry->attribute->st_size;

    strftime(date, 32, "%d-%m-20%y %H:%M", localtime(&entry->attribute->st_ctime));

    fprintf(stdout, "%c%s %lu %s %s %5ld %s ", entry_type, permission_info, nlink, username, groupname, size, date);
    
    free(username);
    free(groupname);

    return 0;
}

static int list_directory_once(struct entry *directory, const int option[]) {
    int retval = 0, entry_nums;
    struct dirent **entries;
    struct entry *entry;

    if ((entry_nums = scandir(directory->real_path, &entries, NULL, entry_priority_compare)) < 0) {
        return -1;
    }

    if (option['l'] == 1) {
        print_blocks_numbers(entries, directory, entry_nums, option);
    }

    for (int i = 0; i < entry_nums; i++) {
        if (*(entries[i]->d_name) == '.' && option['a'] == 0) {
            free(entries[i]);
            continue;
        }

        entry = get_joint_entry(entries[i]->d_name, directory);

        if (option['l'] == 1) {
            retval |= list_entry_attribute(entry);
        }

        if (option['p'] == 1) {
            if (is_directory(entry)) {
                fprintf(stdout, "%s/\n", entries[i]->d_name);
                free_entry(entry);
                continue;
            }
        }

        fprintf(stdout, "%s\n", entry->filename);

        free_entry(entry);

        free(entries[i]);
    }

    free(entries);

    return retval;
}

static int list_file_once(struct entry *file, const int option[]) {
    int retval;
    if (option['l'] == 1) {
        retval = list_entry_attribute(file);
    }
    fprintf(stdout, "%s\n", file->filename);
    return retval;
}

static int list_entry(struct entry *entry, const int option[]) {
    if (!is_entry_located(entry)) {
        log_error("ls: cannot access '%s': No such file or directory", entry->received_path);
        return -1;

    } else if (is_file(entry)) {
        if (!is_file_read_permitted(entry)) {
            log_error("ls: cannot access '%s': Permission denied", entry->received_path);
            return -1;
        }
        return list_file_once(entry, option);

    } else if (is_directory(entry)) {
        if (!is_directory_read_permitted(entry)) {
            log_error("ls: cannot access '%s': Permission denied", entry->received_path);
            return -1;
        }
        return list_directory_once(entry, option);
    }
    return -1;
}

static int list_entries(char *paths[], size_t path_nums, const int option[]) {
    int retval = 0;

    for (size_t i = 0; i < path_nums; i++) {
        struct entry *entry = get_entries_chain(paths[i]);

        if (is_directory(entry) && path_nums > 1) {
            fprintf(stdout, "%s:\n", paths[i]);
        }

        retval |= list_entry(entry, option);

        if (is_directory(entry) && path_nums > 1 && i < path_nums) {
            fputc('\n', stdout);
        }

        free_entry(entry);
    }
   
    return retval;
}

static bool try_match_option(const char *arg, int *option_buffer) {
    const char *p;
    
    if (*arg == '-' && *(arg + 1) != '\0') {
        if (*(arg + 1) == '-') {
            p = arg + 2;
            if (strcmp(p, "all") == 0) {
                option_buffer['a'] = 1;

            } else {
                die("ls: unknown options '--%s'", p);
            }

        } else {
            p = arg + 1;
            while (*p) {
                if (*p == 'a') {
                    option_buffer['a'] = 1;

                } else if (*p == 'l') {
                    option_buffer['l'] = 1;

                } else if (*p == 'p') {
                    option_buffer['p'] = 1;

                } else {
                    die("ls: unknown options -- '%c'", *p);
    
                }
                p++;
            }
        }
        return true;
    } 
    else return false;
}

static void parse(int argc, char *argv[], int *option_buf, char *paths_buf[], size_t *path_nums_buf) {
    size_t path_nums = 0;

    for (int i = 1; i < argc; i++) {
        if (!try_match_option(argv[i], option_buf)) {
            paths_buf[path_nums++] = argv[i];
        }
    }

    if (path_nums == 0) {
        paths_buf[0] = ".";
        path_nums = 1;
    }

    *path_nums_buf = path_nums;
}

int main(int argc, char *argv[]) {
    int option[128];
    char *paths[MAX_SIZE];
    size_t path_nums;
    puts_program_name(argv[0]);
    memset(option, 0, sizeof(option));
    setbuf(stdout, NULL);
    parse(argc, argv, option, paths, &path_nums);
    return list_entries(paths, path_nums, option);
}