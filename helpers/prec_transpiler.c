#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <err.h>
#include "prec_ast.h"
#include "prec_transpiler.h"
#include "prec_symbol_table.h"
#include "prec_type_table.h"

// There is a linked list of output buffers, a new one will be inserted if a function
// is translated within a function being translated.
// This is needed exists because preC supports anonymous functions.
// These are translated to a static function declared just before the current definition,
// with a unique name, and a pointer to this function at the usage site.
// When a declaration ends, the buffers are printed, then the linked list is cleared.

// basic pseudocode example:
// f() { x = func(){ B; y = func() { C; } }; A; }

// translation:
// static anon_1() { C; }
// static anon_0() { B; y = anon_1; }
// f() { x = anon_0; A; }


// top-level declarations of function pointers with no qualifiers **MUST** be turned into
// function declarations, or declaration + definition if it's also initialized.
// so rather than the process described above,
// directly translate the type with the variable name as the identifier, then do not print the equals sign, then directly translate the block.
// if there's no block, then insert a semicolon at the end
// if it's a list, then treat it as several different declarations
// thankfully, 'no qualifiers' implies constness in preC, so there is in fact no risk of
// issues with redefinition of symbols.


// The basic idea is to used memory mapped buffers for all translation:
/*
FILE *f = fmemopen(buffer, sizeof(buffer), "w");
fprintf(f, "mystring");
fclose(f);
*/
// We use open_memstream(3) in the end, which handles reallocation automatically

unsigned global_identifier_counter = 0;
int global_indent_level = 0;

SymPtr sym_table;
TypeTablePtr type_table;

// We expose the top level list of c_include directives and declarations
// because the cleanest way to allow constdata functions to access their own type
// is through inserting into it. Otherwise, it wouldn't be needed
struct TopLevel *top_level_list;

struct BufferList {
    size_t size;
    char *buf;
    FILE *stream;
    struct BufferList *next;
};

struct BufferList *buffer_list;

struct BufferList *current_buffer;

#define NEW_REFERENCE(_e, _source) DUP_T(Expr, Unary, .unOp = { .tag = Ref, .e = _e }, .source_line = _source)

#define NEW_CAST(_e, _t, _source) DUP_T(Expr, Cast, .cast = { .type = _t, .e = _e }, .source_line = _source)

#define NEW_COMPOUND_LITERAL(_e, _t, _source) DUP_T(Expr, CompoundLiteral, .compound_literal = { .type = _t, .init = DUP_T(Initializer, Data, .data = DUP((struct InitializerList){ .current = DUP_T(Initializer, Expr, .expr = _e, .source_line = _source) , .source_line = _source }), .source_line = _source)}, .source_line = _source)

#define NEW_INT(_x, _source) DUP_T(Expr, Int, .int_num = _x, .source_line = _source)

#define NEW_IDENTIFIER(_id, _source) DUP_T(Expr, Identifier, .identifier = _id, .source_line = _source)

#define GROUP(...) __VA_ARGS__

#define QUALIFY(_t, _q, _source) DUP_T(Type, Qualifier, \
    .qualifier = { \
        .qualifiers = _q, \
        .t = _t \
    }, \
    .source_line = _source \
);



#define REWIND_LIST(_name) do { while (_name->prev != NULL) { _name = _name->prev; } } while(0)

#define DISCARD_QUALIFIERS(_type) do { if (_type && _type->tag == Qualifier) _type = _type->qualifier.t; } while(0)

struct BufferList *create_buffer(void) {
    struct BufferList *retval = calloc(sizeof(struct BufferList), 1);
    retval->next = NULL;
    retval->size = 0;
    retval->buf = NULL;
    retval->stream = open_memstream(&retval->buf, &retval->size);

    return retval;
}


void print_buffer_list(struct BufferList *list) {
    struct BufferList *curr = list;
    while (curr != NULL) {
        fclose(curr->stream);
        printf("%s\n", curr->buf);
        curr = curr->next;
    }
}

void destroy_buffer_list(struct BufferList *list) {
    struct BufferList *curr = list;
    while (curr != NULL) {
        free(curr->buf);
        struct BufferList *old = curr;
        curr = curr->next;
        free(old);
    }
}

bool dry_run = false;

unsigned previous_source_line = 0;

// Indicates the source line, if 0 then there's no source line.
// Set by every call to p_src.
// Consumed by every call to p that follows a newline.
// Calls to p_src that have their source line parameter as -1 do not alter source_line.
unsigned source_line = 0;

// If a newline was just printed, then source_line must be != 0
_Bool newline_just_printed = 0;

extern const char *pretty_filename;
extern const char *filename;

#define FILENAME_GRACEFUL ((pretty_filename != NULL) ? pretty_filename : filename)

#define p(...) {\
        if (newline_just_printed) { \
            if (source_line == 0) { \
                fprintf(stderr, "Warning: no source line for: \""); \
                fprintf(stderr, __VA_ARGS__); \
                fprintf(stderr, "\" last known line: %d\n", previous_source_line); \
                newline_just_printed = 0; \
            } else { \
                if (pretty_filename != NULL)\
                    fprintf(current_buffer->stream, "#line %d\n", source_line); \
                source_line = 0; \
                newline_just_printed = 0; \
                tabs(); \
            } \
        } \
    if (!dry_run) { \
        int s = fprintf(current_buffer->stream, __VA_ARGS__);\
        fflush(current_buffer->stream);\
        if (s == -1)\
            err(EXIT_FAILURE, "fprintf");\
    }\
}

#define p_src(_source_line, ...) { \
    if (_source_line >= 0) \
        source_line = _source_line; \
    p(__VA_ARGS__); \
}

#define set_src(_source_line) { previous_source_line = source_line; source_line = _source_line; }

#define NEWLINE() { p("\n"); newline_just_printed = true; }

#define INDENT_STR "  "
void tabs(void) {
    if (!dry_run) {
        for (int i = 0; i < global_indent_level; i++) {
            int s = fprintf(current_buffer->stream, INDENT_STR);
            fflush(current_buffer->stream);
            if (s == -1)
                err(EXIT_FAILURE, "fprintf");
        }
    }
}

void tabs_custom(FILE *stream) {
    for (int i = 0; i < global_indent_level; i++) {
        int s = fprintf(stream, INDENT_STR);
        fflush(stream);
        if (s == -1)
            err(EXIT_FAILURE, "fprintf");
    }
}

// t_XXX functions transpile directly to the buffer
// t_str_XXX functions return a transpiled string


// main resource used: http://unixwiz.net/techtips/reading-cdecl.html

// VALUE_IFNOT_TEST(X) only pastes T of no vara
#define VALUE_IFNOT_TEST(...) __VA_ARGS__
#define VALUE_IFNOT_TEST1(...)
#define VALUE_IFNOT(COND, ...) VALUE_IFNOT_TEST ## COND ( __VA_ARGS__ )
#define __VA_ALT__(__x, ...) VALUE_IFNOT(__VA_OPT__(1), __x) __VA_ARGS__


// Returns type the expression reduces to if. If it's not inferrable (unknown symbols), returns NULL
// this is used ONLY for constdata, the rest of the type inference is done by the C compiler.
// Constdata is a PreC typesystem level feature, so we WILL be able to infer a specific struct/union/enum
// type if and only if we have the type in the type table
struct Type *t_expr(struct Expr *x, bool inline_when_possible);
#define t_expr(_x, ...) t_expr(_x, __VA_ALT__(false, __VA_ARGS__))

// a return value of 2 instead of 1 indicates that it's a void type, and must hence NOT be qualified
bool isBaseType(enum TypeSort x) {
    return x == TypeofExpr
        || x == TypeofType
        || x == Struct
        || x == Tuple
        || x == Union
        || x == Enum
        || x == CType
        || x == f64
        || x == f32
        || x == i64
        || x == u64
        || x == i32
        || x == u32
        || x == i16
        || x == u16
        || x == i8
        || x == u8
        || x == uptr
        || x == iptr
        || x == Bool
        || x == Void
        ;
}

char *t_str_type(struct Type *x, char *identifier, bool fun_pointer_dereferenced);

bool hasConst(QualifierBitVector q) { return (q & Mut) == 0; }
bool hasVolatile(QualifierBitVector q) { return q & Volatile; }
bool hasRestrict(QualifierBitVector q) { return q & Restrict; }

// probably dispatch qualifiers and pointers to one left stack, and
// arrays and functions to another right stack
// in the left stack, right == inner
// in the right stack, left == inner

// a stack might not be the right analogy, what I got told by a friend is this:
// >> Add another layer of parens every time you switch from left to right, always put the basic type on the far left, qualifier placement is finnicky...
// so probably gotta add a marker in the dispatch when this condition happens, to insert parentheses where appropiate (when switching from right to left buffers)

