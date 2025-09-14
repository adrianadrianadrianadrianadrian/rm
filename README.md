This is just me learning about language/compiler design by doing. Goal of this would be to build a language that's somewhere between C and Rust, but closer to C. I'll just lower to C, probably forever.

oh, and I'm letting it leak memory everywhere for now.

So far this compiles via compiler.sh:
```
#include <stdio.h>
#include <stdlib.h>

struct person {
    age: i32,
    height: i32
}

enum result {
    ok: person,
    error: i32
}

fn factorial(input: i32) -> i32 {
    if (input == 0) {
        return 1;
    }
    
    return input * factorial(input - 1);
}

fn main() -> i32 {
    something: *u8 = malloc(100);
    defer free(something);
    loop_count: i32 = 11;
    defer printf("%d != 11\n", loop_count);

    while (loop_count > 0) {
        printf("Hello world! %d\n", factorial(loop_count));
        loop_count = loop_count - 1;
    }

    return 0;
}
```

```
enum result {
    ok: struct person {
        height: i8,
        age: i8
    },
    error: i8
};

fn say_hi(person_name: string) -> void;

fn nested(cb: fn(a: i8, b: u8) -> void, d: struct { f: i8 }) -> void;
```
