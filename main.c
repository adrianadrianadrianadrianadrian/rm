#include <stdio.h>
#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "error.h"
// #include "context.h"
// #include "soundness.h"
// #include "type_checker.h"

int compile(char *file_name, struct error *error) {
    struct list_statement statements = {0};
    FILE *f = fopen(file_name, "r");
    struct token_buffer tb = create_token_buffer(f, file_name);
    if (!parse_rm_file(&tb, &statements, error)) return 0;
    // struct rm_program program = {0};
    // contextualise(&statements, &program);
    // if (!soundness_check(&program, error)) return 0;
    //if (!type_check(&program, error)) return 0;
    return 1;
}

struct data {
    int id;
};

int main(int argc, char **argv) {
    struct error error = {0};

    if (argc <= 1 || !strcmp(argv[1], "")) {
        write_raw_error(stderr, "no input file provided.");
        return 1;
    }

    if (!compile(argv[1], &error)) {
        write_error(stderr, &error);
        return 1;
    }

    return 0;
}
