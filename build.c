#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include "lib/collections.c"

typedef struct list_char string;
struct_list(string);

#define ERROR(...)                        \
    do {                                  \
        fprintf(stderr, "BUILD ERROR: "); \
        fprintf(stderr, __VA_ARGS__);     \
        fprintf(stderr, "\n");            \
        exit(1);                          \
    } while (0)

struct list_char read_file_to_string(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        ERROR("issue opening `%s`", filename);
    }

    struct list_char output = list_create(char, 100);
    char buf[1024] = {0};
    while (fread(&buf, sizeof(char), sizeof(*buf), file)) {
        for (int i = 0; i < sizeof(*buf); ++i) {
            list_append(&output, buf[i]);
        }
    }

    return output;
}

struct list_char from_slice(char *slice)
{
    size_t len = strlen(slice);
    struct list_char output = list_create(char, len + 1);
    for (size_t i = 0; i < len; ++i) {
        list_append(&output, slice[i]);
    }
    return output;
}

struct list_string filter(struct list_string *input,
                          int (*p)(struct list_char *))
{
    struct list_string output = list_create(string, input->size);
    for (size_t i = 0; i < input->size; ++i) {
        if (p(&input->data[i])) {
            list_append(&output, input->data[i]);
        }
    }
    return output;
}

int ends_with(struct list_char *input, char *str)
{
    size_t len = strlen(str);
    if (len > input->size) {
        return 0;
    }

    for (int i = 0; i < len; ++i) {
        if (input->data[input->size - (len - i)] != str[i]) {
            return 0;
        }
    }

    return 1;
}

int is_c_file(struct list_char *input)
{
    return ends_with(input, ".c");
}

void read_file_names_recursive(char *dir,
                               struct list_string *out)
{
    DIR *FD = NULL;
    if (NULL == (FD = opendir(dir))) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(FD))) {
        switch (entry->d_type) {
            case DT_DIR:
            {
                if (!strcmp(".", entry->d_name)
                    || !strcmp("..", entry->d_name)
                    || entry->d_name[0] == '.')
                {
                    continue;
                }

                struct list_char inner_dir = from_slice(dir);
                append_list_char_slice(&inner_dir, "/");
                append_list_char_slice(&inner_dir, entry->d_name);
                append_list_char_slice(&inner_dir, "\0");
                read_file_names_recursive(inner_dir.data, out);
                continue;
            }
            case DT_REG:
            {
                struct list_char name = list_create(char, strlen(entry->d_name));
                append_list_char_slice(&name, dir);
                list_append(&name, '/');
                append_list_char_slice(&name, entry->d_name);
                list_append(out, name);
                continue;
            }
        }
    }
}

int main(int argc, char **argv)
{
    struct list_string files = list_create(string, 100);
    read_file_names_recursive("src", &files);
    read_file_names_recursive("lib", &files);
    struct list_string c_files = filter(&files, is_c_file);

    struct list_char cmd = list_create(char, 100);
    append_list_char_slice(&cmd, "gcc");
    append_list_char_slice(&cmd, " -o compiler");

    for (int i = 0; i < c_files.size; ++i) {
        append_list_char_slice(&cmd, " ");
        append_list_char_slice(&cmd, c_files.data[i].data);
    }

    append_list_char_slice(&cmd, " -lm");
    append_list_char_slice(&cmd, " -D DEBUG_CONTEXT");
    list_append(&cmd, '\0');

    int result = system(cmd.data);
    if (result) {
        ERROR("compilation failed.");
    }

    return 0;
}
