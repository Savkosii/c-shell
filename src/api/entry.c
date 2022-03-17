#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "limits.h"
#include "bool.h"
#include "error.h"
#include "name.h"

const char *program_name;

typedef struct entry {
    char *filename;
    char *received_path;
    char *real_path;
    struct stat *attribute;
    struct entry *previous;
    struct entry *next;
} entry;

static void load_full_path(const char *path, char *path_buf) {
    if (*path != '/') {
        if (snprintf(path_buf, MAX_LEN, "%s/%s", getcwd(NULL, 0), path) > MAX_LEN) {
            die("%s: Error: buffer overflowed", program_name);
        }

    } else {
        if (snprintf(path_buf, MAX_LEN, "%s", path) > MAX_LEN) {
            die("%s: Error: buffer overflowed", program_name);
        }
    }
}

static void initialize(struct entry *entry) {
    entry->filename = NULL;
    entry->received_path = NULL;
    entry->real_path = NULL;
    entry->attribute = NULL;
    entry->next = NULL;
    entry->previous = NULL;
}

static void load_entries_struct(char *path, entry **head_buf, entry **tail_buf) {
    char *p, *token;
    entry *head, *tail, *t;
    entry **q;
    entry *entries[MAX_SIZE];
    size_t n = 0;

    head = (entry *)malloc(sizeof (entry));
    initialize(head);
    head->real_path = strdup("/");
    head->filename = strdup("/");
    head->attribute = (struct stat *)malloc(sizeof (struct stat));
    stat("/", head->attribute);

    if ((p = strtok_r(path, "/", &token)) == NULL) { 
        head->next = NULL;
        head->previous = NULL;
        *head_buf = head;
        *tail_buf = head;
        return;
    }

    q = &(head->next);

    while (p != NULL) {
        *q = (entry *)malloc(sizeof (entry));
        initialize(*q);
        (*q)->filename = strdup(p);
        tail = *q;
        q = &(tail->next);
        p = strtok_r(NULL, "/", &token);
    }

    for (t = head; t != NULL; t = t->next) {
        if (n > MAX_SIZE) {
            die("%s: Error: buffer overflowed", program_name);
        }
        entries[n++] = t;
    }

    for (size_t i = 1; i < n - 1; i++) {
        entries[i]->previous = entries[i-1];
    }

    head->previous = NULL;
    tail->previous = entries[n-2];
    free(tail->next);
    tail->next = NULL;

    *head_buf = head;
    *tail_buf = tail;
}

static void free_entry_part(entry *element) {
    if (element->filename != NULL)  free(element->filename);
    if (element->received_path != NULL)  free(element->received_path);
    if (element->real_path != NULL)  free(element->real_path);
    if (element->attribute != NULL)  free(element->attribute);
    free(element);
}

static void remove_entries_element(entry *element) {
    if (element->next == NULL) {
        element->previous->next = NULL;

    } else {
        element->previous->next = element->next;
        element->next->previous = element->previous;
    }

    free_entry_part(element);
}

static void handle_entries_struct(entry **head, entry **tail) {
    int need_remove_count = 0;
    entry *p;

    for (p = *tail; p != *head; p = p->previous) {
        if (strcmp(p->filename, ".") == 0) {
            if ((*tail) == p) {
                *tail = p->previous;
            }
            remove_entries_element(p);

        } else if (strcmp(p->filename, "..") == 0) {
            need_remove_count++;
            if ((*tail) == p) {
                *tail = p->previous;
            }
            remove_entries_element(p);

        } else {
            if (need_remove_count > 0) {
                if ((*tail) == p) {
                    *tail = p->previous;
                }
                remove_entries_element(p);
                need_remove_count--;
            }
        }
    }
}

