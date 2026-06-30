%expect 1

%{
    #include "stdlib.h"
    #include "prec_ast.h"
    int yylex(void);
    int yyerror(const char *s);

    #define DUP(...) ({typeof(__VA_ARGS__) *tmp;\
                      tmp = malloc(sizeof(__VA_ARGS__));\
                      *tmp = __VA_ARGS__;\
                      tmp; })
    #define DUP_T(_type, _tag, ...) DUP((struct _type){.tag = _tag, __VA_ARGS__})

    #define BIN_EXPR(_tag, _arg1, _arg2) DUP_T(Expr, Binary, .binOp = { .tag = _tag, .e1 = _arg1, .e2 = _arg2 });
    #define UN_EXPR(_tag, _arg) DUP_T(Expr, Unary, .unOp = { .tag = _tag, .e = _arg});
%}

%union {
    struct TopLevel *top_level;
    struct Declaration *declaration;
    struct VarList *var_list;
    struct VarDecl *var_decl;

    struct ConstVarDecl *const_var_decl;
    struct ConstVarList *const_var_list;
    struct ConstDeclaration *const_declaration;
    struct ConstDeclarationList *const_declaration_list;

    struct Expr *expression;
    struct ConstExpr *const_expression;

    struct Statement *stat;
    struct SelectionStatement *selection_stat;
    struct LabeledStatement *labeled_stat;
    struct IterationStatement *iteration_stat;
    struct JumpStatement *jump_stat;

    struct Block *block;
    struct BlockList *block_list;
    struct BlockItem *block_item;

    struct EnumValue *enum_val;
    struct EnumeratorList *enum_list;

    struct ArgumentExpressionList *arg_list;
    struct TypeParamList *type_param_list;
    struct TypeParam *type_param;

    struct Type *type;

    struct DesignatorList *designator_list;
    struct InitializerList *initializer_list;
    struct Initializer *initializer;

    enum StorageClass storage_class;
    enum TypeOp type_operation;

    char *identifier;
    char *string_literal;
    double float_constant;
    intmax_t int_constant;
}

%type <expression> primary_expression postfix_expression unary_expression cast_expression arithmetic_expression conditional_expression assignment_expression expression
%type <const_expression> constant_expression

%type <const_var_decl> const_id_decl
%type <const_var_list> const_var_list
%type <const_declaration> const_declaration
%type <const_declaration_list> struct_declaration_list

%type <var_decl> id_decl
%type <var_list> var_list
%type <declaration> declaration

%type <initializer> initializer compound_literal_initializer
%type <initializer_list> initializer_list
%type <designator_list> designation designator_list
%type <designator> designator

%type <stat> statement
%type <expression> expression_statement
%type <jump_stat> jump_statement
%type <iteration_stat> iteration_statement
%type <labeled_stat> labeled_statement
%type <selection_stat> selection_statement

%type <arg_list> argument_expression_list

%type <block> compound_statement
%type <block_list> block_item_list
%type <block_list> block_item

%type <storage_class> storage_class
%type <type> type regular_type concrete_type base_type
%type <type> enum_specifier
%type <type> struct_or_union_specifier
%type <type_operation> type_qualifier
%type <type_param_list> parameter_type_list
%type <type_param> parameter_declaration
%type <enum_list> enumerator_list
%type <enum_val> enumerator



%token <float_constant> FLOAT_CONSTANT
%token <int_constant> INT_CONSTANT
%token <identifier> IDENTIFIER
%token <string_literal> STRING_LITERAL
%token SIZEOF
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP

%token EXTERN STATIC RESTRICT MUT VOLATILE
%token BOOL U8 I8 U16 I16 U32 I32 U64 I64 F32 F64 VOID
%token STRUCT UNION ENUM
%token ELLIPSIS
%token C_INCLUDE

