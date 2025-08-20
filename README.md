This is just me learning about language/compiler design by doing. Goal of this would be to build a language that's somewhere between C and Rust, but closer to C. I'll just lower to C, probably forever.

Here's some snippets thus far:
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

oh, and I'm letting it leak memory everywhere for now.
