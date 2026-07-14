#include <stdio.h>
#include "parser.h"
#include "lexer.h"
#include "error.h"
#include "context.h"
#include "soundness.h"
#include "type_checker.h"
#include "lowering/c.h"
#include <sys/time.h>
#include <unistd.h>
#include "debug.h"

struct timeval current_time()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t;
}

double ms_delta(const struct timeval *t1, const struct timeval *t2)
{
    return (t2->tv_sec - t1->tv_sec) * 1000.0 +
           (t2->tv_usec - t1->tv_usec) / 1000.0;
}

int compile(char *file_name, struct error *error)
{
    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        write_raw_error(stderr, "input file not found.");
        return 0;
    }

    struct token_buffer tb = create_token_buffer(f, file_name);
    struct parsed_file parsed = {0};
    struct context c = {0};

    struct timeval start = current_time();
    if (!parse_file(&tb, &parsed, error))     return 0;
    struct timeval parse_end = current_time();
    if (!contextualise(&parsed, &c, error))   return 0;
    struct timeval context_end = current_time();
    show_context(&c);
    if (!soundness_check(&parsed, &c, error)) return 0;
    struct timeval soundness_end = current_time();
    if (!type_check(&parsed, &c, error))      return 0;
    struct timeval type_check_end = current_time();
    generate_c(&parsed, &c);
    struct timeval codegen_end = current_time();

    printf("parsed          %lfms\n", ms_delta(&start, &parse_end));
    printf("context         %lfms\n", ms_delta(&parse_end, &context_end));
    printf("soundess        %lfms\n", ms_delta(&context_end, &soundness_end));
    printf("type_checked    %lfms\n", ms_delta(&soundness_end, &type_check_end));
    printf("codegen         %lfms\n", ms_delta(&type_check_end, &codegen_end));
    printf("total           %lfms\n", ms_delta(&start, &codegen_end));

    return 1;
}

int main(int argc, char **argv)
{
    struct error error = {0};

    if (argc <= 1 || !strcmp(argv[1], "")) {
        write_raw_error(stderr, "no input file provided.");
        return 1;
    }

    if (!compile(argv[1], &error)) {
        write_error(stderr, &error);
        return 1;
    }

    return 0;
}
