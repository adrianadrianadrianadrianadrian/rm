#include "../ast.h"
#include "../context.h"
#include <assert.h>
#include "c.h"
#include <regex.h>
#include "../utils.h"
#include "../type_inference.h"

void write_type(struct type *ty, FILE *file);

void write_primitive_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_PRIMITIVE);
    switch (ty->primitive_type) {
        case VOID:
            fprintf(file, "void");
            return;
        case I8:
            fprintf(file, "char");
            return;
        case U8:
            fprintf(file, "unsigned char");
            return;
        case I16:
            fprintf(file, "int");
            return;
        case U16:
            fprintf(file, "unsigned int");
            return;
        case I32:
            fprintf(file, "int");
            return;
        case U32:
            fprintf(file, "unsigned int");
            return;
        case I64:
            fprintf(file, "long");
            return;
        case U64:
            fprintf(file, "unsigned long");
            return;
        case USIZE:
            fprintf(file, "size_t");
            return;
        case F32:
            fprintf(file, "float");
            return;
        case F64:
            fprintf(file, "double");
            return;
        case BOOL:
            fprintf(file, "char");
            return;
        case IO:
            UNREACHABLE("IO should never be written");
        default:
            UNREACHABLE("primitive type not handled");
    }
}

void append_int(int input, struct list_char *out) {
    size_t size = (int)(ceil(log10(input)) + 1);
    char *str = malloc(size);
    sprintf(str, "%d", input);
    for (size_t i = 0; i < size && str[i] != '\0'; i++) {
        list_append(out, str[i]);
    }
    free(str);
}

struct list_char apply_type_modifier(struct type_modifier modifier, struct list_char input)
{
    struct list_char output = list_create(char, input.size * 2);
    switch (modifier.kind) {
        case POINTER_MODIFIER_KIND:
        {
            list_append(&output, '(');
            list_append(&output, '*');
            copy_list_char(&output, &input);
            list_append(&output, ')');
            break;
        }
        case NULLABLE_MODIFIER_KIND:
        {
            copy_list_char(&output, &input);
            break;
        }
        case ARRAY_MODIFIER_KIND:
        {
            list_append(&output, '(');
            copy_list_char(&output, &input);
            list_append(&output, '[');
            if (modifier.array_modifier.literally_sized) {
                append_int(modifier.array_modifier.literal_size, &output);
            }
            list_append(&output, ']');
            list_append(&output, ')');
            break;
        }
        case MUTABLE_MODIFIER_KIND:
            // TODO: this makes it tricky. Revisit.
            break;
        }
    list_append(&output, '\0');
    return output;
}

struct list_char apply_type_modifiers(struct list_type_modifier modifiers, struct list_char input)
{
    struct list_char output = input;
    for (size_t i = 0; i < modifiers.size; i++) {
        output = apply_type_modifier(modifiers.data[i], output);
    }
    return output;
}

void write_struct_type(struct type *ty, int full, FILE *file) {
    assert(ty->kind == TY_STRUCT);
    if (!full) {
        fprintf(file, "struct %s", ty->name->data);
        return;
    }

    fprintf(file, "struct %s {", ty->name->data);
    size_t pair_count = ty->struct_type.pairs.size;
    for (size_t i = 0; i < pair_count; i++) {
        struct key_type_pair pair = ty->struct_type.pairs.data[i];
        write_type(pair.field_type, file);
        struct list_char modified = apply_type_modifiers(pair.field_type->modifiers, pair.field_name);
        fprintf(file, " %s", modified.data);
        fprintf(file, ";");
    }
    fprintf(file, "};");
}

