#include "ast.h"
#include <assert.h>

struct statement_slice {
	struct statement *statements;
	size_t len;
};

struct c_scope {
	struct statement_slice preceding_statements;
    struct list_key_type_pair scoped_variables;
	struct c_scope *parent;
};

void write_type(struct type *ty, FILE *file);

void write_primitive_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_PRIMITIVE);
    switch (ty->primitive_type) {
        case UNIT:
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
            if (modifier.array_modifier.sized) {
                append_int(modifier.array_modifier.size, &output);
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

void write_struct_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_STRUCT);
    if (ty->struct_type.predefined) {
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

void write_enum_type(struct type *ty, FILE *file) {
    assert(ty->kind == TY_ENUM);
    if (ty->enum_type.predefined) {
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
            write_struct_type(ty, file);
            break;
        case TY_FUNCTION:
            write_function_type(ty, file);
            break;
        case TY_ENUM:
            write_enum_type(ty, file);
            break;
        default:
            UNREACHABLE("type kind not handled");
    }
}

void write_expression(struct expression *e, struct c_scope *scope, FILE *file);

void write_literal_expression(struct literal_expression *e, struct c_scope *scope, FILE *file) {
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
                write_expression(pair.expression, scope, file);
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

int infer_type(struct expression *e, struct c_scope *scope, struct type *out);

int get_scoped_variable_type(struct c_scope *scope,
					         struct list_char variable_name,
							 struct type *out)
{
	for (size_t i = 0; i < scope->preceding_statements.len; i++) {
		struct statement s = scope->preceding_statements.statements[i];
		if (s.kind == BINDING_STATEMENT) {
			if (list_char_eq(&s.binding_statement.variable_name, &variable_name))
			{
                if (s.binding_statement.has_type)
                {
                    *out = s.binding_statement.variable_type;
                    return 1;
                }
                
                struct type inferred = {0};
                if (infer_type(&s.binding_statement.value, scope, &inferred))
                {
                    *out = inferred;
                    return 1;
                }
			}
		}
	}

    for (size_t i = 0; i < scope->scoped_variables.size; i++) {
        struct key_type_pair pair = scope->scoped_variables.data[i];
        if (list_char_eq(&pair.field_name, &variable_name)) {
            *out = *pair.field_type;
            return 1;
        }
    }

    if (scope->parent != NULL) {
        return get_scoped_variable_type(scope->parent, variable_name, out);
    }

	return 0;
}

int infer_type(struct expression *e, struct c_scope *scope, struct type *out) {
    switch (e->kind) {
        case LITERAL_EXPRESSION:
        {
            switch (e->literal.kind) {
                case LITERAL_BOOLEAN:
                {
                    *out = (struct type) {
                        .kind = TY_PRIMITIVE,
                        .primitive_type = BOOL
                    };
                    return 1;
                }
                case LITERAL_CHAR:
                {
                    *out = (struct type) {
                        .kind = TY_PRIMITIVE,
                        .primitive_type = U8
                    };
                    return 1;
                }
                case LITERAL_STRUCT:
                {
                    *out = (struct type) {
                        .kind = TY_STRUCT,
                        .name = e->literal.struct_enum.name,
                        .struct_type = (struct struct_type) {
                            .predefined = 1,
                        }
                    };
                    return 1;
                }
                case LITERAL_ENUM:
                {
                    *out = (struct type) {
                        .kind = TY_ENUM,
                        .name = e->literal.struct_enum.name,
                        .struct_type = (struct struct_type) {
                            .predefined = 1,
                        }
                    };
                    return 1;
                }
                case LITERAL_STR:
                case LITERAL_NUMERIC:
                case LITERAL_HOLE:
                case LITERAL_NULL:
                    return 0;
                case LITERAL_NAME:
					return get_scoped_variable_type(scope, *e->literal.name, out);
            }
        }
        case UNARY_EXPRESSION:
		{
			return infer_type(e->unary.expression, scope, out);
		}
        case BINARY_EXPRESSION:
        {
            if (e->binary.binary_op == DOT_BINARY)
            {
                struct type l_type = {0};
                if (infer_type(e->binary.l, scope, &l_type))
                {
                    if (l_type.kind == TY_STRUCT)
                    {
                        assert(e->binary.r->kind == LITERAL_EXPRESSION);
                        // find type of a field with the name above
                    }
                }
            }
        }
        case GROUP_EXPRESSION:
        case FUNCTION_EXPRESSION:
            return 0;
    }
}


// TODO
int expression_is_pointer(struct expression *e, struct c_scope *scope) {
    switch (e->kind) {
        case LITERAL_EXPRESSION:
        {
            if (e->literal.kind == LITERAL_NAME) {
                struct type variable_type = {0};
                if (get_scoped_variable_type(scope, *e->literal.name, &variable_type)) {
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
            break;
    }

    return 0;
}

void write_unary_expression(struct unary_expression *e, struct c_scope *scope, FILE *file) {
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

    write_expression(e->expression, scope, file);
}

void write_binary_expression(struct binary_expression *e, struct c_scope *scope, FILE *file) {
    write_expression(e->l, scope, file);
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
        case DOT_BINARY:
        {
            if (expression_is_pointer(e->l, scope)) {
                fprintf(file, "->");
            } else {
                fprintf(file, ".");
            }
            break;
        }
        default:
            UNREACHABLE("binary operator not handled");
    }
    write_expression(e->r, scope, file);
}

void write_grouped_expression(struct expression *e, struct c_scope *scope, FILE *file) {
    fprintf(file, "(");
    write_expression(e, scope, file);
    fprintf(file, ")");
}

void write_function_expression(struct function_expression *e, struct c_scope *scope, FILE *file) {
    fprintf(file, "%s(", e->function_name->data);
    size_t param_count = e->params->size;
    for (size_t i = 0; i < param_count; i++) {
        write_expression(&e->params->data[i], scope, file);
        if (i < param_count - 1) {
            fprintf(file, ", ");
        }
    }
    fprintf(file, ")");
}

void write_expression(struct expression *e, struct c_scope *scope, FILE *file) {
    switch (e->kind) {
        case LITERAL_EXPRESSION:
            write_literal_expression(&e->literal, scope, file);
            return;
        case UNARY_EXPRESSION:
            write_unary_expression(&e->unary, scope, file);
            return;
        case BINARY_EXPRESSION:
            write_binary_expression(&e->binary, scope, file);
            return;
        case GROUP_EXPRESSION:
            write_grouped_expression(e->grouped, scope, file);
            return;
        case FUNCTION_EXPRESSION:
            write_function_expression(&e->function, scope, file);
            return;
        default:
            UNREACHABLE("expression kind not handled");
    }
}

void write_statement(struct statement *s, struct c_scope *scope, FILE *file);

void write_type_default(struct type *type, FILE *file) {
    // TODO: derive default C values 
    fprintf(file, "0");
}

int variable_is_defined(struct c_scope *scope, struct list_char variable_name)
{
	for (size_t i = 0; i < scope->preceding_statements.len; i++) {
		struct statement s = scope->preceding_statements.statements[i];
		if (s.kind == BINDING_STATEMENT) {
			if (list_char_eq(&s.binding_statement.variable_name, &variable_name)) {
				return 1;
			}
		}
	}

	return 0;
}

void write_binding_statement(struct binding_statement *s, struct c_scope *scope, FILE *file) {
	if (!variable_is_defined(scope, s->variable_name)) {
		if (*&s->has_type) {
			write_type(&s->variable_type, file);
		} else {
			struct type inferred = {0};
			if (infer_type(&s->value, scope, &inferred)) {
				write_type(&inferred, file);
			}
		}
	}

    fprintf(file, " %s = ", s->variable_name.data);
    if (s->value.kind == LITERAL_EXPRESSION
        && s->value.literal.kind == LITERAL_NULL) {
        // TODO: check has_type
        write_type_default(&s->variable_type, file);
    } else {
        write_expression(&s->value, scope, file);
    }
    fprintf(file, ";");
}

void write_if_statement(struct if_statement *s, struct c_scope *scope, FILE *file) {
    fprintf(file, "if (");
    write_expression(&s->condition, scope, file);
    fprintf(file, ")");
    write_statement(s->success_statement, scope, file);
    if (s->else_statement != NULL) {
        fprintf(file, " else ");
        write_statement(s->else_statement, scope, file);
    }
}

void write_return_statement(struct expression *e, struct c_scope *scope, FILE *file) {
    fprintf(file, "return ");
    write_expression(e, scope, file);
    fprintf(file, ";");
}

struct_list(int);

void write_block_statement(struct list_statement *statements, struct c_scope *scope, FILE *file) {
    fprintf(file, "{");
    for (size_t i = 0; i < statements->size; i++) {
		struct c_scope inner_scope = (struct c_scope) {
			.preceding_statements = (struct statement_slice) {
				.statements = statements->data,
				.len = i
			},
			.parent = scope
		};
        write_statement(&statements->data[i], &inner_scope, file);
    }
    fprintf(file, "}");
}

void write_action_statement(struct expression *e, struct c_scope *scope, FILE *file) {
    write_expression(e, scope, file);
    fprintf(file, ";");
}

void write_while_statement(struct while_loop_statement *s, struct c_scope *scope, FILE *file) {
    fprintf(file, "while (");
    write_expression(&s->condition, scope, file);
    fprintf(file, ")");
    write_statement(s->do_statement, scope, file);
}

void write_type_declaration_statement(struct type_declaration_statement *s,
                                      struct c_scope *scope,
                                      FILE *file)
{
    write_type(&s->type, file);
    if (s->type.kind == TY_FUNCTION)
    {
		assert(s->statements != NULL);
        struct c_scope inner_scope = (struct c_scope) {
            .preceding_statements = (struct statement_slice) {
                .statements = NULL,
                .len = 0
            },
            .parent = scope,
            .scoped_variables = s->type.function_type.params
        };
        
        write_block_statement(s->statements, &inner_scope, file);
    }
}

void write_break_statement(struct c_scope *scope, FILE *file) {
    fprintf(file, "break;");
}

void write_include_statement(struct include_statement *s, struct c_scope *scope, FILE *file) {
    fprintf(file, "#include");
    if (s->external) {
        fprintf(file, " <");
    } else {
        fprintf(file, " \"");
    }
    fprintf(file, "%s", s->include.data);
    if (s->external) {
        fprintf(file, ">");
    } else {
        fprintf(file, "\"");
    }
    fprintf(file, "\n");
}

void write_case_predicate(struct switch_pattern *p,
                          const char *switch_name,
                          struct c_scope *scope,
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
                          struct c_scope *scope,
                          FILE *file)
{
    write_case_predicate(&s->pattern, switch_name, scope, file);
    fprintf(file, "{");
    write_statement(s->statement, scope, file);
    fprintf(file, "}");
}

void write_switch_statement(struct switch_statement *s, struct c_scope *scope, FILE *file) {
    struct type inferred_type = {0};
    if (infer_type(&s->switch_expression, scope, &inferred_type)) {
        write_type(&inferred_type, file);
        fprintf(file, " *t = &");
        write_expression(&s->switch_expression, scope, file);
        fprintf(file, ";");
    }

    for (size_t i = 0; i < s->cases.size; i++) {
        write_case_statement(&s->cases.data[i], "t", scope, file);
    }
}

void write_statement(struct statement *s, struct c_scope *scope, FILE *file) {
    switch (s->kind) {
        case BINDING_STATEMENT:
            write_binding_statement(&s->binding_statement, scope, file);
            break;
        case IF_STATEMENT:
            write_if_statement(&s->if_statement, scope, file);
            break;
        case RETURN_STATEMENT:
            write_return_statement(&s->expression, scope, file);
            break;
        case BLOCK_STATEMENT:
            write_block_statement(s->statements, scope, file);
            break;
        case ACTION_STATEMENT:
            write_action_statement(&s->expression, scope, file);
            break;
        case WHILE_LOOP_STATEMENT:
            write_while_statement(&s->while_loop_statement, scope, file);
            break;
        case TYPE_DECLARATION_STATEMENT:
            write_type_declaration_statement(&s->type_declaration, scope, file);
            break;
        case BREAK_STATEMENT:
            write_break_statement(scope, file);
            break;
        case INCLUDE_STATEMENT:
            write_include_statement(&s->include_statement, scope, file);
            break;
        case SWITCH_STATEMENT:
            write_switch_statement(&s->switch_statement, scope, file);
            break;
        default:
            UNREACHABLE("statement type not handled");
    }
}

struct generated_c_state {
    struct list_type *data_types;
    struct list_type *fn_types;
};

static void generate_c_file(struct rm_file file, struct generated_c_state *state) {
    FILE *program = fopen("target/c_output.c", "w");
    fprintf(program, "#include \"c_output.h\"\n");
    for (size_t i = 0; i < file.statements.size; i++) {
        struct statement s = file.statements.data[i];
		struct c_scope scope = {0};

		switch (s.kind) {
			case INCLUDE_STATEMENT:
        		write_statement(&file.statements.data[i], &scope, program);
				break;
			case TYPE_DECLARATION_STATEMENT:
			{
				switch (s.type_declaration.type.kind) {
					case TY_ENUM:
					case TY_STRUCT:
						list_append(state->data_types, s.type_declaration.type);
						break;
					case TY_FUNCTION:
						list_append(state->fn_types, s.type_declaration.type);
        				write_statement(&file.statements.data[i], &scope, program);
						break;
					case TY_PRIMITIVE:
					{
						fprintf(stderr, "woopssss");
						exit(-1);
					}
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
			{
				fprintf(stderr, "woops");
				exit(-1);
			}
        } 
    }
}

void generate_c_header(struct generated_c_state *state) {
    FILE *header = fopen("target/c_output.h", "w");
    fprintf(header, "#ifndef C_OUTPUT_H\n#define C_OUTPUT_H\n");

    for (size_t i = 0; i < state->data_types->size; i++) {
        write_type(&state->data_types->data[i], header);
    }

    for (size_t i = 0; i < state->fn_types->size; i++) {
        write_type(&state->fn_types->data[i], header);
        fprintf(header, ";");
    }
    
    fprintf(header, "\n#endif");
}

void generate_c(struct rm_file file) {
    struct list_type data_types = list_create(type, 100);
    struct list_type fn_types = list_create(type, 100);
    struct generated_c_state state = {
        .data_types = &data_types,
        .fn_types = &fn_types
    };
    generate_c_file(file, &state);
    generate_c_header(&state);
}
