#include <stdint.h>
#define C(_c1, _c2) (int16_t)(_c1 << 8 | + _c2)

enum BinOp {
    Mul='*',
    Div='/',
    Mod='%',
    Add='+',
    Sub='-',
    And='a',
    Or='o',
    Xor='x',
    Less='<',
    More='>',
    Equal=C('=','='),
    MoreEqual=C('>','='),
    LessEqual=C('>','='),
    NotEqual=C('!','='),
    StructDeref=C('-', '>'),
    StructAccess='.',
    Assign='=',
    Sequence=',',
};

enum UnOp {
    Index='i',
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
    Unary='u',
    Binary='b',
    FunctionCall='f',
    Identifier='i',
    String='s',
    Ternary='c',
    Constant='c',
    Cast='<',
    Default='d',
    CompoundLiteral='l',
};

struct Type;
struct Initializer;

struct Expr {
    enum ExprType tag;
    union {
        struct {
            enum BinOp op;
            struct Expr *e1;
            struct Expr *e2;
        } binOp;

        struct {
            enum UnOp op;
            struct Expr *e;
        } unOp;

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

        struct Initializer *compound_literal;

        struct Type *default_initializer;

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

struct DesignatorList {
    union {
        char *access;
        struct ConstExpr *index;
    };
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
    Struct='s',
    Union='u',
    Enum='e',
    CType=C('c','t'),
    i64=C('i',64),
    u64=C('u',64),
    i32=C('i',32),
    u32=C('u',32),
    i16=C('i',16),
    u16=C('u',16),
    i8=C('i',8),
    u8=C('u',8),
};

struct Block;
struct ConstDeclarationList;
struct EnumeratorList;
struct TypeParamList;

struct Type {
    enum TypeSort tag;
    union {
        struct {
            enum TypeOp op;
            struct Type *t;
        } compound;

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

struct TypeParamList {
    // if all are NULL: variadic ender u32(&)(x, y, ...)
    struct Type *val_type;
    char *val_name;
    struct TypeParamsList *next;
};

struct EnumeratorList {
    struct EnumValue *val;
    struct EnumeratorList *next;
};

struct EnumValue {
    char *name;
    _Bool has_val;
    int val;
};


struct ConstExpr {
    struct Expr *x;
};

struct ConstVarList {
    char *var_name;
    struct ConstantExpression *var_val;
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
    Static='s',
    Extern='e',
};

struct VarList {
    char *var_name;
    struct Initializer *var_val;
    struct VarList *next;
};

struct Declaration {
    enum StorageClass *class;
    struct Type *type;
    struct VarList *vars;
};

struct Block {
    struct BlockList *contents;
};

struct BlockList {
    enum { Statement='s', Declaration='d' } tag;
    union {
        struct Statement *stat;
        struct Declaration *decl;
    };
    struct BlockList *next;
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
    enum { While='w', DoWhile='d', For='f' } tag;
    union {
        struct {
            struct Expr *expr;
            struct Statement *stat;
        } while_dowhile_stat;
        struct {
            struct Expr *initialization;
            struct Expr *loop_action;
            struct Expr *loop_condition;
        } for_stat;
    };
};

// Top level node
struct TopLevel {
    enum {CInclude='c', Decl='d'} tag;
    union {
        struct Declaration *decl;
        char *c_include;
    };
    struct DeclarationList *next;
};
