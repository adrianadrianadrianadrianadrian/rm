#include <stdio.h>
#include "parser.c"

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r");
    struct file_buffer b = create_file_buffer(f);
    struct token_buffer tok_buf = create_token_buffer(&b);

    struct rm_file file = {0};
    if (!parse_rm_file(&tok_buf, &file)) {
        printf("failed");
    }

    FILE *program = fopen("program.c", "w");
    for (size_t i = 0; i < file.statements.size; i++) {
        write_statement(&file.statements.data[i], program);
    }

    return 0;
}