%left LOWEST_LEFT_PRECEDENCE
%left OR_OP
%left AND_OP
%left OR
%left XOR
%left AND
%left EQ_OP NE_OP
%left LE_OP '<' '>' GE_OP
%left LEFT_OP RIGHT_OP
%left '+' '-'
%left '*' '/' '%'
%right '&' '^' '!' '~' NOT

%token CASE DEFAULT IF ELSE SWITCH WHILE DO FOR GOTO CONTINUE BREAK RETURN

%start translation_unit

%%

primary_expression
	: IDENTIFIER
	    { $$ = DUP_T(Expr, Identifier, .identifier = $1); }
	| INT_CONSTANT
	    { $$ = DUP_T(Expr, Int, .int_num = $1); }
	| FLOAT_CONSTANT
	    { $$ = DUP_T(Expr, Float, .fp_num = $1); }
	| STRING_LITERAL
	    { $$ = DUP_T(Expr, String, .string = $1); }
	| '(' expression ')'
	    { $$ = $2; }
	;

postfix_expression
	: primary_expression
	    { $$ = $1; }
	| postfix_expression '[' expression ']'
	    { $$ = BIN_EXPR(Index, $1, $3) }
	| postfix_expression '(' ')'
	    { $$ = DUP_T(Expr, FunctionCall, .function_call = { .callee = $1, .args = NULL}); }
	| postfix_expression '(' argument_expression_list ')'
	    { $$ = DUP_T(Expr, FunctionCall, .function_call = { .callee = $1, .args = $3}); }
	| postfix_expression '(' argument_expression_list ',' ')'
	    { $$ = DUP_T(Expr, FunctionCall, .function_call = { .callee = $1, .args = $3}); }
	| postfix_expression '.' IDENTIFIER
	    { $$ = DUP_T(Expr, StructAccess, .struct_access_deref = { .e = $1, .member = $3}); }
	| postfix_expression PTR_OP IDENTIFIER
	    { $$ = DUP_T(Expr, StructDeref, .struct_access_deref = { .e = $1, .member = $3}); }
	| postfix_expression INC_OP
	    { $$ = UN_EXPR(PostIncrement, $1); }
	| postfix_expression DEC_OP
	    { $$ = UN_EXPR(PostDecrement, $1); }
	| '<' type '>' compound_literal_initializer
	    { $$ = DUP_T(Expr, CompoundLiteral, .compound_literal = { .type = $2, .init = $4 } ); }
	| '<' type '>' DEFAULT
	    { $$ = DUP_T(Expr, Default, .default_initializer = $2); }
	;

argument_expression_list
	: assignment_expression
	| argument_expression_list ',' assignment_expression
	;

unary_expression
	: postfix_expression
	    { $$ = $1; }
	| INC_OP unary_expression
	    { $$ = UN_EXPR(PreIncrement, $2); }
	| DEC_OP unary_expression
	    { $$ = UN_EXPR(PreDecrement, $2); }
	| NOT cast_expression
	    { $$ = UN_EXPR(Not, $2); }
	| '&' cast_expression
	    { $$ = UN_EXPR(Ref, $2); }
	| '^' cast_expression
	    { $$ = UN_EXPR(Deref, $2); }
	| '~' cast_expression
	    { $$ = UN_EXPR(Neg, $2); }
	| '!' cast_expression
	    { $$ = UN_EXPR(BoolNot, $2); }
	| SIZEOF unary_expression
	    { $$ = UN_EXPR(Sizeof, $2); }
	| SIZEOF '<' type '>'
	    { $$ = DUP_T(Expr, SizeofType, .sizeof_type = $3); }
	;

cast_expression
	: unary_expression
	    { $$ = $1; }
	| '<' type '>' cast_expression
	    { $$ = DUP_T(Expr, Cast, .cast = { .type = $2, .e = $4 }); }
	;

