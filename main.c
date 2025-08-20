#include <stdio.h>
#include "parser.c"

int main(void) {
    // FILE *f = fopen("test.rn", "r");
    // struct file_buffer b = create_file_buffer(f);
    // struct token_buffer tok_buf = create_token_buffer(&b);
    //
    // struct expression expr = {0};
    // if (parse_expression(&tok_buf, &expr)) {
    //     switch (expr.kind) {
    //         case LITERAL_EXPRESSION:
    //             switch (expr.literal.kind) {
    //                 case LITERAL_BOOLEAN:
    //                     printf("%d\n", expr.literal.boolean);
    //                     break;
    //                 case LITERAL_CHAR:
    //                     printf("%c\n", expr.literal.character);
    //                     break;
    //                 case LITERAL_STR:
    //                     printf("%s\n", expr.literal.str->data);
    //                     break;
    //                 case LITERAL_NUMERIC:
    //                     printf("%lf\n", expr.literal.numeric);
    //                     break;
    //             }
    //         case UNARY_EXPRESSION:
    //         {
    //             switch (expr.unary.operator) {
    //                 case BANG_UNARY:
    //                     printf("!");
    //                     break;
    //             }
    //         }
    //         default:
    //             break;
    //     }
    // }
    //
    // return 0;
}
