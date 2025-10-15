gcc -o compiler main.c tokenizer.c utils.c parser.c -lm
    # && mkdir -p target \
    # && ./compiler "$1" \
    # && cd target \
    # && gcc -o bin c_output.c