arithmetic_expression
    : cast_expression
	    { $$ = $1; }
    | arithmetic_expression '*' arithmetic_expression
	    { $$ = BIN_EXPR(Mul, $1, $3) }
    | arithmetic_expression '/' arithmetic_expression
	    { $$ = BIN_EXPR(Div, $1, $3) }
    | arithmetic_expression '%' arithmetic_expression
	    { $$ = BIN_EXPR(Mod, $1, $3) }
    | arithmetic_expression '+' arithmetic_expression
	    { $$ = BIN_EXPR(Add, $1, $3) }
    | arithmetic_expression '-' arithmetic_expression
	    { $$ = BIN_EXPR(Sub, $1, $3) }
    | arithmetic_expression LEFT_OP arithmetic_expression
	    { $$ = BIN_EXPR(LeftShift, $1, $3) }
    | arithmetic_expression RIGHT_OP arithmetic_expression
	    { $$ = BIN_EXPR(RightShift, $1, $3) }
    | arithmetic_expression '<' arithmetic_expression
	    { $$ = BIN_EXPR(Less, $1, $3) }
    | arithmetic_expression '>' arithmetic_expression
	    { $$ = BIN_EXPR(More, $1, $3) }
    | arithmetic_expression LE_OP arithmetic_expression
	    { $$ = BIN_EXPR(LessEqual, $1, $3) }
    | arithmetic_expression GE_OP arithmetic_expression
	    { $$ = BIN_EXPR(MoreEqual, $1, $3) }
    | arithmetic_expression EQ_OP arithmetic_expression
	    { $$ = BIN_EXPR(Equal, $1, $3) }
    | arithmetic_expression NE_OP arithmetic_expression
	    { $$ = BIN_EXPR(NotEqual, $1, $3) }
    | arithmetic_expression AND arithmetic_expression
	    { $$ = BIN_EXPR(And, $1, $3) }
    | arithmetic_expression XOR arithmetic_expression
	    { $$ = BIN_EXPR(Xor, $1, $3) }
    | arithmetic_expression OR arithmetic_expression
	    { $$ = BIN_EXPR(Or, $1, $3) }
    | arithmetic_expression AND_OP arithmetic_expression
	    { $$ = BIN_EXPR(BoolAnd, $1, $3) }
    | arithmetic_expression OR_OP arithmetic_expression
	    { $$ = BIN_EXPR(BoolOr, $1, $3) }
    ;

conditional_expression
	: arithmetic_expression
	    { $$ = $1; }
	| arithmetic_expression '?' expression ':' arithmetic_expression
	    { $$ = DUP_T(Expr, Ternary, .ternary = { .cond = $1, .if_true = $3, .if_false = $5 }); }
	;

/*Missing: check it's actually constant lol*/
constant_expression
    : conditional_expression
	    { $$ = DUP((struct ConstExpr){ .expr = $1 }); }
    ;

assignment_expression
	: conditional_expression
	    { $$ = $1; }
	| unary_expression '=' assignment_expression
	    { $$ = BIN_EXPR(Assign, $1, $3) }
	;

expression
	: assignment_expression
	    { $$ = $1; }
	| expression ',' assignment_expression
	    { $$ = BIN_EXPR(Sequence, $1, $3) }
	;


const_declaration
    : type ';'
    | type const_var_list ';'
    | type const_var_list ',' ';'
    ;

const_var_list
    : const_id_decl
    | const_var_list ',' const_id_decl
    ;

const_id_decl
    : IDENTIFIER
    | IDENTIFIER '=' constant_expression;


declaration
    : storage_class declaration
    | type ';'
    | type var_list ';'
    | type var_list ',' ';'
    ;

var_list
    : id_decl
    | var_list ',' id_decl
    ;

id_decl
    : IDENTIFIER
    | IDENTIFIER '=' initializer;

initializer
    : assignment_expression
    | '{' initializer_list '}'
    | '{' initializer_list ',' '}'
    | '$' compound_statement
    ;

compound_literal_initializer
    : '{' initializer_list '}'
    | '{' initializer_list ',' '}'
    | '$' compound_statement
    ;

initializer_list
	: initializer
	| designation initializer
	| initializer_list ',' initializer
	;