void write_enum_type(struct type *ty, int full, FILE *file) {
    assert(ty->kind == TY_ENUM);
    if (!full) {
        fprintf(file, "struct %s_type", ty->name->data);
        return;
    }

    size_t variant_count = ty->enum_type.pairs.size;
    fprintf(file, "enum %s_kind {", ty->name->data);
    for (size_t i = 0; i < variant_count; i++) {
        fprintf(file, "%s_kind_%s", ty->name->data, ty->enum_type.pairs.data[i].field_name.data);
        if (i < variant_count - 1) {
            fprintf(file, ",");
        }
    }

    fprintf(file, "}; ");
    fprintf(file, "struct %s_type { enum %s_kind %s_kind; union {",
            ty->name->data, ty->name->data, ty->name->data);

    for (size_t i = 0; i < variant_count; i++) {
        struct key_type_pair pair = ty->enum_type.pairs.data[i];
        write_type(pair.field_type, file);
        struct list_char union_name = list_create(char, pair.field_name.size * 2);
        for (size_t j = 0; j < ty->name->size; j++) {
            char to_add = ty->name->data[j];
            if (to_add != '\0') {
                list_append(&union_name, ty->name->data[j]);
            }
        } 
        append_list_char_slice(&union_name, "_type_");
        for (size_t j = 0; j < pair.field_name.size; j++) {
            list_append(&union_name, pair.field_name.data[j]);
        } 
        struct list_char modified = apply_type_modifiers(pair.field_type->modifiers, union_name);
        fprintf(file, " %s;", modified.data);
    }
    fprintf(file, "};};");
    
    // TODO: current idea is to generate constructor functions per enum variant
    // 
    // struct person { int age; };
    // enum result_kind {
    //     result_kind_ok,
    //     result_kind_error
    // };
    // struct result_type {
    //     enum result_kind result_kind;
    //     union {
    //      struct person result_type_ok;
    //      char result_type_error;
    //     };
    // };
    //
    // would produce the following:
    // struct result_type result_type_ok(struct person p);
    // struct result_type result_type_error(char err);
    // Then when we write the binding expressions for enums we can map to these functions
}

void write_function_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_FUNCTION);
    write_type(ty->function_type.return_type, file);
    struct list_type_modifier return_modifiers = ty->function_type.return_type->modifiers;
    for (size_t i = 0; i < return_modifiers.size; i++) {
        if (return_modifiers.data[i].kind == POINTER_MODIFIER_KIND) {
            fprintf(file, "*");
        }
    }
    fprintf(file, " %s(", ty->name->data);

    size_t param_count = ty->function_type.params.size;
    for (size_t i = 0; i < param_count; i++) {
        struct key_type_pair pair = ty->function_type.params.data[i];
        if (pair.field_type->kind == TY_PRIMITIVE && pair.field_type->primitive_type == IO) {
            continue;
        }
        write_type(pair.field_type, file);
        struct list_char modified = apply_type_modifiers(pair.field_type->modifiers, pair.field_name);
        fprintf(file, " %s", modified.data);
        if (i < param_count - 1) {
            fprintf(file, ", ");
        }
    }
    fprintf(file, ")");
}

void write_type(struct type *ty, FILE *file) {
    switch (ty->kind) {
        case TY_PRIMITIVE:
            write_primitive_type(ty, file);
            break;
        case TY_STRUCT:
            write_struct_type(ty, 0, file);
            break;
        case TY_FUNCTION:
            write_function_type(ty, file);
            break;
        case TY_ENUM:
            write_enum_type(ty, 0, file);
            break;
        default:
            break;
            //UNREACHABLE("type kind not handled");
    }
}

void write_expression(struct expression *e,
                      struct global_context *gc,
                      struct list_scoped_variable *scoped_variables,
                      FILE *file);