/* the memory is allocated by malloc, and thus needs to be freed */
extern entry * get_entries_chain(const char *path) {
    char buffer[MAX_LEN], path_buf[MAX_LEN], *p = path_buf;
    entry *head, *tail;
    size_t path_len = 0, offset;
    
    load_full_path(path, buffer);
    load_entries_struct(buffer, &head, &tail);
    handle_entries_struct(&head, &tail);
    tail->received_path = strdup(path);
    entry *q = head->next;

    while (q != NULL) {
        offset = snprintf(p, MAX_LEN, "/%s", q->filename);

        if ((path_len += offset) > MAX_LEN) {
            die("%s: Error: buffer overflowed", program_name);
        }

        p += offset;

        q->real_path = (char *)malloc(MAX_LEN * sizeof (char));
        q->attribute = (struct stat *)malloc(sizeof (struct stat));

        if (snprintf(q->real_path, MAX_LEN, "%s", path_buf) > MAX_LEN) {
            die("%s: Error: buffer overflowed", program_name);
        }

        if (stat(q->real_path, q->attribute) == -1) {
            free(q->attribute);
            q->attribute = NULL;
        }

        q = q->next;
    }
    return tail;
}

extern void free_entry(struct entry *entry) {
    struct entry *p = entry;
    while (p != NULL) {
        free_entry_part(p);
        p = p->previous;
    }
}

extern bool is_entry_located(const struct entry *entry) {
    if (entry->attribute == NULL) return false;
    else return true;
}

extern bool is_file(const struct entry *entry) {
    if (!is_entry_located(entry)) return false;
    else return S_ISREG(entry->attribute->st_mode);
}

extern bool is_directory(const struct entry *entry) {
    if (!is_entry_located(entry)) return false;
    else return S_ISDIR(entry->attribute->st_mode);
}

extern bool is_empty_directory(const struct entry *entry) {
    if (!is_entry_located(entry) || !is_directory(entry)) return false;
    DIR *stream = opendir(entry->real_path);
    struct dirent *element;
    const char *filename;

    while ((element = readdir(stream)) != NULL) {
        filename = element->d_name;
        if (strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) return false;
    }

    closedir(stream);
    return true;
}

extern bool is_same_entry(const struct entry *entry_A, const struct entry *entry_B) {
    if (strcmp(entry_A->real_path, entry_B->real_path) == 0) return true;
    else return false;
}

// if true, the former is the subdirectory of the latter
extern bool is_subdirectory(const struct entry *entry_A, const struct entry *entry_B) {
    const entry *p = entry_A, *q = entry_B;
    while (true) {
        if (q == NULL) return true;
        else if (p == NULL || strcmp(q->filename, p->filename) != 0) return false;
        p = p->previous;
        q = q->previous;
    }
}

extern bool is_directory_read_permitted(const struct entry *entry) {
    if (entry == NULL) return true;
    int retval;
    char *file_owner_username, *file_owner_groupname;
    char *current_username, *current_groupname;
    mode_t mode_bits = entry->attribute->st_mode;

    file_owner_username = strdup(getpwuid(entry->attribute->st_uid)->pw_name);
    file_owner_groupname = strdup(getgrgid(entry->attribute->st_gid)->gr_name);
    current_username = strdup(getpwuid(getuid())->pw_name);
    current_groupname = strdup(getgrgid(getgid())->gr_name);

    if (strcmp(current_username, "root") == 0) {
        return true;
    }

    if (strcmp(file_owner_username, current_username) == 0) {
        if ((mode_bits & S_IRUSR) && (mode_bits & S_IXUSR)) retval = true;
        else retval = false;

    } else if (strcmp(file_owner_username, current_groupname) == 0) {
        if ((mode_bits & S_IRGRP) && (mode_bits & S_IXGRP)) retval = true;
        else retval = false;

    } else {
        if ((mode_bits & S_IROTH) && (mode_bits & S_IXOTH)) retval = true;
        else retval = false;
    }

    free(file_owner_username);
    free(file_owner_groupname);
    free(current_username);
    free(current_groupname);

    retval &= is_directory_read_permitted(entry->previous);
    return retval;
}

