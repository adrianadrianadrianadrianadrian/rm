gcc -o compiler main.c lexer.c utils.c parser.c lowering/c.c error.c context.c -lm \
    && mkdir -p target \
    && ./compiler "$1" \
    && cd target \
    && gcc -o bin c_output.c
