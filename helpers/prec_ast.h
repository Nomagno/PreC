#ifndef _PREC_AST_H
#define _PREC_AST_H

#include <stdint.h>
#define C(_c1, _c2) (int16_t)(_c1 << 8 | + _c2)

    #define DUP(...) ({typeof(__VA_ARGS__) *tmp;\
                      tmp = malloc(sizeof(__VA_ARGS__));\
                      *tmp = __VA_ARGS__;\
                      tmp; })
    #define DUP_T(_type, _tag, ...) DUP((struct _type){.tag = _tag, __VA_ARGS__})

    #define BIN_EXPR(_tag, _arg1, _arg2) DUP_T(Expr, Binary, .binOp = { .tag = _tag, .e1 = _arg1, .e2 = _arg2 });
    #define UN_EXPR(_tag, _arg) DUP_T(Expr, Unary, .unOp = { .tag = _tag, .e = _arg});

enum BinOp {
    Mul='*',
    Div='/',
    Mod='%',
    Add='+',
    Sub='-',
    And='a',
    BoolAnd='A',
    Or='o',
    BoolOr='O',
    Xor='x',
    LeftShift=C('<','<'),
    RightShift=C('>','>'),
    Less='<',
    More='>',
    Equal=C('=','='),
    MoreEqual=C('>','='),
    LessEqual=C('<','='),
    NotEqual=C('!','='),
    Assign='=',
    Sequence=',',
    Index='i',
};

enum UnOp {
    Sizeof='s',
    Ref='&',
    Deref='^',
    Neg='~',
    Not='n',
    BoolNot='!',
    PreIncrement=C('p', '+'),
    PreDecrement=C('p', '-'),
    PostIncrement=C('+', '+'),
    PostDecrement=C('-', '-'),
};

enum ExprType {
    SizeofType='t',
    Unary='u',
    Binary='b',
    FunctionCall='F',
    Identifier='i',
    String='s',
    Ternary='c',
    Float='f',
    Int='d',
    Constant='c',
    Cast='<',
    CompoundLiteral='l',
    StructDeref=C('-', '>'),
    StructAccess='.',
};

struct Type;
struct Initializer;

struct Expr {
    enum ExprType tag;
    union {
        struct {
            struct Expr *e;
            char *member;
        } struct_access_deref;

        struct {
            enum BinOp tag;
            struct Expr *e1;
            struct Expr *e2;
        } binOp;

        struct {
            enum UnOp tag;
            struct Expr *e;
        } unOp;

        struct Type *sizeof_type;

        struct {
            struct Type *type;
            struct Expr *e;
        } cast;

        struct {
            struct Expr *cond;
            struct Expr *if_true;
            struct Expr *if_false;
        } ternary;

        struct {
            struct Expr *callee;
            struct ArgumentExpressionList *args;
        } function_call;

        struct {
            struct Type *type;
            struct Initializer *init;
        } compound_literal;

        char *string;
        char *identifier;
        double fp_num;
        intmax_t int_num;
    };
};
struct ConstExpr;

struct ArgumentExpressionList {
    struct Expr *expr;
    struct ArgumentExpressionList *next;
};

struct Designator {
    enum {Access='a', DesignatorIndex='i'} tag;
    union {
        char *access;
        struct ConstExpr *index;
    };
};

struct DesignatorList {
    struct Designator *desig;
    struct DesignatorList *next;
};

struct InitializerList {
    struct DesignatorList *designation;
    struct Initializer *current;
    struct InitializerList *next;
};

struct Initializer {
    enum InitializerSort { Expr='e', Data='d', Code='c' } tag;
    union {
        struct Expr *expr;
        struct InitializerList *data;
        struct Block *code;
    };
};

enum TypeOp {
    Mut='m',
    Restrict='r',
    Volatile='v',
    Reference='&',
};

enum TypeSort {
    Compound='c',
    FunPointer='f',
    TypeofExpr='t',
    TypeofType='T',
    Struct='s',
    Union='u',
    Enum='e',
    Array='a',
    CType=C('c','t'),
    f64=C('f',64),
    f32=C('f',32),
    i64=C('i',64),
    u64=C('u',64),
    i32=C('i',32),
    u32=C('u',32),
    i16=C('i',16),
    u16=C('u',16),
    i8=C('i',8),
    u8=C('u',8),
    Void='v',
    Bool='b',
};