struct TypeBuffer {
    char *buf; // where the full type will be composed, holds the base type too
    size_t size;
    FILE *stream; // stream associated to the buffer

    QualifierBitVector base_type_qualifiers;

    enum {Nothing, Left, Right} last_written_to_buffer;

    size_t left_buffer_pos;
    char left_buffer[1024];

    size_t right_buffer_pos;
    char right_buffer[1024];
};

struct TypeBuffer *new_type_buffer(void) {
    struct TypeBuffer *retval = calloc(sizeof(struct TypeBuffer), 1);
    retval->last_written_to_buffer = Nothing;
    retval->stream = open_memstream(&retval->buf, &retval->size);
    retval->left_buffer_pos = sizeof(retval->left_buffer)/2;
    retval->right_buffer_pos = sizeof(retval->right_buffer)/2;
    return retval;
}

#define p_t(...) {\
    int s = fprintf(type_buffer->stream, __VA_ARGS__);\
    fflush(current_buffer->stream);\
    if (s == -1)\
        err(EXIT_FAILURE, "fprintf");\
}

// arg list is the list of the textual representation of each paramter in the function type
void dispatch_function(struct TypeBuffer *type_buffer, char *arg_list[]) {
    type_buffer->last_written_to_buffer = Right;

    type_buffer->right_buffer_pos -= 1;
    type_buffer->right_buffer[type_buffer->right_buffer_pos] = ')';

    char **begginning = arg_list;

    // We do two passes: one to reserve space, and another to actually copy the arguments
    size_t size = 0;
    while (*arg_list != NULL) {
        size += strlen(*arg_list);

        // Reserve space for ", "
        if (*(arg_list+1) != NULL)
            size += 2;

        arg_list++;
    }
    type_buffer->right_buffer_pos -= size+1;

    // Copy over the parameters
    size_t curr_pos = type_buffer->right_buffer_pos;
    type_buffer->right_buffer[curr_pos] = '(';
    curr_pos += 1;


    arg_list = begginning;

    while (*arg_list != NULL) {
        size_t len = strlen(*arg_list);
        memcpy(type_buffer->right_buffer+curr_pos, *arg_list, len);
        curr_pos += strlen(*arg_list);

        if (*(arg_list+1) != NULL)
            memcpy(type_buffer->right_buffer+curr_pos, ", ", 2);

        curr_pos += 2;
        arg_list++;
    }

}

void dispatch_array(struct TypeBuffer *type_buffer, struct Expr *expression) {
    type_buffer->last_written_to_buffer = Right;

    type_buffer->right_buffer_pos -= 1;
    type_buffer->right_buffer[type_buffer->right_buffer_pos] = ']';

    if (expression != NULL) {
        char *expression_buffer = calloc(512, 1);
        FILE *stream = fmemopen(expression_buffer, 512, "w");

        struct BufferList *saved_buffer = current_buffer;
        current_buffer = &(struct BufferList){ .stream = stream };

        t_expr(expression, true /*inline_when_possible*/);

        current_buffer = saved_buffer;
        fclose(stream);

        size_t size = strlen(expression_buffer);
        type_buffer->right_buffer_pos -= size;
        memcpy(type_buffer->right_buffer+type_buffer->right_buffer_pos, expression_buffer, size);
        free(expression_buffer);
    }

    type_buffer->right_buffer_pos -= 1;
    type_buffer->right_buffer[type_buffer->right_buffer_pos] = '[';
}

// Dispatch to left buffer
void dispatch_pointer(struct TypeBuffer *type_buffer) {
    if (type_buffer->last_written_to_buffer == Right) {
        type_buffer->left_buffer_pos -= 2;
        type_buffer->left_buffer[type_buffer->left_buffer_pos] = '(';
        type_buffer->left_buffer[type_buffer->left_buffer_pos+1] = '*';

        type_buffer->right_buffer_pos -= 1;
        type_buffer->right_buffer[type_buffer->right_buffer_pos] = ')';
    } else {
        type_buffer->left_buffer_pos -= 1;
        type_buffer->left_buffer[type_buffer->left_buffer_pos] = '*';
    }
    type_buffer->last_written_to_buffer = Left;
}

void str_insert(char dest[], const char src[], size_t pos) {
    char *buf = calloc(strlen(dest)+strlen(src)+2, 1);
    size_t len = 0;

    strncpy(buf, dest, pos);
    len += pos;
    strncpy(buf+len, src, strlen(src));
    len += strlen(src);
    strncpy(buf+len, dest+pos, strlen(dest)-pos);

    strncpy(dest, buf, strlen(buf));
}

#define LEFT_BUFFER_END type_buffer->left_buffer+type_buffer->left_buffer_pos

// How it works: const will always be dispatched unless what is being dispatched is a mut
// a mut cancels a nearby const
void dispatch_qualifiers(struct TypeBuffer *type_buffer, enum TypeSort tag, bool is_const, bool is_restrict, bool is_volatile) {
    if (isBaseType(tag)) {
        if (!is_const)
            type_buffer->base_type_qualifiers |= Mut;
        else
            type_buffer->base_type_qualifiers &= ~Mut;

        if (is_restrict)
            type_buffer->base_type_qualifiers |= Restrict;
        if (is_volatile)
            type_buffer->base_type_qualifiers |= Volatile;
    } else {
        unsigned pos = 0;
        if (type_buffer->left_buffer[type_buffer->left_buffer_pos] == '(') {
            pos = 2;
        } else if (type_buffer->left_buffer[type_buffer->left_buffer_pos] == '*') {
            pos = 1;
        } else {
            // No, we don't actually error out, we want to be able to gracefully handle this one as dispatch_qualifiers is used in more places than needed
            // on purpose:

            //fprintf(stderr, "Compiler internal precondition violation: Don't know how to parse this internal type left buffer: %s.\n",
            //    LEFT_BUFFER_END);
            //assert(false);
            return;
        }

        // insert qualifiers
        if (is_const) {
            // If there's already a const, do nothing
            size_t size = strlen(LEFT_BUFFER_END);

            if (((int)type_buffer->left_buffer_pos+(int)size-(int)strlen("const")-1) >= 0
                &&
                strncmp(LEFT_BUFFER_END+size-strlen("const")-1,
                        "const",
                        strlen("const")) == 0)
            {
                ;
            } else {
                str_insert(LEFT_BUFFER_END, "const ", pos);
            }

        } else {
            // If there's a const, remove it
            size_t size = strlen(LEFT_BUFFER_END);

            if (((int)type_buffer->left_buffer_pos+(int)size-(int)strlen("const")-1) >= 0
                &&
                strncmp(LEFT_BUFFER_END+size-strlen("const")-1,
                        "const",
                        strlen("const")) == 0)
            {
                memcpy(LEFT_BUFFER_END+size-strlen("const")-1, "               ", strlen("const"));
            }
        }

        if (is_volatile)
            str_insert(LEFT_BUFFER_END, "volatile ", pos);
        if (is_restrict)
            str_insert(LEFT_BUFFER_END, "restrict ", pos);
    }
}

