#include <stdio.h>
#include <stdlib.h>
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


// TODO: top-level declarations of function pointers with no qualifiers **MUST** be turned into
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

unsigned global_identifier_counter = 0;

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
    struct BufferList *retval = malloc(sizeof(struct BufferList));
    retval->size = 0;
    retval->buf = NULL;
    retval->stream = open_memstream(&retval->buf, &retval->size);
    //setbuf(retval->stream, NULL);
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

#define p(...) { int s = fprintf(current_buffer->stream, __VA_ARGS__); fflush(current_buffer->stream); if (s == -1) err(EXIT_FAILURE, "fprintf"); } ;

// t_XXX functions transpile directly to the buffer
// t_str_XXX functions return a transpiled string


// main resource used: http://unixwiz.net/techtips/reading-cdecl.html

void t_internal_type(struct Type *x, FILE *stream) {
}

// - If identifier is NULL, an abstract generator will be generated.
// - If fun_pointer_dereferenced is true, and the type is a function pointer,
//   then the translation will be done as if it was a function instead of a pointer (no innermost pointer).
char *t_str_type(struct Type *x, char *identifier, bool fun_pointer_dereferenced) {
    size_t size = 0;
    char *buffer = NULL;
    FILE *buffer_stream = open_memstream(&buffer, &size);
    //setbuf(buffer_stream, NULL);

    t_internal_type(x, buffer_stream);

    // TODO: PLACEHOLDER CODE
    if (identifier != NULL)
        fprintf(buffer_stream, "int *%s", identifier);
    else
        fprintf(buffer_stream, "int");
    // TODO: END OF PLACEHOLDER CODE

    fclose(buffer_stream);

    return buffer;
}

void t_block(struct Block *b) {
    // TODO: PLACEHOLDER CODE
    p("{ ... }");
}
void t_expr(struct Expr *x);

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
            printf("Compiler error: function initializer without explicit type\n");
            exit(1);
        }

        if (t->tag == Qualifier) {
            t = t->qualifier.t;
        }
        if (t->tag != FunPointer) {
            printf("Compiler error: explicit type of function initializer must be a function pointer\n");
            exit(1);
        }

        // As explanied above, we create a new buffer to print to,
        // print the code to it, then restore the current buffer.
        struct BufferList *buffer_saved = current_buffer;
        struct BufferList *tmp = buffer_list;

        buffer_list = create_buffer();
        buffer_list->next = tmp;
        current_buffer = buffer_list;

        /*generate a unique identifier*/
        char *unique_temporary_identifier;
        asprintf(&unique_temporary_identifier, "_prec_anon_%d", global_identifier_counter);
        global_identifier_counter += 1;

        char *decl = t_str_type(t, unique_temporary_identifier, true /*dereference function pointer*/);
        p("%s", decl);
        // print the code itself
        t_block(x->code);

        current_buffer = buffer_saved;
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
        break;
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
        break;
    case FunctionCall:
        t_expr(x->function_call.callee);
        p("(");
        struct ArgumentExpressionList *curr = x->function_call.args;
        if (curr == NULL)
            break;

        REWIND_LIST(curr);

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
        p("("); t_expr(x->ternary.cond); p(")");
        p("?");
        p("("); t_expr(x->ternary.if_true); p(")");
        p(":");
        p("("); t_expr(x->ternary.if_false); p(")");
        break;
    case Cast:
        p("("); p("%s", t_str_type(x->cast.type, NULL, false)); p(")");
        p("("); t_expr(x->cast.e); p(")");
        break;
    case CompoundLiteral:
        p("("); p("%s", t_str_type(x->compound_literal.type, NULL, false)); p(")");
        t_initializer(x->compound_literal.init, x->compound_literal.type);
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

void t_declaration(struct Declaration *decl) {
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
        p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, false));
        if (node->decl->val != NULL) {
            p(" = ");
            t_initializer(node->decl->val, decl->type);
        }
        p(";\n");
        node = node->next;
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
            buffer_list = create_buffer();
            current_buffer = buffer_list;
            t_declaration(top->decl);
            print_buffer_list(buffer_list);
            destroy_buffer_list(buffer_list);
            buffer_list = NULL;
            current_buffer = NULL;
            break;
        }
        top = top->next;
    }
}
