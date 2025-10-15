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

    // struct list_context c = list_create(context, 100);
    // struct error err = (struct error) {
    //     .message = list_create(char, 100)
    // };
    //
    // int result = contextualize(&file.statements, &c, &err);
    // if (!result) {
    //     printf("Error: %s\n", err.message.data);
    //     exit(-1);
    // }
    //
    // for (size_t i = 0; i < c.size; i++) {
    //     debug_context(&c.data[i]);
    // }
    
    //generate_c(file);
    return 0;
}