char *type_id(struct Type *x) {
    switch(x->tag) {
        case TypeofType:
        case TypeofExpr:
            assert(!"Typeof not supported for tuples for now");
        case Tuple: {
            char *retval = NULL;
            //  translate each of the parameters, also applying this as needed:
                // "Like struct members, tuple members must always be implicitly mut"
            struct TypeParamList *node = x->tuple.member_list;
            REWIND_LIST(node);
            unsigned counter = 0;
            asprintf(&retval, "__prec_internal_tuple_");

            while (node != NULL) {
                assert(node->param != NULL);

                char *member_name = node->param->name;
                if (member_name == NULL) {
                    // Unnamed tuple members get the name _0 if in first position, _1 if in second, etc.
                    asprintf(&member_name, "ANON%d", counter);
                }

                if (node->param->type->tag == Qualifier) {
                    node->param->type->qualifier.qualifiers |= Mut;
                } else {
                    node->param->type =
                        QUALIFY(node->param->type, Mut, node->param->type->source_line);
                }

                asprintf(&retval, "%s_%s_%s", retval, type_id(node->param->type), member_name);

                if (node->param->type)
                node = node->next;
                counter += 1;
            }

            asprintf(&retval, "%s__", retval);

            return retval;
        }
        case FunPointer: {
            char *retval = NULL;
            // translate each of the parameters
            asprintf(&retval, "__funptr_");
            asprintf(&retval, "%s_%s", retval, type_id(x->fun_pointer.return_type));

            struct TypeParamList *node = x->fun_pointer.param_list;
            REWIND_LIST(node);

            while (node != NULL) {
                if (node->param == NULL) {
                    asprintf(&retval, "%s_PREC_ELLIPSIS", retval);
                    break;
                }

                char *member_name = node->param->name;
                if (member_name == NULL) {
                    asprintf(&retval, "%s_%s", retval, type_id(node->param->type));
                } else {
                    asprintf(&retval, "%s_%s_%s", retval, type_id(node->param->type), member_name);
                }

                if (node->param->type)
                node = node->next;
            }

            asprintf(&retval, "%s__", retval);

            return retval;
        }
        case Qualifier: {
            char *retval = "";
            if (x->qualifier.qualifiers & Mut) {
                asprintf(&retval, "%smut", retval);
            }
            if (x->qualifier.qualifiers & Volatile) {
                asprintf(&retval, "%svolatile", retval);
            }
            if (x->qualifier.qualifiers & Restrict) {
                asprintf(&retval, "%srestrict", retval);
            }

            char *translated = type_id(x->qualifier.t);
            asprintf(&retval, "%s_%s", retval, translated);
            free(translated);
            return retval;
        }
        case Reference: {
            char *retval = NULL;
            char *translated = type_id(x->reference);
            asprintf(&retval, "ref_%s", translated);
            free(translated);
            return retval;
        }
        case CType: {
            char *retval = NULL;
            asprintf(&retval, "external_%s", x->c_type);
            return retval;
        }
        case Struct: {
            char *retval = NULL;
            asprintf(&retval, "struct_%s", x->tag_name);
            return retval;
        }
        case Union: {
            char *retval = NULL;
            asprintf(&retval, "union_%s", x->tag_name);
            return retval;
        }
        case Enum: {
            char *retval = NULL;
            asprintf(&retval, "enum_%s", x->tag_name);
            return retval;
        }
        case Array: {
            char *retval = NULL;
            struct Expr *size = x->array.size->expr;
            unsigned array_size = 0;
            while (size->tag == Cast){
                size = size->cast.e;
                if (size->tag == UInt) {
                    array_size = size->uint_num;
                } else if (size->tag == Int) {
                    array_size = size->int_num;
                } else {
                    assert(!"Complex array size expressions not supported yet in tuple type signatures");
                }
            }
            assert(array_size != 0);

            char *translated = type_id(x->array.t);
            asprintf(&retval, "array_%d_%s", array_size, translated);
            free(translated);
            return retval;
        }
        case u8:   return strdup("u8");
        case i8:   return strdup("i8");
        case u16:  return strdup("u16");
        case i16:  return strdup("i16");
        case u32:  return strdup("u32");
        case i32:  return strdup("i32");
        case u64:  return strdup("u64");
        case i64:  return strdup("i64");
        case f32:  return strdup("f32");
        case f64:  return strdup("f64");
        case uptr: return strdup("uptr");
        case iptr: return strdup("iptr");
        case Void: return strdup("void");
        case Bool: return strdup("bool");
    }
}

char *global_tuple_array[1024];

char *register_tuple_if_needed(struct Type *x) {
    assert(x != NULL);

    char *type_identifier = type_id(x);

    int last_free_index = -1;
    bool registered = false;
    for (unsigned i = 0; i < 1024; i++) {
        if (global_tuple_array[i] == NULL) {
            last_free_index = i;
            continue;
        }

        if (strcmp(global_tuple_array[i], type_identifier) == 0) {
            registered = true;
            break;
        }
    }
    if (!registered) {
        global_tuple_array[last_free_index] = type_identifier;
        // Tuples are lazily created:
        // The first time it's encountered, it is registered as a struct and defined.
        // The name is unique and given by the type_id() function.
        // This gives the effect of all possible tuples appearing to be defined: structural typing.
        
        // We create a new buffer to print the type to,
        // print the code to it, then restore the current buffer.
        int saved_indent = global_indent_level;
        global_indent_level = 0;

        struct BufferList *saved_buffer = current_buffer;
        struct BufferList *tmp = buffer_list;

        buffer_list = create_buffer();
        buffer_list->next = tmp;
        current_buffer = buffer_list;

        set_src(x->source_line);
        p("struct %s {", type_identifier);
        NEWLINE();

        global_indent_level += 1;

        struct TypeParamList *node = x->tuple.member_list;
        REWIND_LIST(node);
        unsigned counter = 0;
        while (node != NULL) {
            assert(node->param != NULL);
            set_src(node->param->source_line);

            char *member_name = node->param->name;
            if (member_name == NULL) {
                // Unnamed tuple members get the name _0 if in first position, _1 if in second, etc.
                asprintf(&member_name, "_%d", counter);
            }

            tabs();
            p("%s;", t_str_type(node->param->type, member_name, false));
            NEWLINE();

            if (node->param->type)
            node = node->next;
            counter += 1;
        }
        set_src(x->source_line);

        global_indent_level -= 1;
        p("};");
        //NEWLINE();

        current_buffer = saved_buffer;
        global_indent_level = saved_indent;
    }

    return type_identifier;
}

void t_internal_type(struct Type *x, struct TypeBuffer *type_buffer) {
    switch (x->tag) {
    // Compound types: qualifiers, references, function pointers, arrays
    case Qualifier:
        if (x->qualifier.t->tag == Array) {
            // arrays can't have qualifiers in C, instead apply them to the
            // inner elements
            // u32 [3] mut -> u32 mut [3]
            struct Type curr = *x;
            struct Type *inner_ptr = x->qualifier.t;
            struct Type *inner_inner = inner_ptr->array.t;
            struct Type inner = *x->qualifier.t;
            *x = inner;
            *inner_ptr = curr;
            x->array.t = inner_ptr;
            inner_ptr->qualifier.t = inner_inner;

            // If two qualifier lists exist near each other, merge them
            // u32 mut [3] mut -> u32 mut mut [3] -> u32 mut [3]
            if (inner_ptr->qualifier.t->tag == Qualifier) {
                struct Type *inner = inner_ptr->qualifier.t;
                inner_ptr->qualifier.qualifiers |= inner->qualifier.qualifiers;
                inner_ptr->qualifier.t = inner->qualifier.t;
                free(inner);
            }
            t_internal_type(x, type_buffer);
        } else {
            QualifierBitVector q = x->qualifier.qualifiers;
            t_internal_type(x->qualifier.t, type_buffer);
            dispatch_qualifiers(type_buffer, x->qualifier.t->tag, true, false, false);
            dispatch_qualifiers(type_buffer, x->qualifier.t->tag, !(q & Mut), q & Restrict, q & Volatile);
        }
        break;
    case Reference:
        t_internal_type(x->reference, type_buffer);
        dispatch_qualifiers(type_buffer, x->reference->tag, true, false, false);

        dispatch_pointer(type_buffer);
        dispatch_qualifiers(type_buffer, Reference, true, false, false);
        break;
    case Array:
        t_internal_type(x->array.t, type_buffer);

        if (x->array.size == NULL) {
            dispatch_array(type_buffer, NULL);
        } else {
            dispatch_array(type_buffer, x->array.size->expr);
        }
        break;
    case FunPointer:
        t_internal_type(x->fun_pointer.return_type, type_buffer);
        dispatch_qualifiers(type_buffer, x->fun_pointer.return_type->tag, true, false, false);

        unsigned arg_i = 0;
        char *arg_list[65] = {0};
        struct TypeParamList *node = x->fun_pointer.param_list;

        if (node != NULL) {
            REWIND_LIST(node);
            while (node != NULL) {
                // We substract 1 because cell at index 64 will be used for implicit null termination
                if (arg_i >= sizeof(arg_list)/sizeof(char *)-1) {
                    fprintf(stderr, "%s:%d:%d: Compiler error: Function definitions can't have more than 64 arguments (uh, wtf?)\n",
                            FILENAME_GRACEFUL, x->source_line, 1);
                    exit(1);
                }
                arg_list[arg_i] = t_str_type(node->param->type, node->param->name, false);
                node = node->next;
                arg_i += 1;
            }
        }

        dispatch_function(type_buffer, arg_list);

        dispatch_pointer(type_buffer);
        dispatch_qualifiers(type_buffer, FunPointer, true, false, false);
        break;

    // Base types
    case CType:
        p_t("%s", x->c_type);
        break;
    case f64: p_t("double"); break;
    case f32: p_t("float"); break;
    case u64: p_t("uint64_t"); break;
    case i64: p_t("int64_t"); break;
    case u32: p_t("uint32_t"); break;
    case i32: p_t("int32_t"); break;
    case u16: p_t("uint16_t"); break;
    case i16: p_t("int16_t"); break;
    case u8: p_t("uint8_t"); break;
    case i8: p_t("int8_t"); break;
    case uptr: p_t("uintptr_t"); break;
    case iptr: p_t("intptr_t"); break;
    case Void: {
        // refuse to qualify void types, or rather qualify as special value
        type_buffer->base_type_qualifiers = 1 << 3;

        p_t("void");
        break;
    }
    case Bool: p_t("_Bool"); break;
    case TypeofExpr: {
        // TODO: figure out how to reduce these typeofs to the actual type on the compiler side, when possible
        // The issue is ``typeof(some_function_pointer) x'' has to be translated with t_str_type(type, "x", ...), which doesn't play nice with the compiler architecture.
        // The solution is probably to reduce all typeofs on the AST before doing the rest of the translation, as annoying as that is
        p_t("typeof(");

        struct BufferList *saved_buffer = current_buffer;

        current_buffer = &(struct BufferList){ .stream = type_buffer->stream };

        t_expr(x->typeof_expr);

        current_buffer = saved_buffer;

        p_t(")");
        break;
    }
    case TypeofType: {
        // TODO: figure out how to reduce these typeofs to the actual type on the compiler side, when possible
        // The issue is ``typeof(some_abstract_declarator) x'' has to be translated with t_str_type(type, "x", ...), which doesn't play nice with the compiler architecture.
        // The solution is probably to reduce all typeofs on the AST before doing the rest of the translation, as annoying as that is
        p_t("typeof(%s)", t_str_type(x->typeof_type, NULL, false));
        break;
    }
    case Struct:
    case Union:
        if (x->tag == Struct) {
            p_t("struct ");
        } else {
            p_t("union ");
        }

        p_t("%s ", x->tag_name);

        break;
    case Tuple:
        char *type_identifier = register_tuple_if_needed(x);
        p_t("struct %s ", type_identifier);
        break;
    case Enum:
        p_t("enum ");
        p_t("%s ", x->tag_name);
        break;
    }
    if (isBaseType(x->tag)) {
        p_t(" ");
    }
}

