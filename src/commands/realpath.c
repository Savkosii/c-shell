#include "../api/entry.h"

static int resolve_path(const struct entry *entry, const int option[]) {
    if (!is_entry_located(entry) && option['e'] == 1) {
        log_error("realpath: %s: No such file or directory", entry->received_path);
        return -1;

    } else {
        fprintf(stdout, "%s\n", entry->real_path);
        return 0;
    }
}

static int resolve_paths(char *paths[], size_t path_nums, const int option[]) {
    int retval = 0;
    
    for (size_t i = 0; i < path_nums; i++) {
        struct entry *entry = get_entries_chain(paths[i]);
        retval |= resolve_path(entry, option);
        free_entry(entry);
    }
    
    return retval;
}

static bool try_match_option(const char *arg, int *option_buf) {
    const char *p;
    
    if (*arg == '-' && *(arg + 1) != '\0') {
        if (*(arg + 1) == '-') {
            p = arg + 2;
            if (strcmp(p, "canonicalize-existing") == 0) {
                option_buf['e'] = 1;
                option_buf['m'] = 0;

            } else if (strcmp(p, "canonicalize-missing") == 0) {
                option_buf['m'] = 1;
                option_buf['e'] = 0;

            } else {
                die("realpath: unknown options '--%s'", p);
            }

        } else {
            p = arg + 1;
            while (*p) {
                if (*p == 'e') {
                    option_buf['e'] = 1;
                    option_buf['m'] = 0;

                } else if (*p == 'm') {
                    option_buf['m'] = 1;
                    option_buf['e'] = 0;

                } else {
                    die("realpath: unknown options -- '%c'", *p);
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
        die("realpath: missing operand");
    }

    *paths_nums_buf = paths_nums;
}

int main(int argc, char *argv[]) {
    int option[128];
    char *paths[MAX_SIZE];
    size_t path_nums;
    puts_program_name(argv[0]);
    memset(option, 0, sizeof (option));
    setbuf(stdout, NULL);
    parse(argc, argv, option, paths, &path_nums);
    return resolve_paths(paths, path_nums, option);
}