gcc -o compiler main.c lexer.c utils.c c_generation.c parser.c -lm \
    && mkdir -p target \
    && ./compiler "$1" \
    && cd target \
    && gcc -o bin c_output.c
