#include <stdio.h>
#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "context.c"
#include "type_check.c"
#include "lowering/c.h"

void print_parse_error(struct parse_error *err, int depth) {
    printf("[syntax_error] %s:%d:%d: %s\n",
        err->token_metadata->file_name,
        err->token_metadata->row,
        err->token_metadata->col + 1,
        err->error_message.data);

    if (err->inner != NULL) {
        print_parse_error(err->inner, ++depth);
    }
}

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r");
    struct token_buffer tok_buf = create_token_buffer(f, argv[1]);

    struct parse_error error = {0};
    struct list_statement file = {0};
    if (!parse_rm_file(&tok_buf, &file, &error)) {
        print_parse_error(&error, 0);
        return 1;
    }

    struct rm_program program = {0};
    struct context_error err = (struct context_error) {
        .message = list_create(char, 100)
    };

    int result = contextualise(&file, &program, &err);
    if (!result) {
        printf("Error: %s\n", err.message.data);
        exit(-1);
    }
    
    struct type_check_error type_error = {0};
    int type_check_result = type_check(program.statements, &type_error);
    if (!type_check_result) {
        printf("Error: %s\n", type_error.error_message.data);
        exit(-1);
    }

    generate_c(&program);
    return 0;
}
