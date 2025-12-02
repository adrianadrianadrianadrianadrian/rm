#include <stdio.h>
#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "error.h"
#include "context.h"
#include "type_checker.h"
#include "lowering/c.h"

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r");
    struct token_buffer tok_buf = create_token_buffer(f, argv[1]);
    struct error error = {0};
    struct list_statement statements = {0};

    if (!parse_rm_file(&tok_buf, &statements, &error)) {
        write_error(stderr, &error);
        return 1;
    }

    struct rm_program program = {0};
    if (!contextualise(&statements, &program, &error)) {
        write_error(stderr, &error);
        return 1;
    }

    if (!type_check(program.statements, &error)) {
        write_error(stderr, &error);
        return 1;
    }

    generate_c(&program);
    return 0;
}
