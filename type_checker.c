#include <assert.h>
#include <string.h>
#include "ast.h"
#include "context.h"
#include "utils.h"
#include <string.h>
#include "type_checker.h"
#include "error.h"

// struct list_char show_type(struct type *ty);
// int type_check_single(struct statement_context *s, struct error *error);
// int type_eq(struct type *l, struct type *r);
//
// static void add_error_inner(struct statement_metadata *metadata,
//                             char *error_message,
//                             struct error *out)
// {
//     add_error(metadata->row, metadata->col, metadata->file_name, out, error_message);
// }
//
// struct list_char type_mismatch_generic_error(struct type *expected, struct type *actual)
// {
//     struct list_char output = list_create(char, 50);
//     append_list_char_slice(&output, "mismatch types; expected `");
//     append_list_char_slice(&output, show_type(expected).data);
//     append_list_char_slice(&output, "` but got `");
//     append_list_char_slice(&output, show_type(actual).data);
//     append_list_char_slice(&output, "`.");
//     return output;
// }
//
// int fn_type_eq(struct function_type *l, struct function_type *r) {
//     if (l->params.size != r->params.size) {
//         return 0;
//     }
//     
//     if (!type_eq(l->return_type, r->return_type)) {
//         return 0;
//     }
//         
//     assert(l->params.size == r->params.size);
//     for (size_t i = 0; i < l->params.size; i++) {
//         if (!type_eq(l->params.data[i].field_type, r->params.data[i].field_type)) return 0;
//     }
//
//     return 1;
// }
//
// int type_modifier_eq(struct type_modifier *l, struct type_modifier *r)
// {
//     if (l->kind != r->kind) {
//         return 0;
//     }
//     
//     switch (l->kind) {
//         case ARRAY_MODIFIER_KIND:
//         {
//             // TODO: reference based size
//             if (l->array_modifier.literally_sized && r->array_modifier.literally_sized
//                 && l->array_modifier.literal_size != r->array_modifier.literal_size)
//             {
//                 return 0;
//             }
//             return 1;
//         }
//         case POINTER_MODIFIER_KIND:
//         case NULLABLE_MODIFIER_KIND:
//         case MUTABLE_MODIFIER_KIND:
//             return 1;
//     }
//     
//     UNREACHABLE("type_modifier_eq fell out of switch.");
// }
//
// int type_eq(struct type *l, struct type *r) {
//     assert(l);
//     assert(r);
//     assert(l->kind);
//     assert(r->kind);
//
//     if (l->kind != r->kind) {
//         return 0;
//     }
//     
//     if (l->modifiers.size != r->modifiers.size) {
//         return 0;
//     }
//     
//     for (size_t i = 0; i < l->modifiers.size; i++) {
//         if (!type_modifier_eq(&l->modifiers.data[i], &r->modifiers.data[i])) return 0;
//     }
//     
//     switch (l->kind) {
//         case TY_PRIMITIVE:
//             return l->primitive_type == r->primitive_type;
//         case TY_STRUCT:
//             return list_char_eq(l->name, r->name);
//         case TY_FUNCTION:
//             return fn_type_eq(&l->function_type, &r->function_type);
//         case TY_ENUM:
//             return list_char_eq(l->name, r->name);
//     }
//
//     return 0;
// }
//
// int is_boolean(struct type *ty)
// {
//     switch (ty->kind) {
//         case TY_PRIMITIVE:
//         {
//             return ty->primitive_type == BOOL;
//         }
//         default:
//             return 0;
//     }
// }
//
// int binding_statement_check(struct statement_context *s, struct error *error)
// {
//     assert(s->kind == BINDING_STATEMENT);
//
//     if (s->binding_statement.binding_statement->has_type
//         && !type_eq(&s->binding_statement.value_type,
//                     &s->binding_statement.binding_statement->variable_type))
//     {
//         add_error_inner(s->metadata,
//                         type_mismatch_generic_error(&s->binding_statement.binding_statement->variable_type, 
//                                                     &s->binding_statement.value_type).data,
//                         error);
//         return 0;
//     }
//
//     return 1;
// }
//
// void all_return_statements_inner(struct statement_context *s_ctx, struct list_statement_context *out)
// {
//     assert(s_ctx != NULL);
//     switch (s_ctx->kind) {
//         case RETURN_STATEMENT:
//         {
//             list_append(out, *s_ctx);
//             return;
//         }
//         case IF_STATEMENT:
//         {
//             all_return_statements_inner(s_ctx->if_statement_context.success_statement, out);
//             if (s_ctx->if_statement_context.else_statement != NULL)  {
//                 all_return_statements_inner(s_ctx->if_statement_context.else_statement, out);
//             }
//             return;
//         }
//         case BLOCK_STATEMENT:
//         {
//             for (size_t i = 0; i < s_ctx->block_statements->size; i++) {
//                 all_return_statements_inner(&s_ctx->block_statements->data[i], out);
//             }
//             return;
//         }
//         case WHILE_LOOP_STATEMENT:
//         {
//             all_return_statements_inner(s_ctx->while_loop_statement.do_statement, out);
//             return;
//         }
//         case SWITCH_STATEMENT:
//         {
//             TODO("switch_statement is not part of the context yet.");
//             return;
//         }
//         // cases to ignore
//         case BINDING_STATEMENT:
//         case ACTION_STATEMENT:
//         case TYPE_DECLARATION_STATEMENT:
//         case BREAK_STATEMENT:
//         case C_BLOCK_STATEMENT:
//             return;
//     }
//
//     UNREACHABLE("`all_return_statements_inner` fell out of it's switch.");
// }
//
// struct list_statement_context all_return_statements(struct statement_context *s_ctx)
// {
//     struct list_statement_context output = list_create(statement_context, 10);
//     all_return_statements_inner(s_ctx, &output);
//     return output;
// }
//
// int find_function_definition(struct list_char *function_name,
//                              struct global_context *global_context,
//                              struct type *out)
// {
//     for (size_t i = 0; i < global_context->fn_types.size; i++) {
//         if (list_char_eq(function_name, global_context->fn_types.data[i].name)) {
//             *out = global_context->fn_types.data[i];
//             return 1;
//         }
//     }
//     
//     UNREACHABLE("all functions should exist in the global context by this point.");
// }
//
// int type_check_action_statement(struct statement_context *s, struct error *error)
// {
//     assert(s->kind == ACTION_STATEMENT);
//     if (s->expression.expression->kind != FUNCTION_EXPRESSION) {
//         return 1;
//     }
//
//     struct function_expression *fn_expr = &s->expression.expression->function;
//     struct type fn = {0};
//     if (!find_function_definition(fn_expr->function_name, s->global_context, &fn)) return 0;
//
//     // assumption: fn's list of name:type is ordered how it's defined in the source code,
//     // and the params list of expressions is ordered how it's written in the source code.
//     assert(fn_expr->params->size <= fn.function_type.params.size);
//     for (size_t i = 0; i < fn_expr->params->size; i++) {
//         // TODO: this expression should already have a type attached.
//         struct expression *param_expr = &fn_expr->params->data[i];
//         struct type *expected = fn.function_type.params.data[i].field_type;
//
//         // if (!type_eq(actual, expected)) {
//         //     append_list_char_slice(&error_message, "mismatch types; expected `");
//         //     append_list_char_slice(&error_message, show_type(expected).data);
//         //     append_list_char_slice(&error_message, "` for parameter '");
//         //     append_list_char_slice(&error_message, fn.function_type.params.data[i].field_name.data);
//         //     append_list_char_slice(&error_message, "' but got `");
//         //     append_list_char_slice(&error_message, show_type(actual).data);
//         //     append_list_char_slice(&error_message, "` (in function '");
//         //     append_list_char_slice(&error_message, fn.name->data);
//         //     append_list_char_slice(&error_message, "').");
//         //     add_error_inner(s->metadata, error_message.data, error);
//         //     return 0;
//         // }
//     }
//     return 1;
// }
//
// int type_check_single(struct statement_context *s, struct error *error)
// {
//     switch (s->kind) {
//         case BINDING_STATEMENT:
//             return binding_statement_check(s, error);
//         case TYPE_DECLARATION_STATEMENT:
//         {
//             if (s->type_declaration.type.kind != TY_FUNCTION) return 1;
//             struct type *expected_return_type = s->type_declaration.type.function_type.return_type;
//             struct list_statement_context *body = s->type_declaration.statements;
//
//             for (size_t i = 0; i < body->size; i++) {
//                 struct list_statement_context return_statements = all_return_statements(&body->data[i]);
//                 if (return_statements.size) {
//                     struct list_char error_message = list_create(char, 100);
//                     for (size_t j = 0; j < return_statements.size; j++) {
//                         struct statement_context *this = &return_statements.data[j];
//                         assert(this->kind == RETURN_STATEMENT);
//                         struct type actual = {0};
//                         if (!infer_expression_type(this->expression,
//                                                    this->global_context,
//                                                    &this->scoped_variables,
//                                                    &actual,
//                                                    &error_message))
//                         {
//                             // TODO: add error
//                             return 0;
//                         }
//
//                         if (!type_eq(expected_return_type, &actual)) {
//                             add_error_inner(this->metadata,
//                                             type_mismatch_generic_error(expected_return_type, &actual).data,
//                                             error);
//                             return 0;
//                         }
//                     }
//                 } else {
//                     if (!type_check_single(&s->type_declaration.statements->data[i], error)) return 0;
//                 }
//             }
//             return 1;
//         }
//         case IF_STATEMENT:
//         {
//             // struct if_statement_context *ctx = &s->if_statement_context;
//             // if (!is_boolean(&ctx->condition.type))
//             // {
//             //     add_error_inner(s->metadata, "the condition of an if statement must be a boolean.", error);
//             //     return 0;
//             // }
//             // 
//             // if (!type_check_single(ctx->success_statement, error)) return 0;
//             // if (ctx->else_statement != NULL
//             //     && !type_check_single(ctx->else_statement, error)) return 0;
//             
//             return 1;
//         }
//         case WHILE_LOOP_STATEMENT:
//         {
//             // struct while_loop_statement_context *ctx = &s->while_loop_statement;
//             // if (!is_boolean(&ctx->condition.type))
//             // {
//             //     add_error_inner(s->metadata, "the condition of a while loop must be a boolean.", error);
//             //     return 0;
//             // }
//             // 
//             // if (!type_check_single(ctx->do_statement, error)) return 0;
//             return 1;
//         }
//         case BLOCK_STATEMENT:
//         {
//             for (size_t i = 0; i < s->block_statements->size; i++) {
//                 if (!type_check_single(&s->block_statements->data[i], error)) return 0;
//             }
//             return 1;
//         }
//         case ACTION_STATEMENT:
//             return type_check_action_statement(s, error);
//         case SWITCH_STATEMENT:
//         {
//             TODO("switch statement type check");
//         }
//         case RETURN_STATEMENT:
//         case BREAK_STATEMENT:
//         case C_BLOCK_STATEMENT:
//             return 1;
//     }
//
//     UNREACHABLE("type_check_single dropped out of a switch on all kinds of statements.");
// }
//
// int type_check(struct rm_program *program, struct error *error) {
//     struct list_statement_context statements = program->statements;
//     for (size_t i = 0; i < statements.size; i++) {
//         if (!type_check_single(&statements.data[i], error)) return 0;
//     }
//     return 1;
// }
//
// struct list_char show_modifier(struct type_modifier *m)
// {
//     struct list_char output = list_create(char, 10);
//     switch (m->kind) {
//         case POINTER_MODIFIER_KIND:
//         {
//             append_list_char_slice(&output, "*");
//             break; 
//         }
//         case NULLABLE_MODIFIER_KIND:
//         {
//             append_list_char_slice(&output, "?");
//             break; 
//         }
//         case MUTABLE_MODIFIER_KIND:
//         {
//             append_list_char_slice(&output, "mut ");
//             break; 
//         }
//         case ARRAY_MODIFIER_KIND:
//         {
//             append_list_char_slice(&output, "[");
//             if (m->array_modifier.literally_sized) {
//                 char tmp[1024] = {0}; // TODO don't do this...
//                 sprintf(tmp, "%d", m->array_modifier.literal_size);
//                 append_list_char_slice(&output, tmp);
//             } else if (m->array_modifier.reference_sized) {
//                 append_list_char_slice(&output, m->array_modifier.reference_name->data);
//             }
//             append_list_char_slice(&output, "]");
//             break; 
//         }
//     }
//
//     return output;
// }
//
// struct list_char show_type(struct type *ty) {
//     struct list_char output = list_create(char, 10);
//     if (ty == NULL) {
//         return output;
//     }
//
//     for (size_t i = 0; i < ty->modifiers.size; i++) {
//         append_list_char_slice(&output, show_modifier(&ty->modifiers.data[i]).data);
//     }
//
//     switch (ty->kind) {
//         case TY_PRIMITIVE:
//         {
//             switch (ty->primitive_type) {
//                 case VOID:
//                     append_list_char_slice(&output, "void");
//                     break;
//                 case BOOL:
//                     append_list_char_slice(&output, "bool");
//                     break;
//                 case U8:
//                     append_list_char_slice(&output, "u8");
//                     break;
//                 case I8:
//                     append_list_char_slice(&output, "i8");
//                     break;
//                 case I16:
//                     append_list_char_slice(&output, "i16");
//                     break;
//                 case U16:
//                     append_list_char_slice(&output, "u16");
//                     break;
//                 case I32:
//                     append_list_char_slice(&output, "i32");
//                     break;
//                 case U32:
//                     append_list_char_slice(&output, "u32");
//                     break;
//                 case I64:
//                     append_list_char_slice(&output, "i64");
//                     break;
//                 case U64:
//                     append_list_char_slice(&output, "u64");
//                     break;
//                 case USIZE:
//                     append_list_char_slice(&output, "usize");
//                     break;
//                 case F32:
//                     append_list_char_slice(&output, "f32");
//                     break;
//                 case F64:
//                     append_list_char_slice(&output, "f64");
//                     break;
//             }
//             break;
//         }
//         case TY_STRUCT:
//         {
//             append_list_char_slice(&output, "struct ");
//             append_list_char_slice(&output, ty->name->data);
//             break;
//         }
//         case TY_ENUM:
//         {
//             append_list_char_slice(&output, "enum ");
//             append_list_char_slice(&output, ty->name->data);
//             break;
//         }
//         case TY_FUNCTION:
//         {
//             append_list_char_slice(&output, "fn(");
//             for (size_t i = 0; i < ty->function_type.params.size; i++) {
//                 struct list_char param_type = show_type(ty->function_type.params.data[i].field_type);
//                 append_list_char_slice(&output, param_type.data);
//                 if (i < ty->function_type.params.size - 1) {
//                     append_list_char_slice(&output, ", ");
//                 }
//             }
//             append_list_char_slice(&output, ") -> ");
//             struct list_char return_type = show_type(ty->function_type.return_type);
//             append_list_char_slice(&output, return_type.data);
//             break;
//         }
//     }
//
//     append_list_char_slice(&output, "\0");
//     return output;
// }