void write_literal_expression(struct literal_expression *e,
                              struct global_context *global_context,
                              struct list_scoped_variable *scoped_variables,
                              FILE *file)
{
    switch (e->kind) {
        case LITERAL_BOOLEAN:
        {
            fprintf(file, "%d", e->boolean);
            break;
        }
        case LITERAL_CHAR:
        {
            fprintf(file, "'%c'", e->character);
            break;
        }
        case LITERAL_STR:
        {
            fprintf(file, "\"%s\"", e->str->data);
            break;
        }
        case LITERAL_NUMERIC:
        {
            fprintf(file, "%d", (int)e->numeric); // TODO
            break;
        }
        case LITERAL_NAME:
        {
            fprintf(file, "%s", e->name->data);
            break;
        }
        case LITERAL_HOLE:
            TODO("write hole");
            break;
        case LITERAL_STRUCT:
        {
            fprintf(file, "(struct %s) {", e->struct_enum.name->data);
            size_t pair_count = e->struct_enum.key_expr_pairs.size;
            for (size_t i = 0; i < pair_count; i++) {
                struct key_expression pair = e->struct_enum.key_expr_pairs.data[i];
                fprintf(file, ".%s = ", pair.key->data);
                write_expression(pair.expression, global_context, scoped_variables, file);
                if (i + 1 < pair_count) {
                    fprintf(file, ",");
                }
            }
            fprintf(file, "}");
            break;
        }
        case LITERAL_ENUM:
            TODO("literal enum");
            break;
        case LITERAL_NULL:
            // TODO: this is tmp
            fprintf(file, "NULL");
            break;
        }
}

void write_unary_expression(struct unary_expression *e,
                            struct global_context *global_context,
                            struct list_scoped_variable *scoped_variables,
                            FILE *file)
{
    switch (e->unary_operator) {
        case BANG_UNARY:
            fprintf(file, "!");
            break;
        case STAR_UNARY:
            fprintf(file, "*");
            break;
        case MINUS_UNARY:
            fprintf(file, "-");
            break;
        default:
            UNREACHABLE("unary operator not handled");
    }

    write_expression(e->expression, global_context, scoped_variables, file);
}

int expression_is_pointer(struct expression *e,
                          struct global_context *global_context,
                          struct list_scoped_variable *scoped_variables)
{
    switch (e->kind) {
        case LITERAL_EXPRESSION:
        {
            if (e->literal.kind == LITERAL_NAME) {
                struct type variable_type = {0};
                if (get_scoped_variable_type(scoped_variables,
                        global_context,
                        *e->literal.name,
                        &variable_type))
                {
                    for (size_t i = 0; i < variable_type.modifiers.size; i++) {
                        if (variable_type.modifiers.data[i].kind == POINTER_MODIFIER_KIND) {
                            return 1;
                        }
                    }
                }
            }

            break;
        }
        case UNARY_EXPRESSION:
        case BINARY_EXPRESSION:
        case GROUP_EXPRESSION:
        case FUNCTION_EXPRESSION:
        case VOID_EXPRESSION:
        case MEMBER_ACCESS_EXPRESSION:
            break;
    }

    return 0;
}

void write_binary_expression(struct binary_expression *e,
                             struct global_context *global_context,
                             struct list_scoped_variable *scoped_variables,
                             FILE *file)
{
    write_expression(e->l, global_context, scoped_variables, file);
    switch (e->binary_op) {
        case PLUS_BINARY:
            fprintf(file, " + ");
            break;
        case MINUS_BINARY:
            fprintf(file, " - ");
            break;
        case OR_BINARY:
            fprintf(file, " || ");
            break;
        case AND_BINARY:
            fprintf(file, " && ");
            break;
        case BITWISE_OR_BINARY:
            fprintf(file, " | ");
            break;
        case BITWISE_AND_BINARY:
            fprintf(file, " & ");
            break;
        case GREATER_THAN_BINARY:
            fprintf(file, " > ");
            break;
        case LESS_THAN_BINARY:
            fprintf(file, " < ");
            break;
        case EQUAL_TO_BINARY:
            fprintf(file, " == ");
            break;
        case MULTIPLY_BINARY:
            fprintf(file, " * ");
            break;
        // case DOT_BINARY:
        // {
        //     if (expression_is_pointer(e->l, global_context, scoped_variables)) {
        //         fprintf(file, "->");
        //     } else {
        //         fprintf(file, ".");
        //     }
        //     break;
        // }
        default:
            UNREACHABLE("binary operator not handled");
    }
    write_expression(e->r, global_context, scoped_variables, file);
}

