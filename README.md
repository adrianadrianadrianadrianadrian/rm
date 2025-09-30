This is just me learning about language/compiler design by doing. Goal of this would be to build a language that's somewhere between C and Rust, but closer to C. I'll just lower to C, probably forever.

oh, and I'm letting it leak memory everywhere for now.

So far this compiles via compiler.sh:
```
#include <stdio.h>
#include <stdlib.h>

struct person {
    age: i32,
    height: i32,
    sibling: ?*struct person,
    buffer: *[2][1024]u32
}

enum result {
    ok: struct person,
    error: i32
}

struct api_client {
    port: i16
}

fn factorial(input: i32) -> i32 {
    if (input == 0) {
        return 1;
    }
    
    return input * factorial(input - 1);
}

fn main() -> i32 {
    loop_count: mut i32 = 11;
    i: ?i32 = null;
    client = struct api_client { port = 8080 };
    
    running: bool = false;
    not_running = !running;

    while (loop_count > 0) {
        printf("Hello world! %d\n", factorial(loop_count));
        loop_count = loop_count - 1;
    }

    return 0;
}

fn person_age(p: struct person) -> i32 {
    return p.age;
}
```

switch statement current idea, I'd like full pattern matching here.
```
struct person {
    age: u32,
    height: u32
}

enum result {
    ok: struct person,
    error: i32
}

fn test(res: enum result) -> void {
    switch (res) {
        case { ok: p@{ age: 21 }}:
        {
            printf("Height of 21 year old is %d\n", p.height);
            break;
        }
        case { error: err }:
        {
            printf("Error!!!! %d\n", err);
            break;
        }
    }
}
```

