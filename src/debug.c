#include "context.h"
#include <stdio.h>

// WIP

static char *type_kind_to_str(enum type_kind kind)
{
    switch (kind) {
    case TY_PRIMITIVE:
        return "primitive";
    case TY_STRUCT:
        return "struct";
    case TY_FUNCTION:
        return "function";
    case TY_ENUM:
        return "enum";
    case TY_ANY:
        return "any";
    }
}

static char *type_value_to_str(struct type *ty)
{
    return "hello";
}

static char *type_name_to_str(struct list_char *name)
{
    if (name == NULL) {
        return "none";
    }
    return name->data;
}

static char *type_modifier_to_str(struct type_modifier modifier)
{
    return "mod";
}

static char *type_modifiers_to_str(struct list_type_modifier *modifiers)
{
    struct list_char output = list_create(char, 10);
    list_append(&output, '[');
    for (size_t i = 0; i < modifiers->size; ++i) {
        char *m = type_modifier_to_str(modifiers->data[i]);
        append_list_char_slice(&output, m);
        append_list_char_slice(&output, " - ");
    }
    list_append(&output, ']');
    return output.data;
}

static void show_type(struct type *ty)
{
    printf("( ");
    printf("kind: %s, ", type_kind_to_str(ty->kind));
    printf("value: %s, ", type_value_to_str(ty));
    printf("name: %s, ", type_name_to_str(ty->name));
    printf("modifiers: %s ", type_modifiers_to_str(&ty->modifiers));
    printf(")");
}

void show_context(struct context *ctx)
{
    for (size_t i = 0; i < ctx->expression_type_lookup.keys->size; i++) {
        printf("expression id: %d", ctx->expression_type_lookup.keys->data[i]);
        printf(" type: ");
        show_type(&ctx->expression_type_lookup.data[i]);
        printf("\n");
    }
}
