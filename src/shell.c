#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <glob.h>

#include "api/entry.h"

#define BEGIN_WITH_DELIMITER 0100

#define DELIMITER_CONCAT 0200

#define EMPTY_BETWEEN_DEMILITER 0400

static const char *sys_home_directory;

static const char *app_home_directory;


/*   This function reads from stdin, omitting space char before
 *   reading the first non-space char and after reading the last one.
 *
 *   When encountering '\n' or EOF, it will stop input, and return the nums of char writen,
 *   including '\0', in the former case, and EOF in the latter case.
 */
static int read_command(char *buffer, size_t buffer_max_size) {
    int ch, count = 0;

    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (isspace(ch) && count == 0) {
            continue;

        } else if ((count += 1) < buffer_max_size)  {
            *buffer++ = ch;
        }
    }

    if (count == 0 && ch == EOF) {
        return EOF;
    }
    
    buffer -= 1;

    while (isspace(*buffer)) {
        buffer--;
        count--;
    }

    *(buffer + 1) = '\0';
    count += 1;
    return count;
}

/* similar to read_command(), but does not omit space char */
static int read_line(char *buffer, size_t buffer_max_size) {
    int ch, count = 0;

    while ((ch = fgetc(stdin)) != EOF) {
        if ((count += 1) < buffer_max_size)  {
            *buffer++ = ch;
        }
        if (ch == '\n') break;
    }

    if (count == 0 && ch == EOF) return EOF;

    *buffer = '\0';
    count += 1;
    return count;
}

static void print_prompt() {
    const char *cwd, *username;
    char hostname[MAX_LEN], ch;
    struct passwd *pwd;
    size_t path_len;

    cwd = getcwd(NULL, 0);
    pwd = getpwuid(getuid());
    sys_home_directory = pwd->pw_dir;
    username = pwd->pw_name;
    gethostname(hostname, sizeof(hostname));

    if (strcmp(pwd->pw_name, "root") != 0) {
        ch = '$';

    } else {
        ch = '#';
    }

    path_len = strlen(sys_home_directory);

    if (strncmp(cwd, sys_home_directory, path_len) == 0) {
        printf("%s@%s:~%s%c ", username, hostname, cwd + path_len, ch);

    } else {
        printf("%s@%s:%s%c ", username, hostname, cwd, ch);
    }
}

static void try_unfold_path(const char *path, char *path_buf) {
    if (*path == '~') {
        char *sys_home = strdup(getpwuid(getuid())->pw_dir);
        if (*(path + 1) == '\0') {
            if (snprintf(path_buf, MAX_LEN, "%s", sys_home) > MAX_LEN) {
                die("shell: Error: buffer overflowed");
            }

        } else if (*(path + 1) != '/') {
            if (snprintf(path_buf, MAX_LEN, "/home/%s", path + 1) > MAX_LEN) {
                die("shell: Error: buffer overflowed");
            }

        } else {
            if (snprintf(path_buf, MAX_LEN, "%s%s", sys_home, path + 1) > MAX_LEN) {
                die("shell: Error: buffer overflowed");
            }
        }

        free(sys_home);

    } else {
        if (snprintf(path_buf, MAX_LEN, "%s", path) > MAX_LEN) {
            die("shell: Error: buffer overflowed");
        }
    }
}

/*  Try wildcard at the same time  */
static void load_paths(const char *path, char *paths_buf[], int *path_nums_buf) {
    char unfolded_path[MAX_LEN];
    glob_t res_paths;
    res_paths.gl_pathc = 0;
    res_paths.gl_pathv = NULL;
    res_paths.gl_offs = 0;

    try_unfold_path(path, unfolded_path);

    if (glob(unfolded_path, GLOB_NOCHECK, NULL, &res_paths) == 0) {
        for (int i = 0; i < res_paths.gl_pathc; i++) {
            paths_buf[*path_nums_buf] = strdup(res_paths.gl_pathv[i]);
            if ((*path_nums_buf += 1) > MAX_SIZE) {
                die("shell: Error: buffer overflowed");
            }
        }
    }

    globfree(&res_paths);
}

/* the memory is allocated by malloc, and thus needs to be freed manually */
static void load_argv(char *command, char *argv[], int *argc_buf) {
    *argc_buf = 0;
    for (char *token, *p = strtok_r(command, " ", &token); p; p = strtok_r(NULL, " ", &token)) {
        if (*p == '-') {
            argv[*argc_buf] = strdup(p);
            if ((*argc_buf += 1) > MAX_SIZE) {
                die("shell: Error: buffer overflowed");
            }

        } else {
            load_paths(p, argv, argc_buf);
        }
    }
    argv[*argc_buf] = NULL;
}

