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

bool isBaseType (struct Type *x) {
    return x->tag == TypeofExpr
        || x->tag == TypeofType
        || x->tag == Struct
        || x->tag == Union
        || x->tag == Enum
        || x->tag == CType
        || x->tag == f64
        || x->tag == f32
        || x->tag == i64
        || x->tag == u64
        || x->tag == i32
        || x->tag == u32
        || x->tag == i16
        || x->tag == u16
        || x->tag == i8
        || x->tag == u8
        || x->tag == Void
        || x->tag == Bool
        ;
}

char *t_str_type(struct Type *x, char *identifier, bool fun_pointer_dereferenced);

void t_internal_type(struct Type *x, FILE *stream) {
    if (!isBaseType(x)) {
        switch (x->tag) {
        case Qualifier:
            t_internal_type(x->qualifier.t, stream);
            break;
        case Reference:
            t_internal_type(x->reference, stream);
            break;
        case Array:
            t_internal_type(x->array.t, stream);
            break;
        case FunPointer:
            t_internal_type(x->fun_pointer.return_type, stream);
            break;
        }
    } else {
        switch (x->tag) {
        case CType:
            fprintf(stream, "@%s", x->c_type);
            break;
        case f64: fprintf(stream, "double"); break;
        case f32: fprintf(stream, "float"); break;
        case u64: fprintf(stream, "long unsigned"); break;
        case i64: fprintf(stream, "long signed"); break;
        case u32: fprintf(stream, "unsigned"); break;
        case i32: fprintf(stream, "signed"); break;
        case u16: fprintf(stream, "short unsigned"); break;
        case i16: fprintf(stream, "short signed"); break;
        case u8: fprintf(stream, "unsigned char"); break;
        case i8: fprintf(stream, "signed char"); break;
        case Void: fprintf(stream, "void"); break;
        case Bool: fprintf(stream, "_Bool"); break;
        case TypeofExpr: {
            fprintf(stream, "typeof(");

            struct BufferList *saved_buffer = current_buffer;

            current_buffer = &(struct BufferList){ .stream = stream };

            t_expr(x->typeof_expr);

            current_buffer = saved_buffer;

            fprintf(stream, ")");
            break;
        }
        case TypeofType: {
            fprintf(stream, "typeof<%s>", t_str_type(x->typeof_type, NULL, false));
            break;
        }
        case Struct:                
            break;
        case Union:                
            break;
        case Enum:
            fprintf(stream, "enum ");
            if (x->enum_def.name != NULL) {
                fprintf(stream, "%s ", x->enum_def.name);
            }
            if (x->enum_def.values != NULL) {
                fprintf(stream, "{\n");
                global_indent_level += 1;

                struct EnumeratorList *node = x->enum_def.values;
                REWIND_LIST(node);
                while (node != NULL) {
                    tabs_custom(stream);
                    fprintf(stream, "%s", node->val->name);
                    if (node->val->val != NULL) {
                        fprintf(stream, "=");

                        struct BufferList *saved_buffer = current_buffer;

                        current_buffer = &(struct BufferList){ .stream = stream };

                        t_expr(node->val->val->expr);

                        current_buffer = saved_buffer;
                    }
                    if (node->next != NULL)
                        fprintf(stream, ",");
                    node = node->next;

                    fprintf(stream, "\n");
                }

                global_indent_level -= 1;

                tabs_custom(stream);
                fprintf(stream, "}");
            }
            break;
        }
        fprintf(stream, " ");
    }
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

    /*// TODO: PLACEHOLDER CODE
    if (identifier != NULL)
        fprintf(buffer_stream, "int *%s", identifier);
    else
        fprintf(buffer_stream, "int");
    // TODO: END OF PLACEHOLDER CODE*/

    fclose(buffer_stream);

    return buffer;
}

void t_declaration(struct Declaration *decl);
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
            t_declaration(node->item->decl);
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
            printf("Compiler error: function initializer without explicit type\n");
            exit(1);
        }

        if (t->tag == Qualifier) {
            t = t->qualifier.t;
        }
        if (t->tag != FunPointer) {
            printf("Compiler error: explicit type of function initializer must be a function pointer\n");
            //exit(1);
        }

        // As explanied above, we create a new buffer to print to,
        // print the code to it, then restore the current buffer.
        int saved_indent = global_indent_level;
        global_indent_level = 0;

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
            t_expr(x->binOp.e1);
            p("*");
            t_expr(x->binOp.e2);
            break;
        case Div:
            t_expr(x->binOp.e1);
            p("/");
            t_expr(x->binOp.e2);
            break;
        case Mod:
            t_expr(x->binOp.e1);
            p("%c", '%');
            t_expr(x->binOp.e2);
            break;
        case Add:
            t_expr(x->binOp.e1);
            p("+");
            t_expr(x->binOp.e2);
            break;
        case Sub:
            t_expr(x->binOp.e1);
            p("-");
            t_expr(x->binOp.e2);
            break;
        case And:
            t_expr(x->binOp.e1);
            p("&");
            t_expr(x->binOp.e2);
            break;
        case BoolAnd:
            t_expr(x->binOp.e1);
            p("&&");
            t_expr(x->binOp.e2);
            break;
        case Or:
            t_expr(x->binOp.e1);
            p("|");
            t_expr(x->binOp.e2);
            break;
        case BoolOr:
            t_expr(x->binOp.e1);
            p("||");
            t_expr(x->binOp.e2);
            break;
        case Xor:
            t_expr(x->binOp.e1);
            p("^");
            t_expr(x->binOp.e2);
            break;
        case LeftShift:
            t_expr(x->binOp.e1);
            p("<<");
            t_expr(x->binOp.e2);
            break;
        case RightShift:
            t_expr(x->binOp.e1);
            p(">>");
            t_expr(x->binOp.e2);
            break;
        case Less:
            t_expr(x->binOp.e1);
            p("<");
            t_expr(x->binOp.e2);
            break;
        case More:
            t_expr(x->binOp.e1);
            p(">");
            t_expr(x->binOp.e2);
            break;
        case Equal:
            t_expr(x->binOp.e1);
            p("==");
            t_expr(x->binOp.e2);
            break;
        case MoreEqual:
            t_expr(x->binOp.e1);
            p(">=");
            t_expr(x->binOp.e2);
            break;
        case LessEqual:
            t_expr(x->binOp.e1);
            p("<=");
            t_expr(x->binOp.e2);
            break;
        case NotEqual:
            t_expr(x->binOp.e1);
            p("!=");
            t_expr(x->binOp.e2);
            break;
        case Assign:
            t_expr(x->binOp.e1);
            p("=");
            t_expr(x->binOp.e2);
            break;
        case Sequence:
            t_expr(x->binOp.e1);
            p(",");
            t_expr(x->binOp.e2);
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
        tabs();
        p("%s%s", storage_class, t_str_type(decl->type, node->decl->name, false));
        if (node->decl->val != NULL) {
            p(" = ");
            t_initializer(node->decl->val, decl->type);
        }
        p(";\n");
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