designation
	: designator_list '='
	;

designator_list
	: designator
	| designator_list designator
	;

designator
	: '[' constant_expression ']'
	| '.' IDENTIFIER
	;

storage_class
    : EXTERN
    | STATIC
    ;

type
    : regular_type
    | type_qualifier type
    ;

regular_type
    : '(' regular_type ')'
    | concrete_type
    | regular_type type_qualifier
    ;

concrete_type
    : base_type
    | '@' IDENTIFIER /*Itentifier types are only for use with #c_include*/
    | regular_type '&'
    | regular_type '[' ']'
    | regular_type '[' constant_expression ']'
    | regular_type '(' '&' ')' '(' parameter_type_list ')'
    ;


type_qualifier
	: MUT
	| RESTRICT
	| VOLATILE
	;

base_type
	: VOID
	| BOOL
	| U8 | I8 | U16 | I16 | U32 | I32 | U64 | I64 | F32 | F64
	| struct_or_union_specifier
	| enum_specifier
	;

parameter_type_list
	:
	| parameter_list
	| parameter_list ','
	| parameter_list ',' ELLIPSIS
	| parameter_list ',' ELLIPSIS ','
	;

parameter_list
	: parameter_declaration
	| parameter_list ',' parameter_declaration
	;

parameter_declaration
	: type
	| type IDENTIFIER
	;


struct_or_union_specifier
	: STRUCT IDENTIFIER '{' struct_declaration_list '}'
	| STRUCT '{' struct_declaration_list '}'
	| STRUCT IDENTIFIER
	| UNION IDENTIFIER '{' struct_declaration_list '}'
	| UNION '{' struct_declaration_list '}'
	| UNION IDENTIFIER
	;

struct_declaration_list
	: const_declaration
	| struct_declaration_list const_declaration
	;

enum_specifier
	: ENUM '{' enumerator_list '}'
	| ENUM '{' enumerator_list ',' '}'
	| ENUM IDENTIFIER
	| ENUM IDENTIFIER '{' enumerator_list '}'
	| ENUM IDENTIFIER '{' enumerator_list ',' '}'
	;

enumerator_list
	: enumerator
	| enumerator_list ',' enumerator
	;

enumerator
	: IDENTIFIER
	| IDENTIFIER '=' constant_expression
	;

statement
	: labeled_statement
	| compound_statement
	| expression_statement
	| selection_statement
	| iteration_statement
	| jump_statement
	;

labeled_statement
	: IDENTIFIER ':' statement
	| CASE constant_expression ':' statement
	| DEFAULT ':' statement
	;

compound_statement
	: '{' '}'
	| '{' block_item_list '}'
	;

block_item_list
	: block_item
	| block_item_list block_item
	;

block_item
	: declaration
	| statement
	;

expression_statement
	: ';'
	| expression ';'
	;

selection_statement
	: IF '(' expression ')' statement
	| IF '(' expression ')' statement ELSE statement
	| SWITCH '(' expression ')' statement
	;

iteration_statement
	: WHILE '(' expression ')' statement
	| DO statement WHILE '(' expression ')' ';'
	| FOR '(' expression_statement expression_statement ')' statement
	| FOR '(' expression_statement expression_statement expression ')' statement
	| FOR '(' declaration expression_statement ')' statement
	| FOR '(' declaration expression_statement expression ')' statement
	;

jump_statement
	: GOTO IDENTIFIER ';'
	| CONTINUE ';'
	| BREAK ';'
	| RETURN ';'
	| RETURN expression ';'
	;

translation_unit
	: declaration
	| C_INCLUDE STRING_LITERAL
	| translation_unit declaration
	;


%%
#include <stdio.h>

extern char yytext[];
extern int column;

int yyerror(const char *s)
{
	fflush(stdout);
	printf("\n%*s\n%*s\n", column, "^", column, s);
	return 1;
}

int main(void) {
    yyparse();
}
