#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <err.h>
#include "prec_ast.h"
#include "prec_transpiler.h"


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

struct BufferList {
    size_t size;
    char *buf;
    FILE *stream;
    struct BufferList *next;
};

struct BufferList *buffer_list;

struct BufferList *current_buffer;

#define REWIND_LIST(_name) do { while (_name->prev != NULL) { _name = _name->prev; } } while(0)

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

#define p(...) {\
    int s = fprintf(current_buffer->stream, __VA_ARGS__);\
    fflush(current_buffer->stream);\
    if (s == -1)\
        err(EXIT_FAILURE, "fprintf");\
}

#define INDENT_STR "  "
void tabs(void) {
    for (int i = 0; i < global_indent_level; i++)
        p(INDENT_STR);
}

void tabs_custom(FILE *stream) {
    for (int i = 0; i < global_indent_level; i++) {
        int s = fprintf(stream, INDENT_STR);
        fflush(current_buffer->stream);
        if (s == -1)
            err(EXIT_FAILURE, "fprintf");
    }
}

// t_XXX functions transpile directly to the buffer
// t_str_XXX functions return a transpiled string


// main resource used: http://unixwiz.net/techtips/reading-cdecl.html

void t_expr(struct Expr *x);

// a return value of 2 instead of 1 indicates that it's a void type, and must hence NOT be qualified
bool isBaseType(enum TypeSort x) {
    return x == TypeofExpr
        || x == TypeofType
        || x == Struct
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

        t_expr(expression);

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
            //    type_buffer->left_buffer+type_buffer->left_buffer_pos);
            //assert(false);
            return;
        }

        // insert qualifiers
        if (is_const) {
            // If there's already a const, do nothing
            size_t size = strlen(type_buffer->left_buffer+type_buffer->left_buffer_pos);

            if (((int)type_buffer->left_buffer_pos+(int)size-(int)strlen("const")-1) >= 0
                &&
                strncmp(type_buffer->left_buffer+type_buffer->left_buffer_pos+size-strlen("const")-1,
                        "const",
                        strlen("const")) == 0)
            {
                ;
            } else {
                str_insert(type_buffer->left_buffer+type_buffer->left_buffer_pos, "const ", pos);
            }

        } else {
            // If there's a const, remove it
            size_t size = strlen(type_buffer->left_buffer+type_buffer->left_buffer_pos);

            if (((int)type_buffer->left_buffer_pos+(int)size-(int)strlen("const")-1) >= 0
                &&
                strncmp(type_buffer->left_buffer+type_buffer->left_buffer_pos+size-strlen("const")-1,
                        "const",
                        strlen("const")) == 0)
            {
                memcpy(type_buffer->left_buffer+type_buffer->left_buffer_pos+size-strlen("const")-1, "               ", strlen("const"));
            }
        }

        if (is_volatile)
            str_insert(type_buffer->left_buffer+type_buffer->left_buffer_pos, "volatile ", pos);
        if (is_restrict)
            str_insert(type_buffer->left_buffer+type_buffer->left_buffer_pos, "restrict ", pos);
    }
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

        dispatch_array(type_buffer, x->array.size->expr);
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
                    fprintf(stderr, "Compiler error: Function definitions can't have more than 64 arguments (uh, wtf?)\n");
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
    case u64: p_t("long unsigned"); break;
    case i64: p_t("long signed"); break;
    case u32: p_t("unsigned"); break;
    case i32: p_t("signed"); break;
    case u16: p_t("short unsigned"); break;
    case i16: p_t("short signed"); break;
    case u8: p_t("unsigned char"); break;
    case i8: p_t("signed char"); break;
    case Void: {
        // refuse to qualify void types, or rather qualify as special value
        type_buffer->base_type_qualifiers = 1 << 3;

        p_t("void");
        break;
    }
    case Bool: p_t("_Bool"); break;
    case TypeofExpr: {
        p_t("typeof(");

        struct BufferList *saved_buffer = current_buffer;

        current_buffer = &(struct BufferList){ .stream = type_buffer->stream };

        t_expr(x->typeof_expr);

        current_buffer = saved_buffer;

        p_t(")");
        break;
    }
    case TypeofType: {
        p_t("typeof<%s>", t_str_type(x->typeof_type, NULL, false));
        break;
    }
    case Struct:
    case Union:
        if (x->tag == Struct) {
            p_t("struct ");
        } else {
            p_t("union ");
        }

        if (x->struct_or_union_def.name != NULL) {
            p_t("%s ", x->struct_or_union_def.name);
        }

        if (x->struct_or_union_def.declarations != NULL) {
            p_t("{\n");
            global_indent_level += 1;

            struct ConstDeclarationList *node = x->struct_or_union_def.declarations;
            REWIND_LIST(node);
            while (node != NULL) {
                if (node->decl == NULL) {
                    node = node->next;
                    continue;
                }
                
                struct ConstVarList *var_node = node->decl->vars;
                if (var_node != NULL) {
                    REWIND_LIST(var_node);
                    while (var_node != NULL) {
                        struct BufferList *saved_buffer = current_buffer;

                        current_buffer = &(struct BufferList){ .stream = type_buffer->stream };

                        // Struct members are always implicitly mut
                        if (node->decl->type->tag == Qualifier) {
                            node->decl->type->qualifier.qualifiers |= Mut;
                        } else {
                            node->decl->type = DUP_T(Type, Qualifier, .qualifier = { .qualifiers = Mut, .t = node->decl->type});
                        }
                        tabs_custom(type_buffer->stream);
                        p_t("%s", t_str_type(node->decl->type, var_node->decl->name, false));
                        p_t(";\n");

                        current_buffer = saved_buffer;
                        
                        var_node = var_node->next;
                    }
                } else {
                    tabs_custom(type_buffer->stream);
                    p_t("%s", t_str_type(node->decl->type, NULL, false));
                    p_t(";\n");
                }

                node = node->next;
            }

            global_indent_level -= 1;

            tabs_custom(type_buffer->stream);
            p_t("}");
        }
        break;
    case Enum:
        p_t("enum ");
        if (x->enum_def.name != NULL) {
            p_t("%s ", x->enum_def.name);
        }
        if (x->enum_def.values != NULL) {
            p_t("{\n");
            global_indent_level += 1;

            struct EnumeratorList *node = x->enum_def.values;
            REWIND_LIST(node);
            while (node != NULL) {
                tabs_custom(type_buffer->stream);
                p_t("%s", node->val->name);
                if (node->val->val != NULL) {
                    p_t("=");

                    struct BufferList *saved_buffer = current_buffer;

                    current_buffer = &(struct BufferList){ .stream = type_buffer->stream };

                    t_expr(node->val->val->expr);

                    current_buffer = saved_buffer;
                }
                if (node->next != NULL)
                    p_t(",");
                node = node->next;

                p_t("\n");
            }

            global_indent_level -= 1;

            tabs_custom(type_buffer->stream);
            p_t("}");
        }
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
        size_t size = strlen(type_buffer->left_buffer+type_buffer->left_buffer_pos);

        assert(((int)type_buffer->left_buffer_pos+(int)size-(int)strlen("*const")-1) >= 0);
        if (strncmp(type_buffer->left_buffer+type_buffer->left_buffer_pos+size-strlen("*const")-1,
                    "*const",
                    strlen("*const")) != 0) {
            fprintf(stderr, "Compiler error: Function must be dereferenced but can't.\n");
            exit(1);
        }
        memcpy(type_buffer->left_buffer+type_buffer->left_buffer_pos+size-strlen("*const")-1, "               ", strlen("*const"));

    }
    p_t("%s", type_buffer->left_buffer+type_buffer->left_buffer_pos);
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

