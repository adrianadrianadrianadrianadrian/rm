#include <stdio.h>
#include "parser.h"
#include "ast.h"
#include "utils.h"
#include "lexer.h"
#include "error.h"
#include "context.h"
#include "soundness.h"
// #include "type_checker.h"

int compile(char *file_name, struct error *error) {
    FILE *f = fopen(file_name, "r");
    struct token_buffer tb = create_token_buffer(f, file_name);
    struct parsed_file parsed = {0};
    if (!parse_file(&tb, &parsed, error)) return 0;
    struct context c = {0};
    if (!contextualise(&parsed, &c, error)) return 0;
    if (!soundness_check(&parsed, &c, error)) return 0;
    //if (!type_check(&program, error)) return 0;
    return 1;
}

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
    
    printf("Compilation successful!\n");
    return 0;
}