// - If identifier is NULL, an abstract generator will be generated.
// - If fun_pointer_dereferenced is true, and the type is a function pointer,
//   then the translation will be done as if it was a function instead of a pointer (no innermost pointer).
char *t_str_type(struct Type *x, char *identifier, bool fun_pointer_dereferenced) {
    struct TypeBuffer *type_buffer = new_type_buffer();

    t_internal_type(x, type_buffer);
    if (type_buffer->base_type_qualifiers & (1 << 3)) {
        // do NOT add any qualifiers to the specially marked void base type
        ;
    } else {
        if (!(type_buffer->base_type_qualifiers & Mut)) {
            p_t("const ");
        }
        if (type_buffer->base_type_qualifiers & Restrict) {
            p_t("restrict ");
        }
        if (type_buffer->base_type_qualifiers & Volatile) {
            p_t("volatile ");
        }
    }

    if (fun_pointer_dereferenced) {
        // To dereference, just remove the innermost pointer, which must always exist for a function pointer
        assert(x->tag == FunPointer);
        size_t size = strlen(LEFT_BUFFER_END);

        assert(((int)type_buffer->left_buffer_pos+(int)size-(int)strlen("*const")-1) >= 0);
        if (strncmp(LEFT_BUFFER_END+size-strlen("*const")-1,
                    "*const",
                    strlen("*const")) != 0) {
            fprintf(stderr, "%s:%d:%d: Compiler error: Function must be dereferenced but can't.\n",
                    FILENAME_GRACEFUL, x->source_line, 1);
            exit(1);
        }
        memcpy(LEFT_BUFFER_END+size-strlen("*const")-1, "               ", strlen("*const"));

    }
    p_t("%s", LEFT_BUFFER_END);
    if (identifier != NULL) {
        p_t("%s", identifier);
    }
    p_t("%s", type_buffer->right_buffer+type_buffer->right_buffer_pos);

    fclose(type_buffer->stream);

    char *retval = type_buffer->buf;
    free(type_buffer);
    return retval;
}

/*freeform: no newlines and no indents*/
void t_declaration(struct Declaration *decl, bool freeform, bool top_level);
void t_statement(struct Statement *stat);
void t_typedefinition(struct TypeDefinition *tdef, bool top_level);

void t_block(struct Block *b, struct TypeParamList *param_list) {
    if (b == NULL) {
        tabs();
        p("{ }");
        return;
    }

    set_src(b->source_line);
    tabs();
    p("{");
    NEWLINE();

    global_indent_level += 1;

    // if this block is part of a function, register the parameters in the symbol table
    if (param_list != NULL) {
        REWIND_LIST(param_list);
        while (param_list != NULL) {
            if (param_list->param != NULL && param_list->param->name != NULL) {
                push_symbol(sym_table, param_list->param->name, param_list->param->type, false /*is_global*/);
            }
            param_list = param_list->next;
        }
    }


    struct BlockList *node = b->contents;
    REWIND_LIST(node);
    while (node != NULL) {
        switch(node->item->tag) {
        case TypeDefinition:
            set_src(node->item->tdef->source_line);
            t_typedefinition(node->item->tdef, false /*top_level*/);


            // purely cosmetic newline
            set_src(node->item->decl->source_line);
            NEWLINE();
            break;
        case Declaration:
            set_src(node->item->decl->source_line);
            t_declaration(node->item->decl, false, false /*top_level*/);


            // purely cosmetic newline
            set_src(node->item->decl->source_line);
            NEWLINE();
            break;
        case Statement:
            set_src(node->item->stat->source_line);
             t_statement(node->item->stat);
            //NEWLINE();
            break;
        }
        node = node->next;
    }

    global_indent_level -= 1;

    set_src(b->source_line);
     tabs();
    p("}");
    NEWLINE();
}

// The type can be NULL
// It contains information from the type if available for inferrence:
// u32 a = 0; -> t_initializer(0, u32);
// u32 a = {0}; -> t_initializer({0}, u32);
// u32 a = {(u32){0}}; -> t_initializer({t_initializer()}, NULL);
// This is needed to translate function literals, as ${...} by itself can not be compiled
// without a **direct** inferrence type provided, and will result in a compilation error.
void t_initializer(struct Initializer *x, struct Type *t) {
    switch (x->tag) {
    case Expr:
        t_expr(x->expr, true /*inline_when_possible*/);
        break;
    case Data:
        p("{");
        // translate initializer list
        struct InitializerList *node = x->data;
        REWIND_LIST(node);

        while (node != NULL) {
            struct DesignatorList *desig_node = node->designation;
            if (desig_node != NULL) {
                REWIND_LIST(desig_node);
                while (desig_node != NULL) {
                    switch (desig_node->desig->tag) {
                    case Access:
                        p(".%s", desig_node->desig->access);
                        break;
                    case Index:
                        p("[");
                        t_expr(desig_node->desig->index->expr, true /*inline_when_possible*/);
                        p("]");
                        break;
                    }
                    desig_node = desig_node->next;
                }
                p("=");
            }
            t_initializer(node->current, NULL);
            node = node->next;
            if (node != NULL)
                p(", ");
        }

        p("}");

        break;
    case Code:
        if (t == NULL) {
            fprintf(stderr, "%s:%d:%d: Compiler error: Function initializer without explicit type\n",
                    FILENAME_GRACEFUL, x->source_line, 1);
            exit(1);
        }

        DISCARD_QUALIFIERS(t);
        if (t->tag != FunPointer) {
            fprintf(stderr, "%s:%d:%d: Compiler error: Explicit type of function initializer must be a function pointer %c\n",
                FILENAME_GRACEFUL, x->source_line, 1, t->tag);
            exit(1);
        }

        if (dry_run)
            return;

        // As explanied above, we create a new buffer to print to,
        // print the code to it, then restore the current buffer.
        int saved_indent = global_indent_level;
        global_indent_level = 0;

        struct BufferList *saved_buffer = current_buffer;
        struct BufferList *tmp = buffer_list;

        buffer_list = create_buffer();
        buffer_list->next = tmp;
        current_buffer = buffer_list;

        /*generate a unique identifier*/
        char *unique_temporary_identifier;
        asprintf(&unique_temporary_identifier, "_prec_anon_%d", global_identifier_counter);
        global_identifier_counter += 1;

        x->code_backchannel = unique_temporary_identifier;

        p("static ");

        char *decl = t_str_type(t, unique_temporary_identifier, true /*dereference function pointer*/);
        set_src(x->source_line);
        p("%s", decl);
        // print the code itself

        // TODO: save the symbols up to the global marker, pop them out of the stack
        // new global marker
        push_symbol(sym_table, NULL, NULL, true);
        t_block(x->code, t->fun_pointer.param_list);
        // TODO: push the saved symbols back in

        current_buffer = saved_buffer;
        global_indent_level = saved_indent;

        // This will have been set to true by t_block()
        newline_just_printed = false;
        t_expr(
            NEW_REFERENCE(
                NEW_IDENTIFIER(unique_temporary_identifier, x->source_line),
                x->source_line
            )
        );
        break;
    }
}