void t_block(struct Block *b) {
    if (b == NULL) {
        tabs();
        p("{ }");
        return;
    }

    tabs();
    p("{\n");

    global_indent_level += 1;

    struct BlockList *node = b->contents;
    REWIND_LIST(node);
    while (node != NULL) {
        switch(node->item->tag) {
        case Declaration:
            t_declaration(node->item->decl, false, false /*top_level*/);
            p("\n");
            break;
        case Statement:
            t_statement(node->item->stat);
            //p("\n");
            break;
        }
        node = node->next;
    }

    global_indent_level -= 1;

    tabs();
    p("}\n");
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
        t_expr(x->expr);
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
                        t_expr(desig_node->desig->index->expr);
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
            fprintf(stderr, "Compiler error: Function initializer without explicit type\n");
            exit(1);
        }

        if (t->tag == Qualifier) {
            t = t->qualifier.t;
        }
        if (t->tag != FunPointer) {
            fprintf(stderr, "Compiler error: Explicit type of function initializer must be a function pointer %c\n", t->tag);
            exit(1);
        }

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

        p("static ");

        char *decl = t_str_type(t, unique_temporary_identifier, true /*dereference function pointer*/);
        p("%s", decl);
        // print the code itself
        t_block(x->code);

        current_buffer = saved_buffer;
        global_indent_level = saved_indent;
        p("&%s", unique_temporary_identifier);
        break;
    }
}

