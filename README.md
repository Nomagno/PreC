# The PreC programming language
PreC, short for **Pre**processable **C**, is a simplified version of C99. The syntax and semantics are meant to play far nicer with the C preprocessor than C itself, hence allowing to write macros much easier.

Special thanks to my friend [Linja](https://codeberg.org/linja) for providing ideas and feedback in everything that relates to the language design!

Its major features include:
- Anonymous function literal syntax (no variable capture): `<return_type (&)(args)> ${ ... }`
- No function definition/declaration syntax: instead assign anonymous functions to global function pointer variables.
- Properly context free grammar, no lexer hack.
- Fully non-overloaded operators: unary minus `-a` replaced with `~a`, dereference operator is now `^`, *bitwise* operations are now `and` `or` `xor` `not`.
- Postfix type syntax with no need for parentheses.
- Bitwise operator symbols.
- "Declaration reflects use" is gone, types must now appear fully before identifiers.
- Constness by default, `mut` keyword to declare variables as mutable.
- No `typedef`, use `#define`.
- `typeof` 'backported' from C23.
- Custom-named C types may still be referenced with `@TypeName`.
- `c_include` directive for direct usage of C headers, and hence perfect C library compatibility.

See `examples/informal_spec.rs` and the rest of examples for more details.

PreC currently has a four-stage translation process:
1. The PreC source is preprocessed with the C preprocessor.
2. The preprocessed source is compiled to C with any `c_include` directives replaced with `#include` directives.
3. The C source is preprocessed with the C preprocessor.
4. The preprocessed source is compiled with your system's C compiler.

# Usage (Unix-like systems):
- Run `make` to compile. You may now use `precc.sh` like you would use any other C compiler. It will detect any `.prec` and `.preh` files in the arguments, convert them to `.c`/`.h` versions, and pass them along to the `cc` command.

# Installation (Unix-like systems):
- `make install` will install the `precc` command to your user binary directory.