#undef t_expr
#define t_expr(_x, ...) t_expr(_x, __VA_ALT__(inline_when_possible, __VA_ARGS__))
struct Type *t_expr(struct Expr *x, bool inline_when_possible) {
    struct Type *t;
    struct Type *return_type = NULL;
    switch (x->tag) {
    case SizeofType:
        p("sizeof(");
        p("%s", t_str_type(x->sizeof_type, NULL, false));
        p(")");
        break;
    case Unary:
        p("(");
        switch(x->unOp.tag) {
        case Sizeof:
            p("sizeof(");
            t_expr(x->unOp.e);
            p(")");
            break;
        case Ref:
            p("&");
            t = t_expr(x->unOp.e, false /*inline_when_possible*/);
            
            return_type = DUP_T(Type, Reference,
                .reference = t,
                .source_line = (t != NULL) ? t->source_line : x->source_line
            );
            break;
        case Deref:
            p("*");
            t = t_expr(x->unOp.e);
            DISCARD_QUALIFIERS(t);
            if (t && t->tag == FunPointer) {
                return_type = t;
            } else if (t && t->tag == Reference) {
                return_type = t->reference;
            }
            break;
        case Neg:
            p("-");      t_expr(x->unOp.e);
            break;
        case Not:
            p("~");      t_expr(x->unOp.e);
            break;
        case BoolNot:
            p("!");      t_expr(x->unOp.e);
            break;
        }
        p(")");
        break;
    case Binary:
        p("(");
        switch(x->binOp.tag) {
        case Mul:
            t_expr(x->binOp.e1); p("*"); t_expr(x->binOp.e2);
            break;
        case Div:
            t_expr(x->binOp.e1); p("/"); t_expr(x->binOp.e2);
            break;
        case Mod:
            t_expr(x->binOp.e1); p("%c", '%'); t_expr(x->binOp.e2);
            break;
        case Add:
            struct Type *t1 = t_expr(x->binOp.e1);
            p("+");
            struct Type *t2 = t_expr(x->binOp.e2);

            // really approximate pointer arithmetic inference
            DISCARD_QUALIFIERS(t1);
            DISCARD_QUALIFIERS(t2);
            bool t1_ptr = t1 && (t1->tag == Reference || t1->tag == FunPointer);
            bool t2_ptr = t2 && (t2->tag == Reference || t2->tag == FunPointer);
            if (t1_ptr && !t2_ptr)
                return_type = t1;
            else if (t2_ptr && !t1_ptr)
                return_type = t2;

            break;
        case Sub:
            t1 = t_expr(x->binOp.e1);
            p("-");
            t2 = t_expr(x->binOp.e2);

            // really approximate pointer arithmetic inference
            DISCARD_QUALIFIERS(t1);
            DISCARD_QUALIFIERS(t2);
            t1_ptr = t1 && (t1->tag == Reference || t1->tag == FunPointer);
            t2_ptr = t2 && (t2->tag == Reference || t2->tag == FunPointer);
            if (t1_ptr && !t2_ptr)
                return_type = t1;
            else if (t2_ptr && !t1_ptr)
                return_type = t2;
            break;
        case And:
            t_expr(x->binOp.e1); p("&"); t_expr(x->binOp.e2);
            break;
        case BoolAnd:
            t_expr(x->binOp.e1); p("&&"); t_expr(x->binOp.e2);
            break;
        case Or:
            t_expr(x->binOp.e1); p("|"); t_expr(x->binOp.e2);
            break;
        case BoolOr:
            t_expr(x->binOp.e1); p("||"); t_expr(x->binOp.e2);
            break;
        case Xor:
            t_expr(x->binOp.e1); p("^"); t_expr(x->binOp.e2);
            break;
        case LeftShift:
            t_expr(x->binOp.e1); p("<<"); t_expr(x->binOp.e2);
            break;
        case RightShift:
            t_expr(x->binOp.e1); p(">>"); t_expr(x->binOp.e2);
            break;
        case Less:
            t_expr(x->binOp.e1); p("<"); t_expr(x->binOp.e2);
            break;
        case More:
            t_expr(x->binOp.e1); p(">"); t_expr(x->binOp.e2);
            break;
        case Equal:
            t_expr(x->binOp.e1); p("=="); t_expr(x->binOp.e2);
            break;
        case MoreEqual:
            t_expr(x->binOp.e1); p(">="); t_expr(x->binOp.e2);
            break;
        case LessEqual:
            t_expr(x->binOp.e1); p("<="); t_expr(x->binOp.e2);
            break;
        case NotEqual:
            t_expr(x->binOp.e1); p("!="); t_expr(x->binOp.e2);
            break;
        case Assign:
            t_expr(x->binOp.e1);
            p("=");
            return_type = t_expr(x->binOp.e2);
            break;
        case Sequence:
            t_expr(x->binOp.e1);
            p(",");
            return_type = t_expr(x->binOp.e2);
            break;
        case Index:
            t = t_expr(x->binOp.e1);
            DISCARD_QUALIFIERS(t);
            if (t && t->tag == Array) {
                return_type = t->array.t;
            }
            p("[");
                t_expr(x->binOp.e2);
            p("]");
            break;
        }
        p(")");
        break;
    case FunctionCall:
        t = t_expr(x->function_call.callee);
        DISCARD_QUALIFIERS(t);
        if (t && t->tag == FunPointer) {
            return_type = t->fun_pointer.return_type;
        }

        struct ArgumentExpressionList *curr = x->function_call.args;
        if (curr == NULL) {
            p("()");
            break;
        }

        p("(");

        REWIND_LIST(curr);

        while (curr != NULL) {
            t_expr(curr->expr);
            if (curr->next != NULL)
                p(",");
            curr = curr->next;
        }

        p(")");

        break;
    case String:
        p("%s", x->string);
        break;
    case Identifier:
        p("%s", x->identifier);
        // Don't bother with calculating the return type for prec anon stuff, they are not in the symbol table.
        if (strcmp(x->identifier, "_prec_anon") != 0) {
            return_type = fetch_symbol_type(sym_table, x->identifier);
        }
        break;
    case Float:
        p("%lf", x->fp_num);
        break;
    case Int:
        p("%lu", x->int_num);
        break;
    case UInt:
        p("%luU", x->uint_num);
        break;
    case Ternary:
        t_expr(x->ternary.cond);
        p("?");
        t_expr(x->ternary.if_true);
        p(":");
        t_expr(x->ternary.if_false);
        break;
    case Cast:
        p("("); p("%s", t_str_type(x->cast.type, NULL, false)); p(")");
        t_expr(x->cast.e);
        return_type = x->cast.type;
        break;
    case CompoundLiteral:
        p("("); p("%s", t_str_type(x->compound_literal.type, NULL, false)); p(")");
        t_initializer(x->compound_literal.init, x->compound_literal.type);
        return_type = x->compound_literal.type;
        break;
    case StructAccess:
    case StructDeref: {
        // Complete constdata detection using the type table.
        // dry run makes sure that nothing will actually be printed
        // TODO: this is a hack, the proper way is to pass dry run along the whole t_expr call tree
        // and not as a global variable, but it'll do for all cases of the programmer being non-evil for now
        bool saved_dr = dry_run;
        dry_run = true;
        t = t_expr(x->struct_access_deref.e);
        DISCARD_QUALIFIERS(t);

        bool is_pointer = false;

        if (x->tag == StructDeref) {
            is_pointer = true;
            
            if (t && t->tag == Reference)
                t = t->reference;
            DISCARD_QUALIFIERS(t);
        } else {
            if (t && t->tag == Reference) {
                t = t->reference;
                is_pointer = true;
            } else {
                is_pointer = false;
            }
            DISCARD_QUALIFIERS(t);
        }


        // if false, we need to perform a real run later
        bool is_constdata_access = false;
        bool constdata_inlined = false;
        struct Expr *constdata_inlined_value = NULL;

        if (t && t->tag == Struct) {
            if (t->tag_name != NULL) {
                TypeTablePtr entry = fetch_type(type_table, t->tag_name);
                if (entry != NULL) {
                    struct DeclarationList *decls = entry->constdata;

                    if (decls != NULL)
                        REWIND_LIST(decls);
                    while (!is_constdata_access && decls != NULL) {
                        struct VarList *vars = decls->decl->vars;

                        REWIND_LIST(vars);
                        while (!is_constdata_access && vars != NULL) {
                            if (strcmp(vars->decl->name, x->struct_access_deref.member) == 0) {
                                is_constdata_access = true;

                                struct Type *inline_type = decls->decl->type;
                                DISCARD_QUALIFIERS(inline_type);
                                if (inline_when_possible &&
                                    (   inline_type->tag == FunPointer
                                     || inline_type->tag == Reference
                                     || isBaseType(inline_type->tag))) {
                                    if (vars->decl->val == NULL) {
                                        constdata_inlined = true;
                                        constdata_inlined_value =
                                            NEW_CAST(
                                                NEW_INT(0, x->source_line),
                                                inline_type,
                                                x->source_line
                                            );
                                            return_type = inline_type;
                                    } else if (vars->decl->val->tag == Expr) {
                                        constdata_inlined = true;
                                        constdata_inlined_value =
                                            NEW_CAST(
                                                vars->decl->val->expr,
                                                inline_type,
                                                x->source_line
                                            );
                                            return_type = inline_type;

                                    } else if (vars->decl->val->tag == Code) {
                                        constdata_inlined = true;
                                        constdata_inlined_value =
                                            NEW_REFERENCE(
                                                NEW_IDENTIFIER(
                                                    vars->decl->val->code_backchannel,
                                                    x->source_line
                                                ),
                                                x->source_line
                                            );
                                            return_type = inline_type;
                                    } else {
                                        // fprintf(stderr, "Debug %c %s\n", vars->decl->val->tag, vars->decl->val->code_backchannel);
                                        // exit(1);
                                    }

                                }
                                dry_run = saved_dr;
                            }
                            vars = vars->next;
                        }
                        decls = decls->next;
                    }
                }
            }
        }
        dry_run = saved_dr;

        if (is_constdata_access && constdata_inlined) {
            p("(");
            t_expr(constdata_inlined_value);
            p(")");
        } else if (is_constdata_access && !constdata_inlined) {
            if (!constdata_inlined) {
                p("_prec_internal_constdata_struct_%s_%s",
                    t->tag_name, x->struct_access_deref.member);
            }
        } else {
            t = t_expr(x->struct_access_deref.e);

            if (is_pointer && !is_constdata_access) {
                p("->");
            } else {
                p(".");
            }

            // TODO: complete type inference (pass down the type of the access as return_type)
            //       using the type table too
            p("%s", x->struct_access_deref.member);
        }
        break;
    }
    case StructDerefMethod:
    case StructMethod: {
        bool saved_dr = dry_run;
        dry_run = true;
        t = t_expr(x->struct_access_deref.e);

        struct Type *original_type = t;

        DISCARD_QUALIFIERS(t);

        bool is_pointer = false;

        if (x->tag == StructDerefMethod) {
            is_pointer = true;
            
            if (t && t->tag == Reference)
                t = t->reference;
            DISCARD_QUALIFIERS(t);
        } else {
            if (t && t->tag == Reference) {
                t = t->reference;
                is_pointer = true;
            } else {
                is_pointer = false;
            }
            DISCARD_QUALIFIERS(t);
        }


        // if false, we need to perform a real run later
        bool is_constdata_access = false;

        if (t && t->tag == Struct) {
            if (t->tag_name != NULL) {
                TypeTablePtr entry = fetch_type(type_table, t->tag_name);
                if (entry != NULL) {
                    struct DeclarationList *decls = entry->constdata;
                    if (decls != NULL)
                        REWIND_LIST(decls);
                    while (!is_constdata_access && decls != NULL) {
                        struct VarList *vars = decls->decl->vars;
                        REWIND_LIST(vars);

                        struct Type *decl_type = decls->decl->type;
                        DISCARD_QUALIFIERS(decl_type);

                        while (!is_constdata_access && vars != NULL) {
                            if (strcmp(vars->decl->name, x->struct_access_deref.member) == 0) {
                                is_constdata_access = true;
                                dry_run = saved_dr;
                                p("_prec_internal_constdata_struct_%s_%s",
                                    t->tag_name, x->struct_access_deref.member);
                                if (decl_type->tag == FunPointer) {
                                    return_type = decls->decl->type->fun_pointer.return_type;
                                }
                            }
                            vars = vars->next;
                        }
                        decls = decls->next;
                    }
                }
            }
        }
        dry_run = saved_dr;
        if (!is_constdata_access) {
            t = t_expr(x->struct_access_deref.e);

            if (is_pointer) {
                p("->");
            } else {
                p(".");
            }

            // TODO: complete type inference (pass down the type of the access as return_type)
            //       using the type table too
            p("%s", x->struct_access_deref.member);
        }

        struct ArgumentExpressionList *curr = x->struct_access_deref.method_args;
        p("(");
        struct Expr *e = x->struct_access_deref.e;
        if (!is_pointer) {
            if (e->tag != Identifier) {
                // Wrapper struct to be able to take reference to an rvalue
                p("&(struct { %s; }){", t_str_type(original_type, "x", false ));
                t_expr(e);
                p("}.x");
            } else {
                p("&(");
                t_expr(e);
                p(")");
            }
        } else {
            t_expr(e);
        }

        if (curr != NULL) {
            p(",");

            REWIND_LIST(curr);
            while (curr != NULL) {
                t_expr(curr->expr);
                if (curr->next != NULL)
                    p(",");
                curr = curr->next;
            }

        }
        p(")");
        break;
        }
    }
    return return_type;
}
#undef t_expr
#define t_expr(_x, ...) t_expr(_x, __VA_ALT__(false, __VA_ARGS__))