static void free_paths(char *paths[], size_t path_nums) {
    for (size_t i = 0; i < path_nums; i++) {
        free(paths[i]);
    }
}

static bool catch_delimiter_error(const char *line, const char *delimiter, int error_type) {
    size_t offset = strlen(delimiter);
    while (isspace(*line)) line++;
    const char *p = line, *t = delimiter + offset - 1;
    while ((p = strstr(p, delimiter)) != NULL) {
        const char *q = p - 1;
        if (q < line && (error_type & BEGIN_WITH_DELIMITER)) {
            log_error("shell: syntax error near unexpected token '%s'", delimiter);
            return true;
        }
        
        if (error_type & EMPTY_BETWEEN_DEMILITER) {
            while (isspace(*q)) q--;
        }

        while (t >= delimiter && q >= line && *t == *q) t--, q--;
        if (t < delimiter && (error_type & DELIMITER_CONCAT)) {
            log_error("shell: syntax error near unexpected token '%s'", delimiter);
            return true;
        }
        p += offset;
    }
    return false;
}

static bool catch_syntax_error(const char *line) {
    if (catch_delimiter_error(line, ";",  BEGIN_WITH_DELIMITER | DELIMITER_CONCAT | EMPTY_BETWEEN_DEMILITER) || \
        catch_delimiter_error(line, "&&", BEGIN_WITH_DELIMITER | DELIMITER_CONCAT | EMPTY_BETWEEN_DEMILITER) || \
        catch_delimiter_error(line, "|",  BEGIN_WITH_DELIMITER | DELIMITER_CONCAT | EMPTY_BETWEEN_DEMILITER) || \
        catch_delimiter_error(line, "<<", DELIMITER_CONCAT | EMPTY_BETWEEN_DEMILITER)                        || \
        catch_delimiter_error(line, ">",  EMPTY_BETWEEN_DEMILITER)                                           || \
        catch_delimiter_error(line, ">>", DELIMITER_CONCAT | EMPTY_BETWEEN_DEMILITER)) {
        return true;
    }
    return false;
}

static bool is_end_with_delimiter(const char *string, const char *delimiter) {
    const char *p = string + strlen(string) - 1, *q = delimiter + strlen(delimiter) - 1;
    while (isspace(*p)) p--;
    while (*p == *q) {
        p--, q--;
        if (q < delimiter) return true;
    }
    return false;
}

static int load_full_command(char *line) {
    while (is_end_with_delimiter(line, "&&") || is_end_with_delimiter(line, "|") || \
           is_end_with_delimiter(line, "<<") || is_end_with_delimiter(line, ">") || \
           is_end_with_delimiter(line, ">>")) {
        size_t offset = strlen(line);
        fputs("> ", stdout);
        if (offset > MAX_LEN) {
            die("shell: Error: buffer overflowed");
        }
        int count = 0;
        if (read_command(line + offset, MAX_LEN - offset) == EOF) {
            return -1;
        }
    }
    return 0;
}

/* Don't call this function unless in child process */
static void read_multiple_lines(char *command) {
    int pipe_fd[2];
    char read_buf[MAX_LEN], *p = command, *s = read_buf;

    while ((p = strstr(p, "<<")) != NULL) {
        char end_input_flag[MAX_LEN], line[MAX_LEN], *t = end_input_flag;
        size_t buf_len = 0;
        int nbytes;
        memcpy(p, "  ", 2);
        p += 2;

        while (isspace(*p)) p++;

        for (char *q = p; *q && !isspace(*q); t++, q++) {
            *t = *q;
            *q = ' ';
        }

        memcpy(t, "\n", 2);

        fputs("> ", stdout);
        while ((nbytes = read_line(line, MAX_LEN)) != EOF && strcmp(line, end_input_flag) != 0) {
            if ((buf_len += nbytes - 1) > MAX_LEN) {
                die("shell: Error: buffer overflowed");
            }
            snprintf(s, MAX_LEN, "%s", line);
            s += nbytes - 1;
            fputs("> ", stdout);
        }
    }

    pipe(pipe_fd);
    write(pipe_fd[1], read_buf, strlen(read_buf));
    close(pipe_fd[1]);
    dup2(pipe_fd[0], fileno(stdin));
}