void write_grouped_expression(struct expression *e,
                              struct global_context *global_context,
                              struct list_scoped_variable *scoped_variables,
                              FILE *file)
{
    fprintf(file, "(");
    write_expression(e, global_context, scoped_variables, file);
    fprintf(file, ")");
}

void write_member_access_expression(struct member_access_expression *e,
                                    struct global_context *global_context,
                                    struct list_scoped_variable *scoped_variables,
                                    FILE *file)
{
    write_expression(e->accessed, global_context, scoped_variables, file);
    fprintf(file, ".%s", e->member_name->data);
}

void write_function_expression(struct function_expression *e,
                               struct global_context *global_context,
                               struct list_scoped_variable *scoped_variables,
                               FILE *file)
{
    fprintf(file, "%s(", e->function_name->data);
    size_t param_count = e->params->size;
    for (size_t i = 0; i < param_count; i++) {
        struct type param_type = {0};
        
        // TODO: later we'll attach the type to the expression earlier on.
        assert(infer_expression_type(&e->params->data[i],
                                     global_context,
                                     scoped_variables,
                                     &param_type,
                                     NULL));
        if (param_type.kind == TY_PRIMITIVE && param_type.primitive_type == IO) {
            continue;
        }

        write_expression(&e->params->data[i], global_context, scoped_variables, file);
        if (i < param_count - 1) {
            fprintf(file, ", ");
        }
    }
    fprintf(file, ")");
}

void write_expression(struct expression *e,
                      struct global_context *gc,
                      struct list_scoped_variable *scoped_variables,
                      FILE *file)
{
    switch (e->kind) {
        case LITERAL_EXPRESSION:
            write_literal_expression(&e->literal, gc, scoped_variables, file);
            return;
        case UNARY_EXPRESSION:
            write_unary_expression(&e->unary, gc, scoped_variables, file);
            return;
        case BINARY_EXPRESSION:
            write_binary_expression(&e->binary, gc, scoped_variables, file);
            return;
        case GROUP_EXPRESSION:
            write_grouped_expression(e->grouped, gc, scoped_variables, file);
            return;
        case FUNCTION_EXPRESSION:
            write_function_expression(&e->function, gc, scoped_variables, file);
            return;
        case MEMBER_ACCESS_EXPRESSION:
            write_member_access_expression(&e->member_access, gc, scoped_variables, file);
            return;
        case VOID_EXPRESSION:
            return;
        default:
            UNREACHABLE("expression kind not handled");
    }
}

void write_statement(struct statement_context *s, FILE *file);

void write_type_default(struct type *type, FILE *file) {
    // TODO: derive default C values 
    fprintf(file, "0");
}

void write_binding_statement(struct statement_context *c, FILE *file) {
    assert(c->kind == BINDING_STATEMENT);
    struct binding_statement_context *s = &c->binding_statement;
    if (s->binding_statement->has_type) {
        write_type(&s->binding_statement->variable_type, file);
    } else {
        write_type(&s->inferred_type, file);
    }

    fprintf(file, " %s = ", s->binding_statement->variable_name.data);
    if (s->binding_statement->value.kind == LITERAL_EXPRESSION
        && s->binding_statement->value.literal.kind == LITERAL_NULL) {
        // TODO: check has_type
        write_type_default(&s->binding_statement->variable_type, file);
    } else {
        write_expression(&s->binding_statement->value, c->global_context, &c->scoped_variables, file);
    }
    fprintf(file, ";");
}

