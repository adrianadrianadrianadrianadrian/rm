gcc -o compiler main.c lexer.c utils.c parser.c -lm -D DEBUG_CONTEXT \
    && mkdir -p target \
    && ./compiler "$1" \
    && cd target \
    && gcc -o bin c_output.c
