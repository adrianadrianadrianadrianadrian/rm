#include <stdio.h>
#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "backends/c.c"

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r");
    struct token_buffer tok_buf = create_token_buffer(f);

    struct rm_file file = {0};
    if (!parse_rm_file(&tok_buf, &file)) {
        printf("Failed to parse rm file.");
        return 1;
    }
    
    generate_c(file);
    return 0;
}