bool is_const_expr(struct Expr *x) {
    if (x == NULL)
        return false;

    switch (x->tag) {
    case SizeofType:
        return true;
    case Unary:
        return is_const_expr(x->unOp.e);
    case Binary:
        return is_const_expr(x->binOp.e1) && is_const_expr(x->binOp.e2);
    case FunctionCall:
        return false;
    case String:
        return false;
    case Identifier:
        return false;
        break;
    case Float:
        return true;
    case Int:
        return true;
    case UInt:
        return true;
    case Ternary:
        return is_const_expr(x->ternary.cond)
            && is_const_expr(x->ternary.if_true)
            && is_const_expr(x->ternary.if_false);
    case Cast:
        return is_const_expr(x->cast.e);
    case CompoundLiteral:
        return false;
    case StructAccess:
        return false;
    case StructDeref:
        return false;
    case StructMethod:
        return false;
    case StructDerefMethod:
        return false;
    }
}

bool is_const_sized_type(struct Type *x) {
    if (x == NULL)
        assert(!"NULL argument to is_const_sized_type");

    switch (x->tag) {
    case Qualifier:
        return is_const_sized_type(x->qualifier.t);
    case Array:
        if (x->array.size == NULL) {
            return true;
        } else {
            return is_const_sized_type(x->array.t)
                && is_const_expr(x->array.size->expr);
        }
    case Reference:
    case FunPointer:
        return true;
    // Base types
    case CType:
    case f64:
    case f32:
    case u64:
    case i64:
    case u32:
    case i32:
    case u16:
    case i16:
    case u8:
    case i8:
    case uptr:
    case iptr:
    case Void:
    case Bool:
    case Union:
    case Struct:
    case Tuple:
    case Enum:
        return true;
    case TypeofExpr:
        return true;
    case TypeofType:
        return is_const_sized_type(x->typeof_type);
    }
}