void write_if_statement(struct statement_context *c, FILE *file) {
    assert(c->kind == IF_STATEMENT);
    struct if_statement_context *s = &c->if_statement_context;
    fprintf(file, "if (");
    write_expression(s->condition.e, c->global_context, &c->scoped_variables, file);
    fprintf(file, ")");
    write_statement(s->success_statement, file);
    if (s->else_statement != NULL) {
        fprintf(file, " else ");
        write_statement(s->else_statement, file);
    }
}

void write_return_statement(struct statement_context *c, FILE *file) {
    assert(c->kind == RETURN_STATEMENT);
    struct return_statement_context *s = &c->return_statement;
    fprintf(file, "return ");
    write_expression(s->e.e, c->global_context, &c->scoped_variables, file);
    fprintf(file, ";");
}

struct_list(int);

void write_block_statement(struct list_statement_context *statements, FILE *file) {
    fprintf(file, "{");
    for (size_t i = 0; i < statements->size; i++) {
        write_statement(&statements->data[i], file);
    }
    fprintf(file, "}");
}

void write_action_statement(struct statement_context *c, FILE *file) {
    assert(c->kind == ACTION_STATEMENT);
    struct action_statement_context *s = &c->action_statement_context;
    write_expression(s->e.e, c->global_context, &c->scoped_variables, file);
    fprintf(file, ";");
}

void write_while_statement(struct statement_context *c, FILE *file) {
    assert(c->kind == WHILE_LOOP_STATEMENT);
    struct while_loop_statement_context *s = &c->while_loop_statement;
    fprintf(file, "while (");
    write_expression(s->condition.e, c->global_context, &c->scoped_variables, file);
    fprintf(file, ")");
    write_statement(s->do_statement, file);
}

void write_type_declaration_statement(struct type_declaration_statement_context *s,
                                      FILE *file)
{
    write_type(&s->type, file);
    if (s->type.kind == TY_FUNCTION) {
		assert(s->statements != NULL);
        write_block_statement(s->statements, file);
    }
}

void write_break_statement(FILE *file) {
    fprintf(file, "break;");
}

// void write_include_statement(struct include_statement *s, FILE *file) {
//     fprintf(file, "#include");
//     if (s->external) {
//         fprintf(file, " <");
//     } else {
//         fprintf(file, " \"");
//     }
//     fprintf(file, "%s", s->include.data);
//     if (s->external) {
//         fprintf(file, ">");
//     } else {
//         fprintf(file, "\"");
//     }
//     fprintf(file, "\n");
// }

void write_case_predicate(struct switch_pattern *p,
                          const char *switch_name,
                          FILE *file)
{
    // Just hacking this together for now, to play.
    switch (p->switch_pattern_kind) {
        case OBJECT_PATTERN_KIND:
        {
            break;
        }
        case ARRAY_PATTERN_KIND:
            break;
        case NUMBER_PATTERN_KIND:
        {
            fprintf(file, "if (*%s == %lf)", switch_name, p->number_pattern.number);
            break;
        }
        case STRING_PATTERN_KIND:
        {
            fprintf(file, "if (strcmp(%s, \"%s\") == 0)", switch_name, p->string_pattern.str.data);
            break;
        }
        case VARIABLE_PATTERN_KIND:
        {
            fprintf(file, "if (1)");
            break;
        }
        case UNDERSCORE_PATTERN_KIND:
        {
            fprintf(file, "if (1)");
            break;
        }
        case REST_PATTERN_KIND:
            UNREACHABLE("todo error handling");
        }
}

void write_case_statement(struct case_statement *s,
                          const char *switch_name,
                          FILE *file)
{
    // write_case_predicate(&s->pattern, switch_name, scope, file);
    // fprintf(file, "{");
    // write_statement(s->statement, file);
    // fprintf(file, "}");
}

