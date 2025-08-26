#include <stdio.h>
#include "parser.c"

void display_token(struct token token) {
    switch (token.token_type) {
        case SEMICOLON:
            printf(";");
            return;
        case COLON:
            printf(":");
            return;
        case IDENTIFIER:
            printf("%s", token.identifier->data);
            return;
        case PAREN_OPEN:
        {
            switch (token.paren_type) {
                case ROUND:
                    printf("(");
                    return;
                case CURLY:
                    printf("{");
                    return;
                case SQUARE:
                    printf("[");
                    return;
            }
            return;
        }
        case PAREN_CLOSE:
        {
            switch (token.paren_type) {
                case ROUND:
                    printf(")");
                    return;
                case CURLY:
                    printf("}");
                    return;
                case SQUARE:
                    printf("]");
                    return;
            }
            return;
        }
        case ARROW:
            printf("->");
            return;
        case MATH_OPERATOR:
        {
            switch (token.math_op_type) {
                case EQ:
                    printf("=");
                    return;
                case BANG:
                    printf("!");
                    return;
                case GREATER:
                    printf(">");
                    return;
                case LESS:
                    printf("<");
                    return;
                case MOD:
                    printf("%%");
                    return;
                case DIV:
                    printf("/");
                    return;
                case PLUS:
                    printf("+");
                    return;
                case MINUS:
                    printf("-");
                    return;
                }
            return;
        }
        case KEYWORD:
        {
            switch (token.keyword_type) {
                case FN:
                    printf("fn");
                    return;
                case ENUM:
                    printf("enum");
                    return;
                case STRUCT:
                    printf("struct");
                    return;
                case IF:
                    printf("if");
                    return;
                case WHILE:
                    printf("while");
                    return;
                case RETURN:
                    printf("return");
                    return;
                case BOOLEAN_TRUE:
                    printf("true");
                    return;
                case BOOLEAN_FALSE:
                    printf("false");
                    return;
                case ELSE:
                    printf("else");
                    return;
            }
            return;
        }
        case COMMA:
            printf(",");
            return;
        case PIPE:
            printf("|");
            return;
        case CHAR_LITERAL:
            printf("%c", token.identifier->data[0]);
            return;
        case STR_LITERAL:
            printf("%s", token.identifier->data);
            return;
        case STAR:
            printf("*");
            return;
        case AND:
            printf("&");
            return;
        case NUMERIC:
            printf("%f", token.numeric);
            return;
    }
}

void display_expression(struct expression *expr)
{
    switch (expr->kind) {
        case LITERAL_EXPRESSION:
        {
            switch (expr->literal.kind) {
                case LITERAL_BOOLEAN:
                    printf("%d", expr->literal.boolean);
                    break;
                case LITERAL_CHAR:
                    printf("%c", expr->literal.character);
                    break;
                case LITERAL_STR:
                    printf("%s", expr->literal.str->data);
                    break;
                case LITERAL_NUMERIC:
                    printf("%lf", expr->literal.numeric);
                    break;
                case LITERAL_NAME:
                    printf("%s", expr->literal.name->data);
                    break;
                case LITERAL_HOLE:
                    printf("_");
                    break;
                }
            break;
        }
        case UNARY_EXPRESSION:
        {
            switch (expr->unary.operator) {
                case BANG_UNARY:
                    printf("!");
                    break;
                case STAR_UNARY:
                    printf("*");
                    break;
                case MINUS_UNARY:
                    printf("-");
                    break;
            }
            display_expression(expr->unary.expression);
            break;
        }
        case GROUP_EXPRESSION:
        {
            printf("(");
            display_expression(expr->grouped);
            printf(")");
            break;
        }
        case BINARY_EXPRESSION:
        {
            switch (expr->binary.binary_op) {
                case PLUS_BINARY:
                {
                    display_expression(expr->binary.l);
                    printf(" + ");
                    display_expression(expr->binary.r);
                    break;
                }
                case OR_BINARY:
                {
                    display_expression(expr->binary.l);
                    printf(" || ");
                    display_expression(expr->binary.r);
                    break;
                }
                case AND_BINARY:
                {
                    display_expression(expr->binary.l);
                    printf(" && ");
                    display_expression(expr->binary.r);
                    break;
                }
                case BITWISE_OR_BINARY:
                {
                    display_expression(expr->binary.l);
                    printf(" | ");
                    display_expression(expr->binary.r);
                    break;
                }
                case BITWISE_AND_BINARY:
                {
                    display_expression(expr->binary.l);
                    printf(" & ");
                    display_expression(expr->binary.r);
                    break;
                }
            }
            break;
        }
        case FUNCTION_EXPRESSION:
        {
            printf("%s(", expr->function.function_name->data);
            for (size_t i = 0; i < expr->function.params->size; i++) {
                display_expression(&expr->function.params->data[i]);

                if (i < expr->function.params->size - 1
                    && expr->function.params->size > 1)
                {
                    printf(", ");
                }
            }
            printf(")");
        }
        default:
            break;
    }
}

int main(void) {
    FILE *f = fopen("test.rm", "r");
    struct file_buffer b = create_file_buffer(f);
    struct token_buffer tok_buf = create_token_buffer(&b);

    struct rm_file file = {0};
    if (!parse_rm_file(&tok_buf, &file)) {
        printf("failed");
    }

    for (size_t i = 0; i < file.statements.size; i++) {
        write_statement(&file.statements.data[i]);
    }

    return 0;
}
