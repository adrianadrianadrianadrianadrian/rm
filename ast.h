#ifndef AST_H
#define AST_H

#include <math.h>
#include "utils.h"

enum primitive_type {
    VOID = 1,
    BOOL,
    U8,
    I8,
    I16,
    U16,
    I32,
    U32,
    I64,
    U64,
    USIZE,
    F32,
    F64,
    IO
};

enum type_modifier_kind {
    POINTER_MODIFIER_KIND = 1,
    NULLABLE_MODIFIER_KIND,
    ARRAY_MODIFIER_KIND,
    MUTABLE_MODIFIER_KIND
};

struct array_type_modifier {
    int literally_sized;
    int literal_size;
    int reference_sized;
    struct list_char *reference_name;
};

typedef struct type_modifier {
    enum type_modifier_kind kind;
    union {
        struct array_type_modifier array_modifier;
    };
} type_modifier;

struct_list(type_modifier);

enum type_kind {
    TY_PRIMITIVE = 1,
    TY_STRUCT,
    TY_FUNCTION,
    TY_ENUM
};

typedef struct key_type_pair {
    struct list_char field_name;
    struct type *field_type;
} key_type_pair;

struct_list(key_type_pair);

struct function_type {
    struct list_key_type_pair params;
    struct type *return_type;
};
    
struct struct_type {
    struct list_key_type_pair pairs;
    int predefined;
};

struct enum_type {
    struct list_key_type_pair pairs;
    int predefined;
};

typedef struct type {
    enum type_kind kind;
    struct list_char *name;
    struct list_type_modifier modifiers;
    union {
        struct function_type function_type;
        enum primitive_type primitive_type;
        struct struct_type struct_type;
        struct enum_type enum_type;
    };
} type;

struct_list(type);

enum expression_kind {
    LITERAL_EXPRESSION = 1,
    UNARY_EXPRESSION,
    BINARY_EXPRESSION,
    GROUP_EXPRESSION,
    FUNCTION_EXPRESSION,
    VOID_EXPRESSION,
    MEMBER_ACCESS_EXPRESSION
};

enum literal_expression_kind {
    LITERAL_BOOLEAN = 1,
    LITERAL_CHAR,
    LITERAL_STR,
    LITERAL_NUMERIC,
    LITERAL_NAME,
    LITERAL_HOLE,
    LITERAL_STRUCT,
    LITERAL_ENUM,
    LITERAL_NULL
};

typedef struct key_expression {
    struct list_char *key;
    struct expression *expression;
} key_expression;

struct_list(key_expression);
    
struct literal_struct_enum {
    struct list_char *name;
    struct list_key_expression key_expr_pairs;
};

struct literal_expression {
    enum literal_expression_kind kind;
    union {
        int boolean;
        char character;
        struct list_char *str;
        double numeric;
        struct list_char *name;
        struct literal_struct_enum struct_enum;
    };
};

enum binary_operator {
    PLUS_BINARY = 1,
    MINUS_BINARY,
    OR_BINARY,
    AND_BINARY,
    BITWISE_OR_BINARY,
    BITWISE_AND_BINARY,
    GREATER_THAN_BINARY,
    LESS_THAN_BINARY,
    EQUAL_TO_BINARY,
    MULTIPLY_BINARY
};

struct binary_expression {
    enum binary_operator binary_op;
    struct expression *l;
    struct expression *r;
};

enum unary_operator {
    BANG_UNARY = 1,
    STAR_UNARY,
    MINUS_UNARY
};

struct unary_expression {
    enum unary_operator unary_operator;
    struct expression *expression;
};

struct function_expression {
    struct list_char *function_name;
    struct list_expression *params;
};

struct member_access_expression {
    struct expression *accessed;
    struct list_char *member_name;
};

typedef struct expression {
    enum expression_kind kind;
    union {
        struct unary_expression unary;
        struct literal_expression literal;
        struct expression *grouped;
        struct binary_expression binary;
        struct function_expression function;
        struct member_access_expression member_access;
    };
} expression;

struct_list(expression);

enum statement_kind {
    BINDING_STATEMENT = 1,
    IF_STATEMENT,
    RETURN_STATEMENT,
    BLOCK_STATEMENT,
    ACTION_STATEMENT,
    WHILE_LOOP_STATEMENT,
    TYPE_DECLARATION_STATEMENT,
    BREAK_STATEMENT,
	SWITCH_STATEMENT,
    C_BLOCK_STATEMENT
};

enum switch_pattern_kind {
    OBJECT_PATTERN_KIND,
    ARRAY_PATTERN_KIND,
    NUMBER_PATTERN_KIND,
    STRING_PATTERN_KIND,
    VARIABLE_PATTERN_KIND,
    UNDERSCORE_PATTERN_KIND,
    REST_PATTERN_KIND
};
    
typedef struct key_pattern_pair {
    struct list_char key;
    struct switch_pattern *pattern;
} key_pattern_pair;

struct_list(key_pattern_pair);

struct object_pattern {
    struct list_key_pattern_pair pairs;
};

struct array_pattern {
    struct list_switch_pattern *patterns;
};

struct number_pattern {
    double number;
};

struct string_pattern {
    struct list_char str;
};

struct variable_pattern {
    struct list_char variable_name;
};

typedef struct switch_pattern {
    enum switch_pattern_kind switch_pattern_kind;
    union {
        struct object_pattern object_pattern;
        struct array_pattern array_pattern;
        struct number_pattern number_pattern;
        struct string_pattern string_pattern;
        struct variable_pattern variable_pattern;
    };
} switch_pattern;

struct_list(switch_pattern);

struct type_declaration_statement {
    struct type type;
    struct list_statement *statements;
};

struct binding_statement {
    struct list_char variable_name;
    struct type variable_type;
    struct expression value;
    int has_type;
};

struct if_statement {
    struct expression condition;
    struct statement *success_statement;
    struct statement *else_statement;
};

struct while_loop_statement {
    struct expression condition;
    struct statement *do_statement;
};

typedef struct case_statement {
	struct switch_pattern pattern;
    struct statement *statement;
} case_statement;

struct_list(case_statement);

struct switch_statement {
	struct expression switch_expression;
	struct list_case_statement cases;
};

struct statement_metadata {
    unsigned int row;
    unsigned int col;
    char *file_name;
};

struct c_block_statement {
    struct list_char *raw_c;
};

typedef struct statement {
    enum statement_kind kind;
    union {
        struct expression expression;
        struct binding_statement binding_statement;
        struct if_statement if_statement;
        struct list_statement *statements;
        struct while_loop_statement while_loop_statement;
        struct type_declaration_statement type_declaration;
        struct switch_statement switch_statement;
        struct c_block_statement c_block_statement;
    };
    struct statement_metadata metadata;
} statement;

struct_list(statement);

#endif
