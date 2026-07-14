This is just me learning about language/compiler design by doing. Goal of this would be to build a language that's somewhere between C and Rust, but closer to C. I'll just lower to C, probably forever.

oh, and I'm letting it leak memory everywhere for now (ever).

To have a play: `gcc -o build build.c && ./build` then `./rm main.rm`.

This is constantly changing but currently looks like the following:
```
struct person {
    age: i32,
    height: i32,
    sibling: ?*struct person,
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
    let loop_count: i32 = 9;
    let i: ?i32 = null;
    let client = struct api_client { port = 8080 };

    if (loop_count > 10) {
        `printf("%d > 10\n", loop_count);`
    } else {
        `printf("%d <= 10\n", loop_count);`
    }

    return 0;
}

fn person_age(p: struct person) -> i32 {
    return p.age;
}
```