/* Don't call this function unless in child process */
static void redirect_overwrite_fstream(char *command) {
    char path[MAX_LEN], *p = command, *t;
    while ((p = strstr(p, ">")) != NULL) {
        *p++ = ' ';
        while (isspace(*p)) p++;
        t = path;
        for (char *q = p; *q && !isspace(*q); t++, q++) {
            *t = *q;
            *q = ' ';
        }
        *t = '\0';
    }

    char *matched_paths[MAX_LEN];
    int matched_path_nums;
    load_paths(path, matched_paths, &matched_path_nums);
    if (matched_path_nums > 1) {
        die("%s: ambiguous redirect", path);
    }

    struct entry *entry = get_entries_chain(matched_paths[0]);
    if (is_directory(entry)) {
        die("shell: Error: cannot overwrite directory '%s'", entry->received_path);
    }
    if (!is_directory_write_permitted(entry->previous)) {
        die("shell: cannot create '%s' : Permission denied", entry->received_path);
    }
    if (is_file(entry) && !is_file_write_permitted(entry)) {
        die("shell: cannot overwrite '%s' : Permission denied", entry->received_path);
    }

    int file_mode_bit = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    int fstream_fd = open(matched_paths[0], O_RDWR | O_CREAT | O_TRUNC, file_mode_bit);
    dup2(fstream_fd, fileno(stdout));

    free(matched_paths[0]);
    free_entry(entry);
}

/* Don't call this function unless in child process */
static void redirect_append_fstream(char *command) {
    char path[MAX_LEN], *p = command, *t;
    while ((p = strstr(p, ">>")) != NULL) {
        memcpy(p, "  ", 2);
        p += 2;
        while (isspace(*p)) p++;
        t = path;
        for (char *q = p; *q && !isspace(*q); t++, q++) {
            *t = *q;
            *q = ' ';
        }
        *t = '\0';
    }

    char *matched_paths[MAX_LEN];
    int matched_path_nums;
    load_paths(path, matched_paths, &matched_path_nums);
    if (matched_path_nums > 1) {
        die("%s: ambiguous redirect", path);
    }

    struct entry *entry = get_entries_chain(matched_paths[0]);
    if (is_directory(entry)) {
        die("shell: Error: cannot overwrite directory '%s'", entry->received_path);
    }
    if (!is_directory_write_permitted(entry->previous)) {
        die("shell: cannot create '%s' : Permission denied", entry->received_path);
    }
    if (is_file(entry) && !is_file_write_permitted(entry)) {
        die("shell: cannot overwrite '%s' : Permission denied", entry->received_path);
    }

    int file_mode_bit = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    int fstream_fd = open(matched_paths[0], O_RDWR | O_CREAT | O_APPEND, file_mode_bit);
    dup2(fstream_fd, fileno(stdout));

    free(matched_paths[0]);
    free_entry(entry);
}

/* This function actually serves like strtok_r, but it regards the whole string as one delimiter, 
 * rather than a set of char delimiters.
 *
 * What's more, the empty sequences before and after the return string value will be removed.
 * 
 * If the function has to return an empty string, it will return NULL instead.
*/
static char * strtok_l(char *string, const char *delimiter, char **save_ptr) {
    char *p, *t, *retval;
    if (string != NULL) {
        p = string;
    } 

    else {
        if (*save_ptr == NULL) return NULL;
        p = *save_ptr;
    }

    while (isspace(*p)) p++;
    retval = p;

    if ((t = strstr(p, delimiter)) == NULL) {
        if (*save_ptr != NULL) *save_ptr = NULL;
        return retval;
    }

    if (t == p) return NULL;
    p = t;
    t--;

    while (isspace(*t)) *t-- = '\0';
    for (const char *q = delimiter; *q; q++) *p++ = '\0';
    *save_ptr = p;

    return retval;
}

static int load_commands(char *line, char **commands_buf, size_t *command_nums_buf) {
    if (catch_syntax_error(line)) return -1;
    if (load_full_command(line) == -1) return -1;
    size_t command_nums = 0;

    for (char *token, *p = strtok_r(line, ";", &token); p; p = strtok_r(NULL, ";", &token)) {
        for (char *token, *q = strtok_l(p, "&&", &token); q; q = strtok_l(NULL, "&&", &token)) {
            commands_buf[command_nums++] = q;
            if (command_nums > MAX_SIZE) {
                die("shell: Error: buffer overflowed");
            }
        }
    }

    commands_buf[command_nums] = NULL;
    *command_nums_buf = command_nums;

    return 0;
}