extern bool is_directory_write_permitted(const struct entry *entry) {
    if (!is_directory_read_permitted(entry))  return false;
    int retval;
    char *file_owner_username, *file_owner_groupname;
    char *current_username, *current_groupname;
    mode_t mode_bits = entry->attribute->st_mode;

    file_owner_username = strdup(getpwuid(entry->attribute->st_uid)->pw_name);
    file_owner_groupname = strdup(getgrgid(entry->attribute->st_gid)->gr_name);
    current_username = strdup(getpwuid(getuid())->pw_name);
    current_groupname = strdup(getgrgid(getgid())->gr_name);

    if (strcmp(current_username, "root") == 0) {
        return true;
    }

    if (strcmp(file_owner_username, current_username) == 0) {
        if (mode_bits & S_IWUSR)  retval = true;
        else retval = false;

    } else if (strcmp(file_owner_username, current_groupname) == 0) {
        if (mode_bits & S_IWGRP)  retval = true;
        else retval = false;

    } else {
        if (mode_bits & S_IWOTH)  retval = true;
        else retval = false;
    }

    free(file_owner_username);
    free(file_owner_groupname);
    free(current_username);
    free(current_groupname);

    return retval;
}

extern bool is_file_read_permitted(const struct entry *entry) {
    if (!is_directory_read_permitted(entry->previous)) return false;
    int retval;
    char *file_owner_username, *file_owner_groupname;
    char *current_username, *current_groupname;
    mode_t mode_bits = entry->attribute->st_mode;

    file_owner_username = strdup(getpwuid(entry->attribute->st_uid)->pw_name);
    file_owner_groupname = strdup(getgrgid(entry->attribute->st_gid)->gr_name);
    current_username = strdup(getpwuid(getuid())->pw_name);
    current_groupname = strdup(getgrgid(getgid())->gr_name);

    if (strcmp(current_username, "root") == 0) {
        return true;
    }

    if (strcmp(file_owner_username, current_username) == 0) {
        if (mode_bits & S_IRUSR)  retval = true;
        else retval = false;

    } else if (strcmp(file_owner_username, current_groupname) == 0) {
        if (mode_bits & S_IRGRP)  retval = true;
        else retval = false;

    } else {
        if (mode_bits & S_IROTH)  retval = true;
        else retval = false;
    }

    free(file_owner_username);
    free(file_owner_groupname);
    free(current_username);
    free(current_groupname);

    return retval;
}

extern bool is_file_write_permitted(const struct entry *entry) {
    if (!is_directory_read_permitted(entry->previous)) return false;
    if (!is_directory_write_permitted(entry->previous)) return false;
    int retval;
    char *file_owner_username, *file_owner_groupname;
    char *current_username, *current_groupname;
    mode_t mode_bits = entry->attribute->st_mode;

    file_owner_username = strdup(getpwuid(entry->attribute->st_uid)->pw_name);
    file_owner_groupname = strdup(getgrgid(entry->attribute->st_gid)->gr_name);
    current_username = strdup(getpwuid(getuid())->pw_name);
    current_groupname = strdup(getgrgid(getgid())->gr_name);

    if (strcmp(current_username, "root") == 0) {
        return true;
    }

    if (strcmp(file_owner_username, current_username) == 0) {
        if (mode_bits & S_IWUSR)  retval = true;
        else retval = false;

    } else if (strcmp(file_owner_username, current_groupname) == 0) {
        if (mode_bits & S_IWGRP)  retval = true;
        else retval = false;

    } else {
        if (mode_bits & S_IWOTH)  retval = true;
        else retval = false;
    }

    free(file_owner_username);
    free(file_owner_groupname);
    free(current_username);
    free(current_groupname);

    return retval;
}

