#include <stdio.h>
#include <assert.h>
#include "prec_ast.h"
#include "prec_transpiler.h"

char preceding_function_buffer[1 << 16];

void prelude(char *);

void t_type(struct Type *x) {
    
}

void t_initializer(struct Initializer *x) {
    
}

#define p printf

void t_expr(struct Expr *x) {
    switch (x->tag) {
    case SizeofType:
        p("sizeof(");
        t_type(x->sizeof_type);
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
        p("("); t_type(x->cast.type); p(")");
        p("("); t_expr(x->cast.e); p(")");
        break;
    case CompoundLiteral:
        p("("); t_type(x->compound_literal.type); p(")");
        t_initializer(x->compound_literal.init);
        break;
    case StructAccess:
        p("("); t_expr(x->struct_access_deref.e); p(")");
        p(".");
        p("%s", x->struct_access_deref.member);
        break;
    case StructDeref:
        p("("); t_type(x->cast.type); p(")");
        p("->");
        p("("); t_expr(x->cast.e); p(")");
        break;
    }
}

void transpile(struct TopLevel *top) {
    p("[PLACEHOLDER] Address of top level AST node: %p\n", top);
}
