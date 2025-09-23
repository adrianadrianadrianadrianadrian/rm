gcc -o compiler main.c -lm && ./compiler "$1" && gcc -o build c_output.c