void t_expr(struct Expr *x) {
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
            p("sizeof("); t_expr(x->unOp.e);
            break;
        case Ref:
            p("&");      t_expr(x->unOp.e);
            break;
        case Deref:
            p("*");      t_expr(x->unOp.e);
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
        case PreIncrement:
            p("++");     t_expr(x->unOp.e);
            break;
        case PreDecrement:
            p("--");     t_expr(x->unOp.e);
            break;
        case PostIncrement:
                   t_expr(x->unOp.e); p("++");
            break;
        case PostDecrement:
                   t_expr(x->unOp.e); p("--");
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
            t_expr(x->binOp.e1); p("+"); t_expr(x->binOp.e2);
            break;
        case Sub:
            t_expr(x->binOp.e1); p("-"); t_expr(x->binOp.e2);
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
            t_expr(x->binOp.e1); p("="); t_expr(x->binOp.e2);
            break;
        case Sequence:
            t_expr(x->binOp.e1); p(","); t_expr(x->binOp.e2);
            break;
        case Index:
            t_expr(x->binOp.e1);
            p("[");
                t_expr(x->binOp.e2);
            p("]");
            break;
        }
        p(")");
        break;
    case FunctionCall:
        t_expr(x->function_call.callee);
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
        break;
    case Float:
        p("%lf", x->fp_num);
        break;
    case Int:
        p("%ld", x->int_num);
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
        break;
    case CompoundLiteral:
        p("("); p("%s", t_str_type(x->compound_literal.type, NULL, false)); p(")");
        t_initializer(x->compound_literal.init, x->compound_literal.type);
        break;
    case StructAccess:
        t_expr(x->struct_access_deref.e);
        p(".");
        p("%s", x->struct_access_deref.member);
        break;
    case StructDeref:
        t_expr(x->struct_access_deref.e);
        p("->");
        p("%s", x->struct_access_deref.member);
        break;
    }
}

/*freeform: no newlines and no indents*/
void t_declaration(struct Declaration *decl, bool freeform, bool top_level) {
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

    if (decl->vars == NULL) {
        p("%s%s;", storage_class, t_str_type(decl->type, NULL, false));
        return;
    }

    struct VarList *node = decl->vars;
    REWIND_LIST(node);

    while (node != NULL) {
        if (!freeform) {
            tabs();
        }
        if (node->decl->val != NULL) {
            // top level functions with no qualifiers and a function initializer get implicitly converted to declarations/definitions
            if (top_level && decl->type->tag == FunPointer && node->decl->val->tag == Code) {
                p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, true));
                t_block(node->decl->val->code);
            } else {
                p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, false));
                p(" = ");
                t_initializer(node->decl->val, decl->type);

                if (freeform) { p("; "); }
                else          { p(";\n"); }
            }
        } else {
            if (top_level && decl->type->tag == FunPointer) {
                p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, true));

                if (freeform) { p("; "); }
                else          { p(";\n"); }
            } else {
                p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, false));
                if (freeform) { p("; "); }
                else          { p(";\n"); }
            }
        }
        node = node->next;
    }
}