void t_typedefinition(struct TypeDefinition *tdef, bool top_level) {
    switch(tdef->tag) {
    case NewStruct:
    case NewUnion:
        if (tdef->tag == NewStruct) {
            p("struct ");
        } else if (tdef->tag == NewUnion) {
            p("union ");
        }


        p("%s ", tdef->struct_or_union_def.name);

        /*TRANSLATE STRUCT/UNION BLOCK*/
        if (tdef->struct_or_union_def.declarations != NULL) {
            p("{");
            NEWLINE();
            global_indent_level += 1;

            /*Structs and unions can be forward declared:*/

            struct DeclarationList *node = tdef->struct_or_union_def.declarations;
            REWIND_LIST(node);
            while (node != NULL) {
                if (node->decl == NULL) {
                    node = node->next;
                    continue;
                }

                struct VarList *var_node = node->decl->vars;
                REWIND_LIST(var_node);
                while (var_node != NULL) {
                    set_src(var_node->source_line);

                    // Struct/union members are always implicitly mut.
                    // This is because the behaviour of const members in C structs is crazy:
                    //    local variables with a type that contains a struct that has ANY const member
                    //    can NEVER be reassigned easily.
                    // So, we just don't allow this behaviour.
                    if (node->decl->type->tag == Qualifier) {
                        node->decl->type->qualifier.qualifiers |= Mut;
                    } else {
                        node->decl->type =
                            QUALIFY(node->decl->type, Mut, node->decl->type->source_line)
                    }
                    tabs();
                    p("%s", t_str_type(node->decl->type, var_node->decl->name, false));
                    p(";");
                    NEWLINE();

                    var_node = var_node->next;
                }


                node = node->next;
            }

            global_indent_level -= 1;

            set_src(tdef->source_line);
            tabs();
            p("}");
        }
        break;
    case NewEnum:
        p("enum %s ", tdef->enum_def.name);

        /*TRANSLATE ENUM BLOCK*/
        if (tdef->enum_def.values != NULL) {
            p("{");
            NEWLINE()
            global_indent_level += 1;

            struct EnumeratorList *node = tdef->enum_def.values;
            REWIND_LIST(node);
            while (node != NULL) {
                set_src(node->val->source_line);
                tabs();
                p("%s", node->val->name);
                if (node->val->val != NULL) {
                    p("=");
                    t_expr(node->val->val->expr, true /*inline_when_possible*/);
                }
                if (node->next != NULL)
                    p(",");
                node = node->next;

                NEWLINE();
            }

            global_indent_level -= 1;

            set_src(tdef->source_line);
            tabs();
            p("}");
        }
        break;
    }


    if (tdef->tag == NewStruct) {
        struct DeclarationList *node_regulardata = tdef->struct_or_union_def.const_data;
        if (node_regulardata)
            REWIND_LIST(node_regulardata);
        struct DeclarationList *node_constdata = tdef->struct_or_union_def.const_data;
        if (node_constdata)
            REWIND_LIST(node_constdata);
        if (tdef->struct_or_union_def.name && node_regulardata) {
            // TODO: make sure we cull types every time we exit a scope,
            // for now we just insert, this won't fail to compile any
            // valid programs at least
            insert_type(type_table, tdef->struct_or_union_def.name,
                        node_regulardata, node_constdata,
                        global_indent_level);
            // Currently, constdata not supported for structs
            // declared along with variables in the same decl,
            // mostly because it's annoying to implement

            if (node_constdata != NULL) {
                // Create a _prec_internal_constdata_struct_ ## structname _ ## fieldname
                //    const global variable declaration
                //    for each field,
                //    that contains the constdata field, then
                //    set it to be initialized to the proper data.
                // Add the helper annotations for function-valued literals.

                struct TopLevel *head_of_inserted_constdata_fields = NULL;

                while (node_constdata != NULL) {
                    //struct Type *constdata_curr_decl_type = node_constdata->decl->type;
                    struct VarList *vars_node = node_constdata->decl->vars;

                    if (vars_node == NULL) {
                        node_constdata = node_constdata->next;
                        continue;
                    }

                    REWIND_LIST(vars_node);
                    while (vars_node != NULL) {

                        // build the top-level declaration
                        struct Declaration *constdata_field_decl =
                            DUP((struct Declaration){
                                .class = Static,
                                .type = node_constdata->decl->type,
                                .source_line = tdef->struct_or_union_def.const_data->source_line
                            });
                        DISCARD_QUALIFIERS(constdata_field_decl->type);

                        char *full_name;
                        // add one var
                        asprintf(&full_name, "_prec_internal_constdata_struct_%s_%s",
                            tdef->struct_or_union_def.name, vars_node->decl->name);
                        constdata_field_decl->vars = DUP((struct VarList){
                            .decl = DUP((struct VarDecl) {
                                .name = full_name,
                                .val = vars_node->decl->val,
                                .source_line = tdef->struct_or_union_def.const_data->source_line
                            }),
                            .prev = NULL,
                            .next = NULL,
                            .source_line = tdef->struct_or_union_def.const_data->source_line
                        });

                        // to translate code like this,
                        // the code must be able to access the type we're
                        // dealing with in the first place,
                        // else the ergonomics make no sense.
                        // so the symbol for this must be inserted AFTER the type.
                        // this must only be done in the case of top-level types.
                        // to achieve this, we will translate the declaration AFTER the current declaration,
                        // by inserting it for top_level or translating directly.
                        // TODO: As of C23, this can be achieved for non-top-level types as well
                        //       Because of the rules that allow for limited structural typing.
                        //       Maybe make a branch of the PreC transpiler that targets C23 in the future?

                        if (top_level) {
                            if (head_of_inserted_constdata_fields == NULL) {
                                struct TopLevel *saved_next = top_level_list->next;
                                top_level_list->next = DUP_T(TopLevel, Decl,
                                    .decl = constdata_field_decl,
                                    .prev = top_level_list,
                                    .source_line = constdata_field_decl->source_line
                                );
                                top_level_list->next->next = saved_next;
                                head_of_inserted_constdata_fields = top_level_list->next;
                            } else {
                                struct TopLevel *saved_next = head_of_inserted_constdata_fields->next;
                                head_of_inserted_constdata_fields->next = DUP_T(TopLevel, Decl,
                                    .decl = constdata_field_decl,
                                    .prev = top_level_list,
                                    .source_line = constdata_field_decl->source_line
                                );
                                head_of_inserted_constdata_fields->next->next = saved_next;
                                head_of_inserted_constdata_fields = head_of_inserted_constdata_fields->next;
                            }
                        } else {
                            t_declaration(constdata_field_decl, false /*freeform*/, false /*top_level*/);
                        }


                        vars_node = vars_node->next;
                    }
                    node_constdata = node_constdata->next;
                }
            }
        }
    }

    set_src(tdef->source_line);
    p(";");
}

/*freeform: no newlines and no indents*/
void t_declaration(struct Declaration *decl, bool freeform, bool top_level) {
    set_src(decl->source_line);

    char *storage_class;
    switch (decl->class) {
    case None:
        storage_class = "";
        break;
    case Static:
        storage_class = "static ";
        break;
    case Extern:
        storage_class = "extern ";
        break;
    }

    assert(decl->vars != NULL);


    struct VarList *node = decl->vars;
    REWIND_LIST(node);

    while (node != NULL) {
        if (!freeform) {
            tabs();
        }
        set_src(node->decl->source_line);
        if (node->decl->val != NULL) {
            // top level functions with no qualifiers and a function initializer get implicitly converted to declarations/definitions
            if (top_level && decl->type->tag == FunPointer && node->decl->val->tag == Code) {
                p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, true));

                push_symbol(sym_table, node->decl->name, decl->type, top_level);
                node->decl->val->code_backchannel = node->decl->name;

                // Empty marker on symbol stack
                push_symbol(sym_table, NULL, NULL, true /*is_global*/);

                set_src(node->decl->val->source_line);
                t_block(node->decl->val->code, decl->type->fun_pointer.param_list);
            } else {
                p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, false));
                p(" = ");
                set_src(node->decl->val->source_line);
                t_initializer(node->decl->val, decl->type);

                // TODO: make sure to cull symbols every time a scope is exited,
                // for now we just insert, this won't fail to compile any
                // valid programs at least
                push_symbol(sym_table, node->decl->name, decl->type, top_level);

                if (freeform) { p("; "); }
                else          { p(";"); NEWLINE(); }
            }
        } else {
            if (top_level && decl->type->tag == FunPointer) {
                p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, true));

                if (freeform) { p("; "); }
                else          { p(";"); NEWLINE(); }

                push_symbol(sym_table, node->decl->name, decl->type, top_level);
            } else {
                p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, false));

                // TODO: make sure to cull symbols every time a scope is exited,
                // for now we just insert, this won't fail to compile any
                // valid programs at least
                push_symbol(sym_table, node->decl->name, decl->type, top_level);

                // in preC, all non-extern variables are zero-initialized by default if no initializer is specified
                // Check if it's a VLA (if any of the array types contained within are not 100% constant expressions). If it's the case, do not print the initializer
                // as it's illegal C99 to initialize a VLA
                if (decl->class != Extern && is_const_sized_type(decl->type))
                    p(" = {0}");

                if (freeform) { p("; "); }
                else          { p(";"); NEWLINE(); }
            }
        }
        node = node->next;
    }
}

