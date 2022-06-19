#include <ctype.h>
#include <unistd.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../api/entry.h"


typedef enum MODE_CHANGE_TYPE {
    RESET,
    REMOVE,
    APPEND
} MODE_CHANGE_TYPE;


static int change_file_mode(struct entry *entry, mode_t mode, MODE_CHANGE_TYPE type) {
    if (type == APPEND) {
        entry->attribute->st_mode |= mode;
    } else if (type == REMOVE) {
        entry->attribute->st_mode ^= mode;
    } else {
        entry->attribute->st_mode = mode;
    }
    if (chmod(entry->real_path, entry->attribute->st_mode) == -1) {
        warn("cannot access '%s'", entry->received_path);
        return -1;
    }
    return 0;
}


static int change_files_mode(char *paths[], size_t path_nums, 
                            mode_t mode, MODE_CHANGE_TYPE type) {
    int retval = 0;

    for (size_t i = 0; i < path_nums; i++) {
        struct entry *entry = get_entries_chain(paths[i]);
        retval |= change_file_mode(entry, mode, type);
        free_entry(entry);
    }
    
    return retval;
}


static bool is_rwx_mode(const char *str) {
    if (*str == 0 || strlen(str) > 3) {
        return false;
    }
    for (const char *p = str; *p; p++) {
        if (*p != 'r' && *p != 'w' && *p != 'x') {
            return false;
        }
    }
    return true;
}


static bool is_num_mode(const char *str) {
    if (*str == 0 || strlen(str) > 3) {
        return false;
    }
    for (const char *p = str; *p; p++) {
        if (*p < '0' || *p > '7') {
            return false;
        }
    }
    return true;
}


static mode_t __parse_rwx_mode(const char *mode_str) {
    mode_t mode = 0;
    const mode_t READ_MODE = S_IRUSR | S_IRGRP | S_IROTH;
    const mode_t WRITE_MODE = S_IWUSR | S_IWGRP;
    const mode_t EXEC_MODE = S_IXUSR | S_IXGRP | S_IXOTH; 
    for (const char *p = mode_str; *p; p++) {
        if (*p == 'r') {
            mode |= READ_MODE;
        } else if (*p == 'w') {
            mode |= WRITE_MODE;
        } else {
            mode |= EXEC_MODE;
        }
    }
    return mode;
}


static mode_t __parse_num_mode(const char *mode_str){
    char bits[] = {0, 0, 0};
    mode_t mode = 0;

    for (size_t i = 0; mode_str[i]; i++) {
        bits[3 - i] = mode_str[i] - '0';
    }

    const mode_t READ_MODE[]  = {S_IRUSR, S_IRGRP, S_IROTH};
    const mode_t WRITE_MODE[] = {S_IWUSR, S_IWGRP, S_IWOTH};
    const mode_t EXEC_MODE[] = {S_IXUSR, S_IXGRP, S_IXOTH};

    for (size_t i = 0; i < 3; i++) {
        char bit = bits[i];
        if (bit / 4 > 0) {
            mode |= READ_MODE[i];
            bit %= 4;
        }
        if (bit / 2 > 0) {
            mode |= WRITE_MODE[i];
            bit %= 2;
        }
        if (bit / 1 > 0) {
            mode |= EXEC_MODE[i];
        }
    }

    return mode;
}


static mode_t __parse_mode(const char *mode_str) {
    mode_t mode = 0;
    if (is_rwx_mode(mode_str)) {
        mode = __parse_rwx_mode(mode_str);
    } 
    if (is_num_mode(mode_str)) {
        mode = __parse_num_mode(mode_str);
    }
    return mode;
}


static bool is_valid_mode(const char *mode_str) {
    return is_rwx_mode(mode_str) || is_num_mode(mode_str) || *mode_str == 0;
}


static bool match_mode(const char *arg, mode_t *mode_buf, MODE_CHANGE_TYPE *type) {
    const char *p = arg;

    if (*arg != '-' && *arg != '+') {
        return false;
    }

    if (strncmp(p, "-u=", 3) == 0) {
        p += 3;
        *type = RESET;
    } else if (strncmp(p, "-", 1) == 0) {
        p += 1;
        *type = REMOVE;
    } else if (strncmp(p, "+", 1) == 0) {
        p += 1;
        *type = APPEND;
    } 
    
    if (p == arg) {
        return false;
    }

    if (*p == 0) {
        *mode_buf = 0;
        return true;
    }

    if (!is_valid_mode(p)) {
        die("chmod: invalid mode '%s'", p);
    }

    *mode_buf = __parse_mode(p);

    return true;
}


static void parse_argv(int argc, char *argv[], 
                        mode_t *mode_buf, MODE_CHANGE_TYPE *type, 
                        char *paths_buf[], size_t *path_nums_buf) {
    size_t paths_nums = 0;

    if (argc < 3) {
        die("chmod: missing operand");
    }

    for (int i = 1; i < argc; i++) {
        if (!match_mode(argv[i], mode_buf, type)) {
            paths_buf[paths_nums++] = argv[i];
        }
    }

    if (paths_nums == 0) {
        die("chmod: missing operand");
    }

    *path_nums_buf = paths_nums;
}


int main (int argc, char *argv[]){
    char *paths[MAX_SIZE];
    size_t path_nums;
    mode_t mode_bits;
    MODE_CHANGE_TYPE type;
    parse_argv(argc, argv, &mode_bits, &type, paths, &path_nums);
    return change_files_mode(paths, path_nums, mode_bits, type);
}