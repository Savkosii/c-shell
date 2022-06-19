#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include "../api/entry.h"


static int read_bytes_split_by(int fd, void *buffer, 
                            size_t max_len, int8_t delimiter) {
    int8_t byte_buf;
    size_t count = 0;
    while (read(fd, &byte_buf, sizeof(byte_buf)) != 0 && count < max_len) {
        *(int8_t *)buffer++ = byte_buf;
        count++;
        if (byte_buf == delimiter) {
            break;
        }
    }
    return count;
}


static bool is_whitespace_line(int8_t *line, size_t len) {
    for (int8_t *p = line; p < line + len; p++) {
        if (!isspace(*p)) {
            return false;
        }
    }
    return true;
}


static int show_source(struct entry *file, const int option[]) {
    int fd;
    int8_t line_buf[MAX_LEN];
    int line_number = 1, empty_line_count = 0;

    if (strcmp(file->received_path, "-") == 0) {
        fd = fileno(stdin);

    } else if ((fd = open(file->received_path, O_RDONLY)) == -1) {
        warn("cannot access file '%s'", file->received_path);
        return -1;
    }

    int line_whitespace_flag = false;
    int nbytes = 0;
    while ((nbytes = read_bytes_split_by(fd, line_buf, sizeof(line_buf), '\n')) != 0) {
        if (option['s'] == 1) {
            if (is_whitespace_line(line_buf, nbytes)) {
                if (!line_whitespace_flag) {
                    line_whitespace_flag = true;
                } else {
                    continue;
                }
            } else {
                line_whitespace_flag = false;
            }
        }

        for (size_t i = 0; i < nbytes; i++) {
            if (option['e'] == 1 && line_buf[i] == '\n') {
                fputc('$', stdout);
            }
        
            if (option['t'] == 1 && line_buf[i] == '\t') {
                fputs("^I", stdout);
                continue;
            }

            fputc(line_buf[i], stdout);
        }
    }
    return 0;
}


static int show_sources(char *paths[], size_t paths_nums, const int option[]) {
    int retval = 0;

    for (size_t i = 0; i < paths_nums; i++) {
        struct entry *entry = get_entries_chain(paths[i]);
        retval |= show_source(entry, option);
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
                
            } else if (strcmp(p, "show-ends") == 0) {
                option_buf['e'] = 1;

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

                } else if (*p == 'e' || *p == 'E') {
                    option_buf['e'] = 1;

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
    return false;
}


static void parse_argv(int argc, char *argv[], int *option_buf, 
                    char *paths_buf[], size_t *paths_nums_buf) {
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
    memset(option, 0, sizeof option);
    setbuf(stdout, NULL);
    parse_argv(argc, argv, option, paths, &paths_nums);
    return show_sources(paths, paths_nums, option);
}