void write_switch_statement(struct switch_statement *s, FILE *file) {
    // struct type inferred_type = {0};
    // if (infer_type(&s->switch_expression, scope, &inferred_type)) {
    //     write_type(&inferred_type, file);
    //     fprintf(file, " *t = &");
    //     write_expression(&s->switch_expression, scope, file);
    //     fprintf(file, ";");
    // }
    //
    // for (size_t i = 0; i < s->cases.size; i++) {
    //     write_case_statement(&s->cases.data[i], "t", scope, file);
    // }
}

void write_c_block(struct c_block_statement *s, FILE *file) {
    fprintf(file, "%s\n", s->raw_c->data);
}

void write_statement(struct statement_context *s, FILE *file) {
    switch (s->kind) {
        case BINDING_STATEMENT:
            write_binding_statement(s, file);
            break;
        case IF_STATEMENT:
            write_if_statement(s, file);
            break;
        case RETURN_STATEMENT:
            write_return_statement(s, file);
            break;
        case BLOCK_STATEMENT:
            write_block_statement(s->block_statements, file);
            break;
        case ACTION_STATEMENT:
            write_action_statement(s, file);
            break;
        case WHILE_LOOP_STATEMENT:
            write_while_statement(s, file);
            break;
        case TYPE_DECLARATION_STATEMENT:
            write_type_declaration_statement(&s->type_declaration, file);
            break;
        case BREAK_STATEMENT:
            write_break_statement(file);
            break;
        case SWITCH_STATEMENT:
            //write_switch_statement(&s->switch_statement, file);
            break;
        case C_BLOCK_STATEMENT:
            write_c_block(&s->c_block_statement, file);
            break;
        default:
            UNREACHABLE("statement type not handled");
    }
}

struct generated_c_state {
    struct list_type *data_types;
    struct list_type *fn_types;
};

static void generate_c_file(struct rm_program *file) {
    FILE *output_file = fopen("target/c_output.c", "w");
    fprintf(output_file, "#include \"c_output.h\"\n");

    for (size_t i = 0; i < file->statements.size; i++) {
        struct statement_context s = file->statements.data[i];

		switch (s.kind) {
			case TYPE_DECLARATION_STATEMENT:
			{
				switch (s.type_declaration.type.kind) {
					case TY_FUNCTION:
        				write_statement(&s, output_file);
						break;
					case TY_PRIMITIVE:
					{
						fprintf(stderr, "woopssss");
						exit(-1);
					}
					case TY_ENUM:
					case TY_STRUCT:
						break;
				}
				break;
			}
			case BINDING_STATEMENT:
			case IF_STATEMENT:
			case RETURN_STATEMENT:
			case BLOCK_STATEMENT:
			case ACTION_STATEMENT:
			case WHILE_LOOP_STATEMENT:
            case SWITCH_STATEMENT:
			case BREAK_STATEMENT:
			case C_BLOCK_STATEMENT:
			{
				fprintf(stderr, "woops");
				exit(-1);
			}
        } 
    }
}

void generate_c_header(struct rm_program *program) {
    struct global_context *global_context = &program->global_context;
    FILE *header = fopen("target/c_output.h", "w");
    fprintf(header, "#ifndef C_OUTPUT_H\n#define C_OUTPUT_H\n");
    fprintf(header, "#include <stdio.h>\n");
    fprintf(header, "#include <stdlib.h>\n");

    for (size_t i = 0; i < global_context->data_types.size; i++) {
        struct type data_type = global_context->data_types.data[i];
        if (data_type.kind == TY_STRUCT) {
            write_struct_type(&global_context->data_types.data[i], 1, header);
        } else if (data_type.kind == TY_ENUM) {
            write_enum_type(&global_context->data_types.data[i], 1, header);
        } else {
            UNREACHABLE("generating data types in c header");
        }
    }

    for (size_t i = 0; i < global_context->fn_types.size; i++) {
        write_function_type(&global_context->fn_types.data[i], header);
        fprintf(header, ";");
    }
    
    fprintf(header, "\n#endif");
}

void generate_c(struct rm_program *program) {
    generate_c_header(program);
    generate_c_file(program);
}