void t_statement(struct Statement *stat) {
    switch (stat->tag) {
    case Block:
        t_block(stat->b);
        break;
    case Expr:
        tabs();
        if (stat->e != NULL)
            t_expr(stat->e);
        p(";\n");
        break;
    case Selection:
        switch (stat->s->tag) {
        case If:
            tabs();
            p("if (");
            t_expr(stat->s->simple_if.clause);
            p(")\n");
            if (stat->s->simple_if.action->tag == Block) {
                t_statement(stat->s->simple_if.action);
            } else {
                global_indent_level += 1;
                t_statement(stat->s->simple_if.action);
                global_indent_level -= 1;
            }
            break;
        case IfElse:
            tabs();
            p("if (");
            t_expr(stat->s->if_else.clause);
            p(")\n");
            if (stat->s->if_else.action_true->tag == Block) {
                t_statement(stat->s->if_else.action_true);
            } else {
                global_indent_level += 1;
                t_statement(stat->s->if_else.action_true);
                global_indent_level -= 1;
            }

            tabs();
            p("else\n");
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
            break;
        case Switch:
            tabs();
            p("switch (")
            t_expr(stat->s->switch_stat.clause);
            p(")\n");
            if (stat->s->switch_stat.block->tag == Block) {
                t_statement(stat->s->switch_stat.block);
            } else {
                global_indent_level += 1;
                t_statement(stat->s->switch_stat.block);
                global_indent_level -= 1;
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
            p(";\n");
            break;
        case Goto:
            tabs();
            p("goto %s;\n", stat->j->goto_stat.label_name);
            break;
        case Break:
            tabs();
            p("break;\n")
            break;
        case Continue:
            tabs();
            p("continue;\n")
            break;
        }
        break;
    case Labeled:
        switch (stat->l->tag) {
        case Case:
            global_indent_level -= 1;

            tabs();
            p("case ");
            t_expr(stat->l->case_expr->expr);
            p(":\n");

            global_indent_level += 1;

            t_statement(stat->l->stat);
            break;
        case Default_Label:
            global_indent_level -= 1;
            tabs();
            p("default:\n");
            global_indent_level += 1;

            t_statement(stat->l->stat);
            break;
        case Label:
            tabs();
            p("%s:\n", stat->l->label_name);
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
            p(")\n");
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
            p("do\n");
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
            p(");\n");
            break;
        case For_Decl:
            tabs();
            p("for (");

            struct VarList *vars = stat->i->for_stat_decl.init->vars;
            if (vars != NULL && vars->prev != NULL) {
                fprintf(stderr, "Compiler error: Initializer of for loop can only have one variable.\n"
                                "                If you must have several loop variables, "
                                "please declare them in the outer scope.\n");
                exit(1);
            }
            
            t_declaration(stat->i->for_stat_decl.init, true /*freeform: no newlines and no indents*/, false /*top_level*/);
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

void transpile(struct TopLevel *top) {
    REWIND_LIST(top);
    while (top != NULL) {
        switch (top->tag) {
        case CInclude:
            printf("\n#include %s\n", top->c_include);
            break;
        case Decl:
            buffer_list = NULL;
            current_buffer = NULL;
            global_indent_level = 0;

            buffer_list = create_buffer();
            current_buffer = buffer_list;
            t_declaration(top->decl, false, true /*top_level*/);
            print_buffer_list(buffer_list);
            destroy_buffer_list(buffer_list);

            buffer_list = NULL;
            current_buffer = NULL;
            global_indent_level = 0;
            break;
        }
        top = top->next;
    }
}
