#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
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


// The basic idea is to used memory mapped buffers:
/*
FILE *f = fmemopen(buffer, sizeof(buffer), "w");
fprintf(f, "mystring");
fclose(f);
*/

struct BufferList {
    char *current_buf;
    FILE *current_stream;
    struct BufferList *next;
};

struct BufferList *buffer_list;

struct BufferList *current_buffer;


struct BufferList *create_buffer(void) {
    struct BufferList *retval = malloc(sizeof(struct BufferList));
    unsigned size = 1 << 16;
    retval->current_buf = calloc(size, 1);
    retval->current_stream = fmemopen(retval->current_buf, size, "w");
    return retval;
}

void print_buffer_list(struct BufferList *list) {
    struct BufferList *curr = list;
    while (curr != NULL) {
        printf("%s", curr->current_buf);
        curr = curr->next;
    }
}

void destroy_buffer_list(struct BufferList *list) {
    struct BufferList *curr = list;
    while (curr != NULL) {
        fclose(curr->current_stream);
        free(curr->current_buf);
        struct BufferList *old = curr;
        curr = curr->next;
        free(old);
    }
}

#define p(...) fprintf(current_buffer->current_stream, __VA_ARGS__)

#define pt(...) fprintf(stream, __VA_ARGS__)

// t_XXX functions transpile directly to the buffer
// t_str_XXX functions return a transpiled string


// if identifier is NULL, an abstract generator will be generated

// main resource used: http://unixwiz.net/techtips/reading-cdecl.html

void t_internal_type(struct Type *x, FILE *stream) {
}


char *t_str_type(struct Type *x, char *identifier) {
    unsigned size = 1 << 12;

    char *buffer = calloc(size, 1);

    FILE *buffer_stream = fmemopen(buffer, size, "w");

    
    t_internal_type(x, buffer_stream);

}

void t_initializer(struct Initializer *x) {
    
}

void t_expr(struct Expr *x) {
    switch (x->tag) {
    case SizeofType:
        p("sizeof(");
        p("%s", t_str_type(x->sizeof_type, NULL));
        p(")");
        break;
    case Unary:
        switch(x->unOp.tag) {
        case Sizeof:
            p("sizeof("); t_expr(x->unOp.e); p(")");
            break;
        case Ref:
            p("&(");      t_expr(x->unOp.e); p(")");
            break;
        case Deref:
            p("*(");      t_expr(x->unOp.e); p(")");
            break;
        case Neg:
            p("-(");      t_expr(x->unOp.e); p(")");
            break;
        case Not:
            p("~(");      t_expr(x->unOp.e); p(")");
            break;
        case BoolNot:
            p("!(");      t_expr(x->unOp.e); p(")");
            break;
        case PreIncrement:
            p("++(");     t_expr(x->unOp.e); p(")");
            break;
        case PreDecrement:
            p("--(");     t_expr(x->unOp.e); p(")");
            break;
        case PostIncrement:
            p("(");       t_expr(x->unOp.e); p(")++");
            break;
        case PostDecrement:
            p("(");       t_expr(x->unOp.e); p(")--");
            break;
        }
    case Binary:
        switch(x->binOp.tag) {
        case Mul:
            p("("); t_expr(x->binOp.e1); p(")");
            p("*");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Div:
            p("("); t_expr(x->binOp.e1); p(")");
            p("/");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Mod:
            p("("); t_expr(x->binOp.e1); p(")");
            p("%c", '%');
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Add:
            p("("); t_expr(x->binOp.e1); p(")");
            p("+");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Sub:
            p("("); t_expr(x->binOp.e1); p(")");
            p("-");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case And:
            p("("); t_expr(x->binOp.e1); p(")");
            p("&");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case BoolAnd:
            p("("); t_expr(x->binOp.e1); p(")");
            p("&&");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Or:
            p("("); t_expr(x->binOp.e1); p(")");
            p("|");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case BoolOr:
            p("("); t_expr(x->binOp.e1); p(")");
            p("||");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Xor:
            p("("); t_expr(x->binOp.e1); p(")");
            p("^");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case LeftShift:
            p("("); t_expr(x->binOp.e1); p(")");
            p("<<");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case RightShift:
            p("("); t_expr(x->binOp.e1); p(")");
            p(">>");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Less:
            p("("); t_expr(x->binOp.e1); p(")");
            p("<");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case More:
            p("("); t_expr(x->binOp.e1); p(")");
            p(">");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Equal:
            p("("); t_expr(x->binOp.e1); p(")");
            p("==");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case MoreEqual:
            p("("); t_expr(x->binOp.e1); p(")");
            p(">=");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case LessEqual:
            p("("); t_expr(x->binOp.e1); p(")");
            p("<=");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case NotEqual:
            p("("); t_expr(x->binOp.e1); p(")");
            p("!=");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Assign:
            p("("); t_expr(x->binOp.e1); p(")");
            p("=");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Sequence:
            p("("); t_expr(x->binOp.e1); p(")");
            p(",");
            p("("); t_expr(x->binOp.e2); p(")");
            break;
        case Index:
            p("("); t_expr(x->binOp.e1); p(")");
            p("[");
                p("("); t_expr(x->binOp.e2); p(")");
            p("]");
            break;
        }
    case FunctionCall:
        t_expr(x->function_call.callee);
        p("(");
        struct ArgumentExpressionList *curr = x->function_call.args;
        assert(curr != NULL);
        t_expr(curr->expr);
        curr = curr->next;
        while (curr != NULL && curr->next != NULL) {
            if (curr->expr == NULL) {
                p(", ...");
            }
            else {
                p(","); t_expr(curr->expr);
            }
            curr = curr->next;
        }
        p(")");
        break;
    case String:
        p("\"");
        p("%s", x->string);
        p("\"");
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
        p("("); t_expr(x->ternary.cond); p(")");
        printf("?");
        p("("); t_expr(x->ternary.if_true); p(")");
        printf(":");
        p("("); t_expr(x->ternary.if_false); p(")");
        break;
    case Cast:
        p("("); p("%s", t_str_type(x->cast.type, NULL)); p(")");
        p("("); t_expr(x->cast.e); p(")");
        break;
    case CompoundLiteral:
        p("("); p("%s", t_str_type(x->compound_literal.type, NULL)); p(")");
        t_initializer(x->compound_literal.init);
        break;
    case StructAccess:
        p("("); t_expr(x->struct_access_deref.e); p(")");
        p(".");
        p("%s", x->struct_access_deref.member);
        break;
    case StructDeref:
        p("("); t_expr(x->struct_access_deref.e); p(")");
        p("->");
        p("%s", x->struct_access_deref.member);
        break;
    }
}

void transpile(struct TopLevel *top) {
    p("[PLACEHOLDER] Address of top level AST node: %p\n", top);
}