extern bool is_file_execute_permitted(const struct entry *entry) {
    if (!is_directory_read_permitted(entry->previous)) return false;
    int retval;
    char *file_owner_username, *file_owner_groupname;
    char *current_username, *current_groupname;
    mode_t mode_bits = entry->attribute->st_mode;

    file_owner_username = strdup(getpwuid(entry->attribute->st_uid)->pw_name);
    file_owner_groupname = strdup(getgrgid(entry->attribute->st_gid)->gr_name);
    current_username = strdup(getpwuid(getuid())->pw_name);
    current_groupname = strdup(getgrgid(getgid())->gr_name);

    if (strcmp(current_username, "root") == 0) {
        return true;
    }

    if (strcmp(file_owner_username, current_username) == 0) {
        if (mode_bits & S_IXUSR)  retval = true;
        else retval = false;

    } else if (strcmp(file_owner_username, current_groupname) == 0) {
        if (mode_bits & S_IXGRP)  retval = true;
        else retval = false;

    } else {
        if (mode_bits & S_IXOTH)  retval = true;
        else retval = false;
    }

    free(file_owner_username);
    free(file_owner_groupname);
    free(current_username);
    free(current_groupname);
    
    return retval;
}


static entry * get_entry_dup(const entry *source) { 
    entry *buffer, **p = &buffer;
    while(source != NULL) {
        *p = (entry *)malloc(sizeof (entry));;
        initialize(*p);
        if (source->filename != NULL) {
            (*p)->filename = strdup(source->filename);
        }
        if (source->received_path != NULL) {
            (*p)->received_path = strdup(source->received_path);
        }
        if (source->real_path != NULL) {
            (*p)->real_path = strdup(source->real_path);
        }
        if (source->attribute != NULL) {
            (*p)->attribute = (struct stat*)malloc(sizeof (struct stat));
            memcpy((*p)->attribute, source->attribute, sizeof (struct stat));
        }
        source = source->previous;
        p = &((*p)->previous);
    }
    return buffer;
}

extern entry * get_joint_entry(const char *filename, const entry *target) {
    entry *buffer = get_entry_dup(target);
    buffer->next = (entry *)malloc(sizeof (entry));
    buffer->next->previous = buffer;
    buffer = buffer->next;
    buffer->next = NULL;
    buffer->filename = strdup(filename);
    buffer->real_path = (char *)malloc(MAX_LEN * sizeof (char));
    buffer->received_path = (char *)malloc(MAX_LEN * sizeof (char));
    buffer->attribute = (struct stat*)malloc(sizeof (struct stat));
    if (strcmp(target->real_path, "/") != 0) {
        if (snprintf(buffer->real_path, MAX_LEN, "%s/%s", target->real_path, filename) > MAX_LEN) {
            die("%s: Error: buffer overflowed", program_name);
        }

    } else {
        if (snprintf(buffer->real_path, MAX_LEN, "/%s", filename) > MAX_LEN) {
            die("%s: Error: buffer overflowed", program_name);
        }
    }

    if (*(target->received_path + strlen(target->received_path) - 1) != '/') {
        if (snprintf(buffer->received_path, MAX_LEN, "%s/%s", target->received_path, filename) > MAX_LEN) {
            die("%s: Error: buffer overflowed", program_name);
        }

    } else {
        if (snprintf(buffer->received_path, MAX_LEN, "%s%s", target->received_path, filename) > MAX_LEN) {
            die("%s: Error: buffer overflowed", program_name);
        }
    }

    if (stat(buffer->real_path, buffer->attribute) == -1) {
        free(buffer->attribute);
        buffer->attribute = NULL;
    }

    return buffer;
}

extern entry * get_real_destination(const char *filename, const struct entry *target) {
    if (!is_entry_located(target)) {
        if (is_entry_located(target->previous)) {
            return get_entry_dup(target);

        } else {
            log_error("%s: cannot access '%s': No such file or directory", program_name, target->received_path);
            return NULL;
        }

    } else {
        if (!is_directory(target)) {
            log_error("%s: failed to access '%s': Not a directory", program_name, target->received_path);
            return NULL;

        } else {
            return get_joint_entry(filename, target);
        }
    }
}