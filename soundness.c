#include "soundness.h"
#include "ast.h"
#include "context.h"
#include "error.h"
#include <assert.h>

// static void add_error_inner(struct statement_metadata *metadata,
//                             char *error_message,
//                             struct error *out)
// {
//     add_error(metadata->row, metadata->col, metadata->file_name, out, error_message);
// }
//
// int check_statement_soundness(struct statement_context *ctx, struct error *error);
//
// int check_expression_soundness(struct expression *e,
//                                struct global_context *global_context,
//                                struct list_scoped_variable *scoped_variables,
//                                struct list_char *error);
//
// int check_literal_expression_soundness(struct literal_expression *e,
//                                        struct global_context *global_context,
//                                        struct list_scoped_variable *scoped_variables,
//                                        struct list_char *error)
// {
//     switch (e->kind) {
//         case LITERAL_NAME:
//         {
//             for (size_t i = 0; i < scoped_variables->size; i++) {
//                 struct scoped_variable *var = &scoped_variables->data[i];
//                 if (list_char_eq(&var->name, e->name)) {
//                     return 1;
//                 }
//             }
//             append_list_char_slice(error, "`");
//             append_list_char_slice(error, e->name->data);
//             append_list_char_slice(error, "` is not in the current scope.");
//             return 0;
//         }
//         case LITERAL_STRUCT:
//         case LITERAL_ENUM:
//         {
//             struct list_char *name = e->struct_enum.name;
//             for (size_t i = 0; i < global_context->data_types.size; i++) {
//                 struct type *data_type = &global_context->data_types.data[i];
//                 if (list_char_eq(data_type->name, name)) {
//                     struct list_key_type_pair *pairs = NULL;
//                     if (data_type->kind != TY_ENUM) {
//                         pairs = &data_type->enum_type.pairs;
//                     } else if (data_type->kind != TY_STRUCT) {
//                         pairs = &data_type->struct_type.pairs;
//                     } else {
//                         return 0;
//                     }
//                     assert(pairs);
//                     
//                     for (size_t p = 0; p < pairs->size; p++) {
//                         int found = 0;
//                         for (size_t l = 0; l < e->struct_enum.key_expr_pairs.size; l++) {
//                             struct key_expression *literal_pair = &e->struct_enum.key_expr_pairs.data[l];
//                             if (list_char_eq(literal_pair->key, &pairs->data[p].field_name)) {
//                                 found = 1;
//                                 if (!check_expression_soundness(literal_pair->expression,
//                                                                 global_context,
//                                                                 scoped_variables,
//                                                                 error))
//                                 {
//                                     return 0;
//                                 }
//                                 break;
//                             }
//                         }
//
//                         if (!found) {
//                             append_list_char_slice(error, "required field `");
//                             append_list_char_slice(error, pairs->data[p].field_name.data);
//                             append_list_char_slice(error, "` is missing.");
//                             return 0;
//                         }
//                         // TODO: check duplicate fields
//                     }
//                     
//                     return 1;
//                 }
//             }
//             return 0;
//         }
//         case LITERAL_HOLE:
//         case LITERAL_NULL:
//         case LITERAL_BOOLEAN:
//         case LITERAL_CHAR:
//         case LITERAL_STR:
//         case LITERAL_NUMERIC:
//             return 1;
//     }
//
//     UNREACHABLE("dropped out of check_literal_expression_soundnes switch.");
// }
//
// int expression_is_literal_name(struct expression *e)
// {
//     return e->kind == LITERAL_EXPRESSION && e->literal.kind == LITERAL_NAME;
// }
//
// int check_binary_expression_soundness(struct binary_expression *e,
//                                       struct global_context *global_context,
//                                       struct list_scoped_variable *scoped_variables,
//                                       struct list_char *error)
// {
//     return 1;
// }
//
// int check_expression_soundness(struct expression *e,
//                                struct global_context *global_context,
//                                struct list_scoped_variable *scoped_variables,
//                                struct list_char *error)
// {
//     switch (e->kind) {
//         case UNARY_EXPRESSION:
//             return check_expression_soundness(e->unary.expression, global_context, scoped_variables, error);
//         case LITERAL_EXPRESSION:
//             return check_literal_expression_soundness(&e->literal, global_context, scoped_variables, error);
//         case GROUP_EXPRESSION:
//             return check_expression_soundness(e->grouped, global_context, scoped_variables, error);
//         case BINARY_EXPRESSION:
//             return check_binary_expression_soundness(&e->binary, global_context, scoped_variables, error);
//         case FUNCTION_EXPRESSION:
//         case VOID_EXPRESSION:
//         case MEMBER_ACCESS_EXPRESSION:
//             return 1;
//     }
//
//     return 1;
// }
//
// int check_struct_soundness(struct type *type,
//                            struct global_context *global_context,
//                            struct list_char *error)
// {
//     assert(type->kind == TY_STRUCT);
//     int struct_count = 0;
//     for (size_t i = 0; i < global_context->data_types.size; i++) {
//         if (list_char_eq(type->name, global_context->data_types.data[i].name)) {
//             struct_count += 1;
//             if (struct_count > 1) {
//                 append_list_char_slice(error, "`struct ");
//                 append_list_char_slice(error, type->name->data);
//                 append_list_char_slice(error, "` already exists.");
//                 return 0;
//             }
//         }
//     }
//     
//     struct list_string visited = list_create(string, 10);
//     struct list_key_type_pair pairs = type->struct_type.pairs;
//     for (size_t i = 0; i < pairs.size; i++) {
//         struct string field_name = (struct string) {
//             .name = pairs.data[i].field_name
//         };
//         
//         for (size_t j = 0; j < visited.size; j++) {
//             if (list_char_eq(&visited.data[j].name, &field_name.name)) {
//                 append_list_char_slice(error, "field `");
//                 append_list_char_slice(error, field_name.name.data);
//                 append_list_char_slice(error, "` already exists on struct.");
//                 return 0;
//             }
//         }
//
//         list_append(&visited, field_name);
//     }
//     
//     for (size_t i = 0; i < pairs.size; i++) {
//         struct type *ty = pairs.data[i].field_type;
//         for (size_t m = 0; m < ty->modifiers.size; m++) {
//             struct type_modifier *modifier = &ty->modifiers.data[m];
//             if (modifier->kind == ARRAY_MODIFIER_KIND
//                 && modifier->array_modifier.reference_sized)
//             {
//                 struct list_char *ref_name = modifier->array_modifier.reference_name;
//                 int found = 0;
//                 for (size_t p = 0; p < pairs.size; p++) {
//                     if (list_char_eq(&pairs.data[p].field_name, modifier->array_modifier.reference_name)) {
//                         struct type *found_type = pairs.data[p].field_type;
//                         if (found_type->kind == TY_PRIMITIVE && found_type->primitive_type == USIZE) {
//                             found = 1;
//                         } else {
//                             append_list_char_slice(error, "`");
//                             append_list_char_slice(error, ref_name->data);
//                             append_list_char_slice(error, "` must be bound to a field of type `usize`");
//                             return 0;
//                         }
//                     }
//                 }
//                 
//                 if (!found) {
//                     append_list_char_slice(error, "`");
//                     append_list_char_slice(error, ref_name->data);
//                     append_list_char_slice(error, "` is unbounded within `");
//                     append_list_char_slice(error, type->name->data);
//                     append_list_char_slice(error, "`");
//                     return 0;
//                 }
//             }
//         }
//     }
//
//     return 1;
// }
//
// int check_enum_soundness(struct enum_type type,
//                          struct global_context *global_context,
//                          struct error *error)
// {
//     TODO("check_enum_soundness");
// }
//
// int check_fn_soundness(struct type_declaration_statement_context *type_declaration,
//                        struct error *error)
// {
//     assert(type_declaration->type.kind == TY_FUNCTION);
//     for (size_t i = 0; i < type_declaration->statements->size; i++) {
//         struct statement_context *this = &type_declaration->statements->data[i];
//         if (!check_statement_soundness(this, error)) return 0;
//     }
//     return 1;
// }
//
// int check_binding_statement_soundness(struct statement_context *ctx, struct error *error)
// {
//     assert(ctx->kind == BINDING_STATEMENT);
//     struct list_char error_message = list_create(char, 100);
//
//     for (size_t i = 0; i < ctx->scoped_variables.size; i++) {
//         struct list_char *binding_name = &ctx->binding_statement.variable_name;
//         if (list_char_eq(&ctx->scoped_variables.data[i].name, binding_name))
//         {
//             append_list_char_slice(&error_message, "`");
//             append_list_char_slice(&error_message, binding_name->data);
//             append_list_char_slice(&error_message, "` is already defined in this scope.");
//             add_error_inner(ctx->metadata, error_message.data, error);
//             return 0;
//         }
//     }
//
//     if (!check_expression_soundness(&ctx->binding_statement.value,
//                                     ctx->global_context,
//                                     &ctx->scoped_variables,
//                                     &error_message))
//     {
//         add_error_inner(ctx->metadata, error_message.data, error);
//         return 0;
//     }
//
//     return 1;
// }
//
// int check_if_statement_soundness(struct statement_context *ctx, struct error *error)
// {
//     assert(ctx->kind == IF_STATEMENT);
//     struct if_statement_context *if_ctx = &ctx->if_statement_context;
//     struct list_char error_message = list_create(char, 100);
//
//     if (!check_expression_soundness(if_ctx->condition,
//                                     ctx->global_context,
//                                     &ctx->scoped_variables,
//                                     &error_message))
//     {
//         add_error_inner(ctx->metadata, error_message.data, error);
//         return 0;
//     }
//     
//     if (!check_statement_soundness(if_ctx->success_statement, error)) return 0;
//     if (if_ctx->else_statement
//         && !check_statement_soundness(if_ctx->else_statement, error)) return 0;
//
//     return 1;
// }
//
// int check_return_statement_soundness(struct statement_context *ctx,
//                                      struct error *error)
// {
//     assert(ctx->kind == RETURN_STATEMENT);
//     struct list_char error_message = list_create(char, 100);
//
//     if (!check_expression_soundness(&ctx->expression,
//                                     ctx->global_context,
//                                     &ctx->scoped_variables,
//                                     &error_message))
//     {
//         add_error_inner(ctx->metadata, error_message.data, error);
//         return 0;
//     }
//     
//     return 1;
// }
//
// int check_block_statement_soundness(struct statement_context *ctx,
//                                     struct error *error)
// {
//     assert(ctx->kind == BLOCK_STATEMENT);
//     for (size_t i = 0; i < ctx->block_statements->size; i++) {
//         if (!check_statement_soundness(&ctx->block_statements->data[i], error)) return 0;
//     }
//     return 1;
// }
//
// int check_action_statement_soundness(struct statement_context *ctx,
//                                      struct error *error)
// {
//     assert(ctx->kind == ACTION_STATEMENT);
//     struct list_char error_message = list_create(char, 100);
//     if (!check_expression_soundness(&ctx->expression,
//                                     ctx->global_context,
//                                     &ctx->scoped_variables,
//                                     &error_message))
//     {
//         add_error_inner(ctx->metadata, error_message.data, error);
//         return 0;
//     }
//     return 1;
// }
//
// int check_while_statement_soundness(struct statement_context *ctx, struct error *error)
// {
//     assert(ctx->kind == WHILE_LOOP_STATEMENT);
//     struct while_loop_statement_context *while_ctx = &ctx->while_loop_statement;
//     struct list_char error_message = list_create(char, 100);
//
//     if (!check_expression_soundness(while_ctx->condition,
//                                     ctx->global_context,
//                                     &ctx->scoped_variables,
//                                     &error_message))
//     {
//         add_error_inner(ctx->metadata, error_message.data, error);
//         return 0;
//     }
//     
//     if (!check_statement_soundness(while_ctx->do_statement, error)) return 0;
//
//     return 1;
// }
//
// int check_statement_soundness(struct statement_context *ctx,
//                               struct error *error)
// {
//     switch (ctx->kind) {
//         case RETURN_STATEMENT:
//             return check_return_statement_soundness(ctx, error);
//         case BINDING_STATEMENT:
//             return check_binding_statement_soundness(ctx, error);
//         case IF_STATEMENT:
//             return check_if_statement_soundness(ctx, error);
//         case BLOCK_STATEMENT:
//             return check_block_statement_soundness(ctx, error);
//         case ACTION_STATEMENT:
//             return check_action_statement_soundness(ctx, error);
//         case WHILE_LOOP_STATEMENT:
//             return check_while_statement_soundness(ctx, error);
//         case BREAK_STATEMENT:
//         case SWITCH_STATEMENT:
//             return 0;
//         case C_BLOCK_STATEMENT:
//             return 1;
//         case TYPE_DECLARATION_STATEMENT:
//             UNREACHABLE("top level statements shouldn't be here.");
//         }
//
//     UNREACHABLE("dropped out of switch in check_statement_soundness.");
// }
//
//
// int soundness_check(struct rm_program *program, struct error *error)
// {
//     for (size_t i = 0; i < program->statements.size; i++) {
//         struct statement_context ctx = program->statements.data[i];
//         switch (ctx.kind) {
//             case TYPE_DECLARATION_STATEMENT:
//             {
//                 switch (ctx.type_declaration.type.kind) {
//                     case TY_FUNCTION:
//                     {
//                         if (!check_fn_soundness(&ctx.type_declaration, error)) return 0;
//                         break;
//                     }
//                     case TY_STRUCT:
//                     {
//                         struct list_char error_message = list_create(char, 100);
//                         if (!check_struct_soundness(&ctx.type_declaration.type,
//                                                     &program->global_context,
//                                                     &error_message))
//                         {
//                             add_error_inner(ctx.metadata, error_message.data, error);
//                             return 0;
//                         }
//                         break;
//                     }
//                     case TY_ENUM:
//                     {
//                         struct list_char error_message = list_create(char, 100);
//                         if (!check_enum_soundness(ctx.type_declaration.type.enum_type,
//                                                   &program->global_context,
//                                                   error))
//                         {
//                             add_error_inner(ctx.metadata, error_message.data, error);
//                             return 0;
//                         }
//                         break;
//                     }
//                     case TY_PRIMITIVE:
//                         UNREACHABLE("TY_PRIMITIVE shouldn't have made it here via parsing.");
//                 }
//                 break;
//             }
//             default:
//                 UNREACHABLE("statement shouldn't have made it here via parsing.");
//         }
//     }
//
//     return 1;
// }
