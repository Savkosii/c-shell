#include <unistd.h>

#include "api/entry.h"

static void load_string_builder(char *string_builder_buf, const char *source, const int option[], int *line_number_buf) {
    if (option['n'] == 1 || (option['b'] == 1 && *source != '\n')) {
        if (snprintf(string_builder_buf, MAX_LEN, "%6d  %s", *line_number_buf, source) > MAX_LEN) {
            die("cat: Error: buffer overflowed");
        }
        *line_number_buf += 1;

    } else if (snprintf(string_builder_buf, MAX_LEN, "%s", source) > MAX_LEN) {
        die("cat: Error: buffer overflowed");
    }
}

static void replace_end_of_line_sign(char *string_builder) {
    size_t offset = strlen(string_builder);
    if (offset + 1 > MAX_LEN) {
        die("cat: Error: buffer overflowed");
    }
    sprintf(string_builder + offset - 1, "$\n");
}

static void replace_tab_char(char *string_builder) {
    char *p, *q;
    size_t offset;
    int tab_count = 0;

    for (p = string_builder; *p; p++) {
        if (*p == '\t') tab_count++;
    }

    offset = strlen(string_builder);

    if (tab_count > 0) {
        if (tab_count + offset > MAX_LEN) {
            die("cat: Error: buffer overflowed");
        }

        p = string_builder + offset - 1;
        q = string_builder + offset + tab_count;

        for (*q-- = '\0'; p >= string_builder; p--) {
            if (*p != '\t') {
                *q-- = *p;
            }
            else {
                *q-- = 'I';
                *q-- = '^';
            }
        }
    }
}

static int read_file_once(struct entry *file, const int option[]) {
    FILE *stream;
    char read_buf[MAX_LEN], string_builder[MAX_LEN];
    int line_number = 1, empty_line_count = 0;

    if (strcmp(file->received_path, "-") == 0) {
        stream = stdin;

    } else if (!is_entry_located(file)) {
        log_error("cat: %s: no such a file or directory", file->received_path);
        return -1;

    } else if (is_directory(file)) {
        log_error("cat: %s: is a directory", file->received_path);
        return -1;

    } else if (!is_file_read_permitted(file)) {
        log_error("cat: cannot open '%s': Permission denied", file->received_path);
        return -1;

    } else if ((stream = fopen(file->received_path, "r")) == NULL) {
        log_error("cat: cannot open '%s'", file->received_path);
        return -1;
    }

    while (fgets(read_buf, MAX_LEN, stream) != NULL) {
        if (option['s'] == 1) {
            if (*read_buf != '\n') {
                empty_line_count = 0;
            }
            else {
                empty_line_count++;
                if (empty_line_count > 1) continue;
            }
        }

        load_string_builder(string_builder, read_buf, option, &line_number);

        if (option['e'] == 1) {
            replace_end_of_line_sign(string_builder);
        }

        if (option['t'] == 1) {
            replace_tab_char(string_builder);
        }

        fputs(string_builder, stdout);
    }

    return 0;
}

static int read_files(char *paths[], size_t paths_nums, const int option[]) {
    int retval = 0;

    for (size_t i = 0; i < paths_nums; i++) {
        struct entry *entry = get_entries_chain(paths[i]);
        retval |= read_file_once(entry, option);
        if (i < paths_nums - 1) fputc('\n', stdout);
        free_entry(entry);
    }

    return retval;
}

static bool try_match_option(const char *arg, int *option_buf) {
    const char *p;

    if (*arg == '-' && *(arg + 1) != '\0') {
        if (*(arg + 1) == '-') {
            p = arg + 2;
            if (strcmp(p, "show-all") == 0) {
                option_buf['t'] = 1;
                option_buf['e'] = 1;
                
            } else if (strcmp(p, "number-nonblank") == 0) {
                option_buf['b'] = 1;

            } else if (strcmp(p, "show-ends") == 0) {
                option_buf['e'] = 1;

            } else if (strcmp(p, "number") == 0) {
                option_buf['n'] = 1;

            } else if (strcmp(p, "squeeze-blank") == 0) {
                option_buf['s'] = 1;

            } else if (strcmp(p, "show-tabs") == 0) {
                option_buf['t'] = 1;

            } else {
                die("cat: unknown options '--%s'", p);
            }
            
        } else {
            p = arg + 1;
            while (*p) {
                if (*p == 'A') {
                    option_buf['t'] = 1;
                    option_buf['e'] = 1;

                } else if (*p == 'b') {
                    option_buf['b'] = 1;
                    option_buf['n'] = 0;

                } else if (*p == 'e' || *p == 'E') {
                    option_buf['e'] = 1;

                } else if (*p == 'n') {
                    option_buf['n'] = 1;
                    option_buf['b'] = 0;

                } else if (*p == 's') {
                    option_buf['s'] = 1;

                } else if (*p == 't' || *p == 'T') {
                    option_buf['t'] = 1;
                    
                } else {
                    die("cat: unknown options -- '%c'", *p);
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
        paths_buf[0] = "-";
        paths_nums = 1;
    }

    *paths_nums_buf = paths_nums;
}

int main(int argc, char *argv[], char *envp[]) {
    int option[128];
    char *paths[MAX_SIZE];
    size_t paths_nums;
    puts_program_name(argv[0]);
    memset(option, 0, sizeof option);
    setbuf(stdout, NULL);
    parse(argc, argv, option, paths, &paths_nums);
    return read_files(paths, paths_nums, option);
}