gcc -o compiler main.c && ./compiler "$1" && gcc -o build c_output.c
