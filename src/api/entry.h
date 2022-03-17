#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include "bool.h"

#include "name.h"

#include "limits.h"

#include "error.h"

typedef struct entry {
    char *filename;
    char *received_path;
    char *real_path;
    struct stat *attribute;
    struct entry *previous;
    struct entry *next;
} entry;

extern entry * get_entries_chain(const char *path);

extern void free_entry(struct entry *entry);

extern entry * get_joint_entry(const char *filename, const entry *target);

extern entry * get_real_destination(const char *filename, const struct entry *target);

extern bool is_entry_located(const struct entry *entry);

extern bool is_file(const struct entry *entry);

extern bool is_directory(const struct entry *entry);

extern bool is_empty_directory(const struct entry *entry);

extern bool is_same_entry(const struct entry *entry_A, const struct entry *entry_B);

extern bool is_subdirectory(const struct entry *entry_A, const struct entry *entry_B);

extern bool is_directory_read_permitted(const struct entry *entry);

extern bool is_directory_write_permitted(const struct entry *entry);

extern bool is_file_read_permitted(const struct entry *entry);

extern bool is_file_write_permitted(const struct entry *entry);

extern bool is_file_execute_permitted(const struct entry *entry);
