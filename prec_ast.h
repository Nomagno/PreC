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
    Identifier='i',
    String='s',
    Constant='c',
    Cast='<',
    Default='d',
    CompoundLiteral='l',
};

struct Block;
struct StructOrUnionDef;
struct EnumDef;
struct TypeParamList;

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

        struct Initializer *compound_literal;

        struct Type *default_initializer;

        char *string;
        char *identifier;
        float fp_num;
        int int_num;
    };
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


struct Type {
    enum TypeSort tag;
    union {
        struct {
            enum TypeOp op;
            struct Type *t;
        } compound;

        struct {
            enum TypeOp op;
            char *name;
            struct StructOrUnionDef *def;
        } struct_or_union_def;

        struct {
            enum TypeOp op;
            char *name;
            struct EnumDef *def;
        } enum_def;

        char *c_type;

        struct {
            struct Type *return_type;
            struct TypeParamList *param_list;
        } fun_pointer;
    };
};
