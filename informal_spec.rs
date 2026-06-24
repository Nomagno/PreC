// Preprocessable C (PreC):
// Following changes to C99:
// - uses the C preprocessor with no changes
// - can use `#c_include` to use C headers, full C ABI compatibility is preserved
// - full list of primitive types: bool, u8, i8, u16, i16, u32, i32, u64, i64, f32, f64
// - NO type coercion at all, must explicitly cast
// - constness by default, `mut` to mutate
// - struct autoderreference: no need to do a->x, just do a.x
// - structs are zero-initialized
 
// - no typedef:
//      struct vector { i32 x, y; };
//      MUST be referred to as ``struct vector''
// - for type aliases, use macros #define Vector struct vector
// - Parentheses allowed in type names in a lot more places, no erroring out stupidly
// - Types go all before the variable name: int& [5] a;
// - trailing commas allowed everywhere reasonable
// - Structs can have default values accessed through a new ``default'' value:
//    struct search_result { i32 x = -1; };
//    struct search_result foo = default;
 
// - operators are now overloading-free:
//      | -> OR
//      ^ -> XOR
//      + (unary) -> [ELIMINATED]
//      ~ (bitwise) -> NOT
//      - (unary) -> ~
//      & (bitwise) -> AND
//      Reference operator: &
//      Dereference operator: ^
// - +=, *=, etc, all gone
// - keywords gone for obvious reasons: auto, register, _Imaginary, _Complex
//
 
// Parameter names used for initialiation, but otherwise discarded from the type as in regular C
mut i32 (&)(i32 a, i32 b) add = ${ return a+b; };

// Have to use a new type of code-valued compound literal because no parameter names specified
i32 (&)(i32, i32) sub = (i32 (&)(i32 a, i32 b)) ${ return a-b; };

i32 (&)(void) main = ${
    i32 x = sub(1, 3); // value: -2

    mut i32 y = add(1, 3); // value: 4

    // redefine add to multiply by two after adding
    add = (i32 (&)(i32 j, i32 k)) ${ return 2*(j+k); }

    y = add(1, 3); // value: 8
    
    return 0;
}