static int locate_application_path(char **arg_buf) {
    int retval = 0;
    struct entry *entry;
    if (!strchr(*arg_buf, '/')) {
        struct entry *app_home_entry = get_entries_chain(app_home_directory);
        entry = get_joint_entry(*arg_buf, app_home_entry);
        free_entry(app_home_entry);
        if (!is_file(entry)) {
            log_error("%s: command not found", *arg_buf);
            retval = -1;
        }

    } else {
        entry = get_entries_chain(*arg_buf);
        if (!is_entry_located(entry)) {
            log_error("%s: No such file or directory", *arg_buf);
            retval = -1;

        } else if (is_directory(entry)) {
            log_error("%s: Is a directory", *arg_buf);
            retval = -1;
        }
    }

    if (retval != -1) {
        if (!is_file_execute_permitted(entry)) {
            log_error("shell: Error: cannot execute command '%s': Permission denied", *arg_buf);
            retval = -1;

        } else {
            free(*arg_buf);
            *arg_buf = strdup(entry->received_path);
        }
    }

    free_entry(entry);

    return retval;
}

static int cd(size_t argc, char *argv[]) {
    if (argc > 2) {
        free_paths(argv, argc);
        log_error("cd: too many arguments");
        return -1;
    }

    int retval;
    const char *path = argv[1];
    if (path == NULL || *path == '\0') {
        path = sys_home_directory;
    }

    struct entry *entry = get_entries_chain(path);
    if (!is_entry_located(entry)) {
        log_error("cd: %s: No such a directory", entry->received_path);
        retval = -1;

    } else if (!is_directory(entry)) {
        log_error("cd: '%s': Not a directory", entry->received_path);
        retval = -1;

    } else if (!is_directory_read_permitted(entry)) {
        log_error("cd: cannot access '%s': Permission denied", entry->received_path);
        retval = -1;

    } else {
        retval = chdir(entry->received_path);
    }

    free(argv[0]);
    free_entry(entry);

    return retval;
}


/* This function does not call fork() before execution, and thus has to call it manually */
static void exec_once(char *command) {
    int argc;
    char *argv[MAX_SIZE];
    if (strstr(command, "<<")) {
        read_multiple_lines(command);
    }

    if (strstr(command, ">>")) {
        redirect_append_fstream(command);
            

    } else if (strstr(command, ">")) {
        redirect_overwrite_fstream(command);
    }
    
    load_argv(command, argv, &argc);

    if (locate_application_path(&argv[0]) == -1) {
        free_paths(argv,argc);
        exit(1);
    }

    execv(argv[0], argv);
}

static int exec_normal_command(char *command) {
    if (strncmp(command, "cd", 2) == 0) {
        char *argv[MAX_SIZE];
        int argc;
        load_argv(command, argv, &argc);
        return cd(argc, argv);

    } else {
        if (fork() == 0) {
            exec_once(command);
        }
    }

    return 0;
}


static void exec_commands_with_pipes(char *line) {
    char *commands[MAX_SIZE];
    size_t command_nums = 0;
    for (char *token, *p = strtok_l(line, "|", &token); p; p = strtok_l(NULL, "|", &token)) {
        commands[command_nums++] = p;
    }
    int pipes[command_nums - 1][2];

    for (size_t i = 0; i < command_nums; i++) {
        if (i == 0) {
            pipe(pipes[i]);
            if (fork() == 0) {
                close(pipes[i][0]);
                dup2(pipes[i][1], fileno(stdout));
                exec_once(commands[i]);
            }
            close(pipes[i][1]);

        } else if (i < command_nums - 1) {
            pipe(pipes[i]);
            if (fork() == 0) {
                dup2(pipes[i-1][0], fileno(stdin));
                dup2(pipes[i][1], fileno(stdout));
                exec_once(commands[i]);
            }
            close(pipes[i-1][0]);
            close(pipes[i][1]);

        } else {
            if (fork() == 0) {
                dup2(pipes[i-1][0], fileno(stdin));
                exec_once(commands[i]);
            }
            close(pipes[i-1][0]);
        }
    }
}

static int exec(char *line) {
    char *commands[MAX_SIZE];
    size_t command_nums;
    if (load_commands(line, commands, &command_nums) == -1) {
        return -1;
    }

    for (size_t i = 0; i < command_nums; i++) {
            if (!strchr(commands[i], '|')) {
                exec_normal_command(commands[i]);
            } else {
                exec_commands_with_pipes(commands[i]);
            }
        while (wait(NULL) != -1) ;
    }

    return 0;
}

int main(){
    char line[MAX_LEN];
    int nbytes;
    app_home_directory = getcwd(NULL, 0);
    setbuf(stdout, NULL);
    print_prompt();
    while ((nbytes = read_command(line, MAX_LEN)) != EOF) {
        if ((nbytes > MAX_LEN)) {
            die("shell: Error: buffer overflowed");
        }
        exec(line);
        print_prompt();
    }
    return 0;
}


