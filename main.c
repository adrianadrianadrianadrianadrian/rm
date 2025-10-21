#include <stdio.h>
#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "context.c"
#include "type_check.c"
#include "lowering/c.h"

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r");
    struct token_buffer tok_buf = create_token_buffer(f);

    struct list_statement file = {0};
    if (!parse_rm_file(&tok_buf, &file)) {
        printf("Failed to parse rm file.");
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