struct Block;
struct ConstDeclarationList;
struct EnumeratorList;
struct TypeParamList;

struct Type {
    enum TypeSort tag;
    union {
        struct Expr *typeof_expr;

        struct Type *typeof_type;

        struct {
            enum TypeOp tag;
            struct Type *t;
        } compound;

        struct {
            struct ConstExpr *size;
            struct Type *t;
        } array;

        struct {
            char *name;
            struct ConstDeclarationList *declarations;
        } struct_or_union_def;

        struct {
            char *name;
            struct EnumeratorList *values;
        } enum_def;

        char *c_type;

        struct {
            struct Type *return_type;
            struct TypeParamList *param_list;
        } fun_pointer;
    };
};

struct TypeParam {
    struct Type *type;
    char *name;
};
struct TypeParamList {
    struct TypeParam *param; // NULL: ellipsis
    struct TypeParamList *next;
};

struct EnumeratorList {
    struct EnumValue *val;
    struct EnumeratorList *next;
};

struct EnumValue {
    char *name;
    struct ConstExpr *val;
};


struct ConstExpr {
    struct Expr *expr;
};

struct ConstVarDecl {
    char *name;
};

struct ConstVarList {
    struct ConstVarDecl *decl;
    struct ConstVarList *next;
};

struct ConstDeclaration {
    struct Type *type;
    struct ConstVarList *vars;
};

struct ConstDeclarationList {
    struct ConstDeclaration *decl;
    struct ConstDeclarationList *next;
};


enum StorageClass {
    None='n',
    Static='s',
    Extern='e',
};

struct VarDecl {
    char *name;
    struct Initializer *val;
};

struct VarList {
    struct VarDecl *decl;
    struct VarList *next;
};

struct Declaration {
    enum StorageClass class;
    struct Type *type;
    struct VarList *vars;
};

struct Block {
    struct BlockList *contents;
};

struct BlockList {
    struct BlockItem *item;
    struct BlockList *next;
};

struct BlockItem {
    enum { Statement='s', Declaration='d' } tag;
    union {
        struct Statement *stat;
        struct Declaration *decl;
    };
};

enum StatementType {
    Labeled='l',
    Block='b',
    Expression='e',
    Selection='s',
    Iteration='i',
    Jump='j',
};

struct Statement {
    enum StatementType tag;
    union {
        struct LabeledStatement *l;
        struct Block *b;
        struct Expr *e; // NULL for ';'
        struct SelectionStatement *s;
        struct IterationStatement *i;
        struct JumpStatement *j;
    };
};

struct SelectionStatement {
    enum { If='i', IfElse='e', Switch='s' } tag;
    union {
        struct {
            struct Expr *clause;
            struct Statement *action;
        } simple_if;
        struct {
            struct Expr *clause;
            struct Statement *action_true;
            struct Statement *action_false;
        } if_else;
        struct {
            struct Expr *clause;
            struct Statement *block;
        } switch_stat;
    };
};

struct JumpStatement {
    enum { Return='r', Continue='c', Goto='g', Break='b'} tag;
    union {
        struct {
            struct Expr* expr;
        } return_stat;
        struct {
            char *label_name;
        } goto_stat;
    };
};

struct LabeledStatement {
    enum { Case='c', Default_Label='d', Label='l' } tag;
    char *label_name; // For the Label case
    struct ConstExpr *case_expr; // For the Case case
    struct Statement *stat; // For all cases
};

struct IterationStatement {
    enum { While='w', DoWhile='d', For_Decl='f', For_Expr='F' } tag;
    union {
        struct {
            struct Expr *expr;
            struct Statement *stat;
        } while_dowhile_stat;
        struct {
            struct Declaration *init;
            struct Expr *clause;
            struct Expr *update;
        } for_stat_decl;
        struct {
            struct Expr *init;
            struct Expr *clause;
            struct Expr *update;
        } for_stat_expr;
    };
};

// Top level node
struct TopLevel {
    enum {CInclude='c', Decl='d'} tag;
    union {
        struct Declaration *decl;
        char *c_include;
    };
    struct TopLevel *next;
};

#endif