void t_statement(struct Statement *stat) {
    set_src(stat->source_line);
    switch (stat->tag) {
    case Block:
        t_block(stat->b, NULL);
        break;
    case Expr:
        tabs();
        if (stat->e != NULL)
            t_expr(stat->e);
        p(";"); NEWLINE();
        break;
    case Selection:
        switch (stat->s->tag) {
        case If:
            if (stat->s->simple_if.decl != NULL) {
                tabs();
                p("{");
                NEWLINE();
                global_indent_level += 1;
                t_declaration(stat->s->simple_if.decl, false, false);
            }

            tabs();
            p("if (")
            if (stat->s->simple_if.clause != NULL) {
                t_expr(stat->s->simple_if.clause);
            } else if (stat->s->simple_if.decl != NULL) {
                // If there's several variables in the declaration, this will always take the last
                // as the clause
                p("%s", stat->s->simple_if.decl->vars->decl->name);
            }
            p(")");
            NEWLINE();


            if (stat->s->simple_if.action->tag == Block) {
                t_statement(stat->s->simple_if.action);
            } else {
                global_indent_level += 1;
                t_statement(stat->s->simple_if.action);
                global_indent_level -= 1;
            }


            if (stat->s->simple_if.decl != NULL) {
                global_indent_level -= 1;
                NEWLINE();
                set_src(stat->s->source_line);

                tabs();
                p("}");
                NEWLINE();
            }
            break;
        case IfElse:
            if (stat->s->if_else.decl != NULL) {
                tabs();
                p("{");
                NEWLINE();
                set_src(stat->s->source_line);

                global_indent_level += 1;
                t_declaration(stat->s->if_else.decl, false, false);
            }

            tabs();
            p("if (")
            if (stat->s->if_else.clause != NULL) {
                t_expr(stat->s->if_else.clause);
            } else if (stat->s->if_else.decl != NULL) {
                // If there's several variables in the declaration, this will always take the last
                // as the clause
                p("%s", stat->s->if_else.decl->vars->decl->name);
            }
            p(")");
            NEWLINE();


            if (stat->s->if_else.action_true->tag == Block) {
                t_statement(stat->s->if_else.action_true);
            } else {
                global_indent_level += 1;
                t_statement(stat->s->if_else.action_true);
                global_indent_level -= 1;
            }

            set_src(stat->s->if_else.action_false->source_line);
            tabs();
            p("else");
            NEWLINE();
            if (stat->s->if_else.action_false->tag == Block ||
                (stat->s->if_else.action_false->tag == Selection &&
                    (stat->s->if_else.action_false->s->tag == If
                    || stat->s->if_else.action_false->s->tag == IfElse))) {
                t_statement(stat->s->if_else.action_false);
            } else {
                global_indent_level += 1;
                t_statement(stat->s->if_else.action_false);
                global_indent_level -= 1;
            }


            if (stat->s->if_else.decl != NULL) {
                global_indent_level -= 1;
                NEWLINE();
                set_src(stat->s->source_line);

                tabs();
                p("}");
                NEWLINE();
            }

            break;
        case Switch:
            if (stat->s->switch_stat.decl != NULL) {
                tabs();
                p("{");
                NEWLINE();
                global_indent_level += 1;
                t_declaration(stat->s->switch_stat.decl, false, false);
            }

            tabs();
            p("switch (")
            if (stat->s->switch_stat.clause != NULL) {
                t_expr(stat->s->switch_stat.clause);
            } else if (stat->s->switch_stat.decl != NULL) {
                // If there's several variables in the declaration, this will always take the last
                // as the clause
                p("%s", stat->s->switch_stat.decl->vars->decl->name);
            }
            p(")");
            NEWLINE();
            if (stat->s->switch_stat.block->tag == Block) {
                t_statement(stat->s->switch_stat.block);
            } else {
                global_indent_level += 1;
                t_statement(stat->s->switch_stat.block);
                global_indent_level -= 1;
            }

            if (stat->s->switch_stat.decl != NULL) {
                global_indent_level -= 1;
                NEWLINE();
                set_src(stat->s->source_line);

                tabs();
                p("}");
                NEWLINE();
            }
        }
        break;
    case Jump:
        switch (stat->j->tag) {
        case Return:
            tabs();
            p("return");

            if (stat->j->return_stat.expr != NULL) {
                p(" ");
                t_expr(stat->j->return_stat.expr);
            }
            p(";");
            NEWLINE();
            break;
        case Goto:
            tabs();
            p("goto %s;", stat->j->goto_stat.label_name);
            NEWLINE();
            break;
        case Break:
            tabs();
            p("break;")
            NEWLINE();
            break;
        case Continue:
            tabs();
            p("continue;")
            NEWLINE();
            break;
        }
        break;
    case Labeled:
        switch (stat->l->tag) {
        case Case:
            global_indent_level -= 1;

            tabs();
            p("case ");
            t_expr(stat->l->case_expr->expr, true /*inline_when_possible*/);
            p(":");
            NEWLINE();

            global_indent_level += 1;

            t_statement(stat->l->stat);

            set_src(stat->source_line);
            tabs();
            p("break;")
            NEWLINE();
            break;
        case CaseFall:
            global_indent_level -= 1;

            tabs();
            p("case ");
            t_expr(stat->l->case_expr->expr, true /*inline_when_possible*/);
            p(":");
            NEWLINE();

            global_indent_level += 1;

            t_statement(stat->l->stat);
            break;
        case Default_Label:
            global_indent_level -= 1;
            tabs();
            p("default:");
            NEWLINE();
            global_indent_level += 1;

            t_statement(stat->l->stat);
            break;
        case Label:
            tabs();
            p("%s:", stat->l->label_name);
            NEWLINE();
            t_statement(stat->l->stat);
            break;
        }
        break;
    case Iteration:
        switch (stat->i->tag) {
        case While:
            tabs();
            p("while (");
            t_expr(stat->i->while_dowhile_stat.expr);
            p(")");
            NEWLINE();
            if (stat->i->while_dowhile_stat.stat->tag == Block) {
                t_statement(stat->i->while_dowhile_stat.stat);
            } else {
                global_indent_level += 1;
                t_statement(stat->i->while_dowhile_stat.stat);
                global_indent_level -= 1;
            }
            break;
        case DoWhile:
            tabs();
            p("do");
            NEWLINE();
            if (stat->i->while_dowhile_stat.stat->tag == Block) {
                t_statement(stat->i->while_dowhile_stat.stat);
            } else {
                global_indent_level += 1;
                t_statement(stat->i->while_dowhile_stat.stat);
                global_indent_level -= 1;
            }
            tabs();
            p("while (");
            t_expr(stat->i->while_dowhile_stat.expr);
            p(");");
            NEWLINE();
            break;
        case For_Decl:
            tabs();
            p("for (");

            struct VarList *vars = stat->i->for_stat_decl.init->vars;
            if (vars != NULL && vars->prev != NULL) {
                fprintf(stderr, "%s:%d:%d: Compiler error: Initializer of for loop can only have one variable.\n"
                                "                If you must have several loop variables, "
                                "please declare them in the outer scope.\n",
                        FILENAME_GRACEFUL, vars->source_line, 1);
                exit(1);
            }

            t_declaration(stat->i->for_stat_decl.init, true /*freeform: no newlines and no indents*/, false /*top_level*/);

            if (stat->i->for_stat_decl.clause != NULL)
                t_expr(stat->i->for_stat_decl.clause);
            p("; ");

            if (stat->i->for_stat_decl.update != NULL)
                t_expr(stat->i->for_stat_decl.update);

            p(")");

            if (stat->i->for_stat_decl.stat->tag == Block) {
                t_statement(stat->i->for_stat_decl.stat);
            } else {
                global_indent_level += 1;
                t_statement(stat->i->for_stat_decl.stat);
                global_indent_level -= 1;
            }

            break;
        case For_Expr:
            tabs();
            p("for (");
            t_expr(stat->i->for_stat_expr.init);
            p("; ");
            t_expr(stat->i->for_stat_expr.clause);
            p("; ");
            if (stat->i->for_stat_expr.update != NULL)
                t_expr(stat->i->for_stat_expr.update);

            p(")");

            if (stat->i->for_stat_expr.stat->tag == Block) {
                t_statement(stat->i->for_stat_expr.stat);
            } else {
                global_indent_level += 1;
                t_statement(stat->i->for_stat_expr.stat);
                global_indent_level -= 1;
            }

            break;
        }
    }
}

#define RESET_TRANSLATION_DATA() buffer_list = NULL; current_buffer = NULL; global_indent_level = 0; buffer_list = create_buffer(); current_buffer = buffer_list;

#define CLEAR_TRANSLATION_DATA() buffer_list = NULL; current_buffer = NULL; global_indent_level = 0;



void transpile(struct TopLevel *top) {
    printf("#include \"stdint.h\"\n");
    if (pretty_filename != NULL)
        printf("#line 1 \"%s\"\n", pretty_filename);

    sym_table = new_symbol_table();
    type_table = new_type_table();
    top_level_list = top;
    REWIND_LIST(top_level_list);
    while (top_level_list != NULL) {
        switch (top_level_list->tag) {
        case CInclude:
            printf("\n#include %s", top_level_list->c_include);
            printf("\n");
            newline_just_printed = true;
            break;
        case Stat:
            RESET_TRANSLATION_DATA();

            set_src(top_level_list->stat->source_line);
            t_statement(top_level_list->stat);
            print_buffer_list(buffer_list);
            destroy_buffer_list(buffer_list);

            CLEAR_TRANSLATION_DATA();
            break;
        case TDef:
            RESET_TRANSLATION_DATA();

            set_src(top_level_list->tdef->source_line);
            t_typedefinition(top_level_list->tdef, true /*top_level*/);
            print_buffer_list(buffer_list);
            destroy_buffer_list(buffer_list);

            CLEAR_TRANSLATION_DATA();
            break;
        case Decl:
            RESET_TRANSLATION_DATA();

            set_src(top_level_list->decl->source_line);
            t_declaration(top_level_list->decl, false, true /*top_level*/);
            print_buffer_list(buffer_list);
            destroy_buffer_list(buffer_list);

            CLEAR_TRANSLATION_DATA();
            break;
        }
        top_level_list = top_level_list->next;
    }
}
