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
    let loop_count: mut i32 = 9;
    let i: ?i32 = null;
    let client = struct api_client { port = 8080 };

    if (loop_count > 10) {
        printf("%d > 10\n", loop_count);
    } else {
        printf("%d <= 10\n", loop_count);
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

Currently for IO, as you can see, I'm directly including C headers and running functions such as `printf`. This is just a hack to see something happen. I think long term I'm leaning toward having a special struct that's provided by the compiler, called `io`, that'll have the ability to essentially run syscalls on the respective platform. Then any function that wishes to do IO will need to take this struct as a param. The value of this will come from the main function only. Though, there'll be a way to create it out of thin air for dev purposes in non release builds.

e.g.
```
fn main(io: struct io, ..) -> i32 {
    print(io, "Hello, world!\n");
    return 0;
}

fn print(io: struct io, message: []u8) {
    ...
}
```
