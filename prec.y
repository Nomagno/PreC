%expect 1

%{
    #include <stdlib.h>

    #include "helpers/prec_ast.h"
    #include "helpers/prec_transpiler.h"

    int yyerror(const char *s);
    int yylex(void);
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
    struct Designator *designator;
    struct InitializerList *initializer_list;
    struct Initializer *initializer;

    enum StorageClass storage_class;
    enum Qualifier qualifier;

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
%type <block_item> block_item

%type <storage_class> storage_class
%type <type> type regular_type concrete_type base_type
%type <type> enum_specifier
%type <type> struct_or_union_specifier
%type <qualifier> type_qualifier
%type <type_param_list> parameter_type_list parameter_list
%type <type_param> parameter_declaration
%type <enum_list> enumerator_list
%type <enum_val> enumerator

%type <top_level> top_level translation_unit;


%token <float_constant> FLOAT_CONSTANT
%token <int_constant> INT_CONSTANT
%token <identifier> IDENTIFIER
%token <string_literal> STRING_LITERAL
%token SIZEOF TYPEOF
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

%start top_level

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
	;

argument_expression_list
	: assignment_expression
	    { $$ = DUP((struct ArgumentExpressionList){ .expr = $1, .next = NULL }); }
	| argument_expression_list ',' assignment_expression
	    { $1->next = DUP((struct ArgumentExpressionList){ .expr = $3, .prev = $1, .next = NULL }); $$ = $1->next; }
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
        { $$ = DUP((struct ConstDeclaration){ .type = $1, .vars = NULL}); }
    | type const_var_list ';'
        { $$ = DUP((struct ConstDeclaration){ .type = $1, .vars = $2}); }
    | type const_var_list ',' ';'
        { $$ = DUP((struct ConstDeclaration){ .type = $1, .vars = $2}); }
    ;

const_var_list
    : const_id_decl
        { $$ = DUP((struct ConstVarList){ .decl = $1, .next = NULL}); }
    | const_var_list ',' const_id_decl
        { $1->next = DUP((struct ConstVarList){ .decl = $3, .prev = $1, .next = NULL }); $$ = $1->next; }
    ;

const_id_decl
    : IDENTIFIER
        { $$ = DUP((struct ConstVarDecl){ .name = $1 }); }
    ;

declaration
    : storage_class type ';'
        { $$ = DUP((struct Declaration) { .class = $1, .type = $2, .vars = NULL }); }
    | storage_class type var_list ';'
        { $$ = DUP((struct Declaration) { .class = $1, .type = $2, .vars = $3 }); }
    | storage_class type var_list ',' ';'
        { $$ = DUP((struct Declaration) { .class = $1, .type = $2, .vars = $3 }); }
    | type ';'
        { $$ = DUP((struct Declaration) { .class = None, .type = $1, .vars = NULL }); }
    | type var_list ';'
        { $$ = DUP((struct Declaration) { .class = None, .type = $1, .vars = $2 }); }
    | type var_list ',' ';'
        { $$ = DUP((struct Declaration) { .class = None, .type = $1, .vars = $2 }); }
    ;

var_list
    : id_decl
        { $$ = DUP((struct VarList){ .decl = $1, .next = NULL}); }
    | var_list ',' id_decl
        { $1->next = DUP((struct VarList){ .decl = $3, .prev = $1, .next = NULL }); $$ = $1->next; }
    ;

id_decl
    : IDENTIFIER
        { $$ = DUP((struct VarDecl){ .name = $1, .val = NULL}); }
    | IDENTIFIER '=' initializer
        { $$ = DUP((struct VarDecl){ .name = $1, .val = $3}); }
    ;

initializer
    : assignment_expression
        { $$ = DUP_T(Initializer, Expr, .expr = $1); }
    | '{' initializer_list '}'
        { $$ = DUP_T(Initializer, Data, .data = $2); }
    | '{' initializer_list ',' '}'
        { $$ = DUP_T(Initializer, Data, .data = $2); }
    | '$' compound_statement
        { $$ = DUP_T(Initializer, Code, .code = $2); }
    ;

compound_literal_initializer
    : '{' initializer_list '}'
        { $$ = DUP_T(Initializer, Data, .data = $2); }
    | '{' initializer_list ',' '}'
        { $$ = DUP_T(Initializer, Data, .data = $2); }
    | '$' compound_statement
        { $$ = DUP_T(Initializer, Code, .code = $2); }
    ;

initializer_list
	: initializer
        { $$ = DUP((struct InitializerList){ .designation = NULL, .current = $1, .next = NULL}); }
	| designation initializer
        { $$ = DUP((struct InitializerList){ .designation = $1, .current = $2, .next = NULL}); }
	| initializer_list ',' initializer
        { $1->next = DUP((struct InitializerList){ .designation = NULL, .current = $3, .prev = $1, .next = NULL}); $$ = $1->next; }
	| initializer_list ',' designation initializer
        { $1->next = DUP((struct InitializerList){ .designation = $3, .current = $4, .prev = $1, .next = NULL}); $$ = $1->next; }
	;

designation
	: designator_list '='
	    { $$ = $1; }
	;

designator_list
	: designator
	    { $$ = DUP((struct DesignatorList){ .desig = $1, .next = NULL }); }
	| designator_list designator
	    { $1->next = DUP((struct DesignatorList){ .desig = $2, .prev = $1, .next = NULL }); $$ = $1->next; }
	;

designator
	: '[' constant_expression ']'
	    { $$ = DUP_T(Designator, DesignatorIndex, .index = $2); }
	| '.' IDENTIFIER
	    { $$ = DUP_T(Designator, Access, .access = $2); }
	;

storage_class
    : EXTERN
        { $$ = Static; }
    | STATIC
        { $$ = Extern; }
    ;

type
    : regular_type
        { $$ = $1; }
    | type_qualifier type
        {
            // Two qualifiers together get squashed into a qualifier bit vector
            if ($2->tag == Qualifier) {
                $$ = $2;
                $$->qualifier.qualifiers |= $1;
            } else {
                $$ = DUP_T(Type, Qualifier, .qualifier = { .qualifiers = $1, .t = $2});
            }
        }
    ;

regular_type
    : '(' regular_type ')'
	    { $$ = $2; }
    | concrete_type
	    { $$ = $1; }
    | regular_type type_qualifier
        {
            // Two qualifiers together get squashed into a qualifier bit vector
            if ($1->tag == Qualifier) {
                $$ = $1;
                $$->qualifier.qualifiers |= $2;
            } else {
                $$ = DUP_T(Type, Qualifier, .qualifier = { .qualifiers = $2, .t = $1});
            }
        }
    ;

concrete_type
    : base_type
    | TYPEOF '(' expression ')'
        { $$ = DUP_T(Type, TypeofExpr, .typeof_expr = $3); }
    | TYPEOF '<' type '>'
        { $$ = DUP_T(Type, TypeofType, .typeof_type = $3); }
    | '@' IDENTIFIER /*Itentifier types are only for use with #c_include*/
        { $$ = DUP_T(Type, CType, .c_type = $2); }
    | regular_type '&'
        { $$ = DUP_T(Type, Reference, .reference = $1); }
    | regular_type '[' ']'
        { $$ = DUP_T(Type, Array, .array = { .size = NULL, .t = $1}); }
    | regular_type '[' constant_expression ']'
        { $$ = DUP_T(Type, Array, .array = { .size = $3, .t = $1}); }
    | regular_type '(' '&' ')' '(' parameter_type_list ')'
        { $$ = DUP_T(Type, FunPointer, .fun_pointer = { .return_type = $1, .param_list = $6}); }
    ;


type_qualifier
	: MUT
	    { $$ = Mut; }
	| RESTRICT
	    { $$ = Restrict; }
	| VOLATILE
	    { $$ = Volatile; }
	;

base_type
	: VOID
	    { $$ = DUP_T(Type, Void); }
	| BOOL
	    { $$ = DUP_T(Type, Bool); }
	| U8
	    { $$ = DUP_T(Type, u8); }
	| I8 
	    { $$ = DUP_T(Type, i8); }
    | U16 
	    { $$ = DUP_T(Type, u16); }
    | I16 
	    { $$ = DUP_T(Type, i16); }
    | U32 
	    { $$ = DUP_T(Type, u32); }
    | I32 
	    { $$ = DUP_T(Type, i32); }
    | U64 
	    { $$ = DUP_T(Type, u64); }
    | I64 
	    { $$ = DUP_T(Type, i64); }
    | F32 
	    { $$ = DUP_T(Type, f32); }
    | F64
	    { $$ = DUP_T(Type, f64); }
    | struct_or_union_specifier
	    { $$ = $1; }
	| enum_specifier
	    { $$ = $1; }
	;

parameter_type_list
	:
	    { $$ = NULL; }
	| parameter_list
	    { $$ = $1; }
	| parameter_list ','
	    { $$ = $1; }
	| parameter_list ',' ELLIPSIS
	    { $1->next = DUP((struct TypeParamList){ .param = NULL, .prev = $1, .next = NULL }); $$ = $1->next; }
	| parameter_list ',' ELLIPSIS ','
	    { $1->next = DUP((struct TypeParamList){ .param = NULL, .prev = $1, .next = NULL }); $$ = $1->next; }
	;

parameter_list
	: parameter_declaration
	    { $$ = DUP((struct TypeParamList){ .param = $1, .next = NULL }); }
	| parameter_list ',' parameter_declaration
	    { $1->next = DUP((struct TypeParamList){ .param = $3, .prev = $1, .next = NULL }); $$ = $1->next; }
	;

parameter_declaration
	: type
	    { $$ = DUP((struct TypeParam) { .type = $1, .name = NULL }); }
	| type IDENTIFIER
	    { $$ = DUP((struct TypeParam) { .type = $1, .name = $2 }); }
	;


struct_or_union_specifier
	: STRUCT IDENTIFIER '{' struct_declaration_list '}'
        { $$ = DUP_T(Type, Struct, .struct_or_union_def = { .name = $2, .declarations = $4 }); }
	| STRUCT '{' struct_declaration_list '}'
        { $$ = DUP_T(Type, Struct, .struct_or_union_def = { .name = NULL, .declarations = $3 }); }
	| STRUCT IDENTIFIER
        { $$ = DUP_T(Type, Struct, .struct_or_union_def = { .name = $2, .declarations = NULL }); }
	| UNION IDENTIFIER '{' struct_declaration_list '}'
        { $$ = DUP_T(Type, Union, .struct_or_union_def = { .name = $2, .declarations = $4 }); }
	| UNION '{' struct_declaration_list '}'
        { $$ = DUP_T(Type, Union, .struct_or_union_def = { .name = NULL, .declarations = $3 }); }
	| UNION IDENTIFIER
        { $$ = DUP_T(Type, Union, .struct_or_union_def = { .name = $2, .declarations = NULL }); }
	;

struct_declaration_list
	: const_declaration
	    { $$ = DUP((struct ConstDeclarationList){ .decl = $1, .next = NULL }); }
	| struct_declaration_list const_declaration
	    { $1->next = DUP((struct ConstDeclarationList){ .decl = $2, .prev = $1, .next = NULL }); $$ = $1->next; }
	;

enum_specifier
	: ENUM '{' enumerator_list '}'
        { $$ = DUP_T(Type, Enum, .enum_def = { .name = NULL, .values = $3}); }
	| ENUM '{' enumerator_list ',' '}'
        { $$ = DUP_T(Type, Enum, .enum_def = { .name = NULL, .values = $3}); }
	| ENUM IDENTIFIER
        { $$ = DUP_T(Type, Enum, .enum_def = { .name = $2, .values = NULL}); }
	| ENUM IDENTIFIER '{' enumerator_list '}'
        { $$ = DUP_T(Type, Enum, .enum_def = { .name = $2, .values = $4}); }
	| ENUM IDENTIFIER '{' enumerator_list ',' '}'
        { $$ = DUP_T(Type, Enum, .enum_def = { .name = $2, .values = $4}); }
	;

enumerator_list
	: enumerator
	   { $$ = DUP((struct EnumeratorList){ .val = $1, .next = NULL }); }
	| enumerator_list ',' enumerator
	   { $1->next = DUP((struct EnumeratorList){ .val = $3, .prev = $1, .next = NULL }); $$ = $1->next; }
	;

enumerator
	: IDENTIFIER
	    { $$ = DUP((struct EnumValue) { .name = $1, .val = NULL }); }
	| IDENTIFIER '=' constant_expression
	    { $$ = DUP((struct EnumValue) { .name = $1, .val = $3 }); }
	;

statement
	: labeled_statement
	    { $$ = DUP_T(Statement, Labeled, .l = $1); }
	| compound_statement
	    { $$ = DUP_T(Statement, Block, .b = $1); }
	| expression_statement
	    { $$ = DUP_T(Statement, Expr, .e = $1); }
	| selection_statement
	    { $$ = DUP_T(Statement, Selection, .s = $1); }
	| iteration_statement
	    { $$ = DUP_T(Statement, Iteration, .i = $1); }
	| jump_statement
	    { $$ = DUP_T(Statement, Jump, .j = $1); }
	;

labeled_statement
	: IDENTIFIER ':' statement
	    { $$ = DUP_T(LabeledStatement, Label, .label_name = $1, .stat = $3); }
	| CASE constant_expression ':' statement
	    { $$ = DUP_T(LabeledStatement, Case, .case_expr = $2, .stat = $4); }
	| DEFAULT ':' statement
	    { $$ = DUP_T(LabeledStatement, Default_Label, .stat = $3); }
	;

compound_statement
	: '{' '}'
	    { $$ = NULL; }
	| '{' block_item_list '}'
	    { $$ = DUP((struct Block){ .contents = $2 }); }
	;

block_item_list
	: block_item
	    { $$ = DUP((struct BlockList){ .item = $1, .next = NULL }); }
	| block_item_list block_item
	    { $1->next = DUP((struct BlockList){ .item = $2, .prev = $1, .next = NULL }); $$ = $1->next; }
	;

block_item
	: declaration
	    { $$ = DUP_T(BlockItem, Statement, .decl = $1); }
	| statement
	    { $$ = DUP_T(BlockItem, Statement, .stat = $1); }
	;

expression_statement
	: ';'
	    { $$ = NULL; }
	| expression ';'
	    { $$ = $1; }
	;

selection_statement
	: IF '(' expression ')' statement
	    { $$ = DUP_T(SelectionStatement, If, .simple_if = { .clause = $3, .action = $5 }); }
	| IF '(' expression ')' statement ELSE statement
	    { $$ = DUP_T(SelectionStatement, IfElse, .if_else = { .clause = $3, .action_true = $5, .action_false = $7 }); }
	| SWITCH '(' expression ')' statement
	    { $$ = DUP_T(SelectionStatement, Switch, .switch_stat = { .clause = $3, .block = $5 }); }
	;

iteration_statement
	: WHILE '(' expression ')' statement
	    { $$ = DUP_T(IterationStatement, While, .while_dowhile_stat = { .expr = $3, .stat = $5 }); }
	| DO statement WHILE '(' expression ')' ';'
	    { $$ = DUP_T(IterationStatement, DoWhile, .while_dowhile_stat = { .expr = $5, .stat = $2 }); }
	| FOR '(' expression_statement expression_statement ')' statement
	    { $$ = DUP_T(IterationStatement, For_Expr, .for_stat_expr = { .init = $3, .clause = $4 }); }
	
	| FOR '(' expression_statement expression_statement expression ')' statement
	    { $$ = DUP_T(IterationStatement, For_Expr, .for_stat_expr = { .init = $3, .clause = $4, .update = $5 }); }
	| FOR '(' declaration expression_statement ')' statement
	    { $$ = DUP_T(IterationStatement, For_Decl, .for_stat_decl = { .init = $3, .clause = $4}); }
	| FOR '(' declaration expression_statement expression ')' statement
	    { $$ = DUP_T(IterationStatement, For_Decl, .for_stat_decl = { .init = $3, .clause = $4, .update = $5 }); }
	;

jump_statement
	: GOTO IDENTIFIER ';'
	    { $$ = DUP_T(JumpStatement, Goto, .goto_stat = { .label_name = $2 }); }
	| CONTINUE ';'
	    { $$ = DUP_T(JumpStatement, Continue); }
	| BREAK ';'
	    { $$ = DUP_T(JumpStatement, Break); }
	| RETURN ';'
	    { $$ = DUP_T(JumpStatement, Return, .return_stat = { .expr = NULL }); }
	| RETURN expression ';'
	    { $$ = DUP_T(JumpStatement, Return, .return_stat = { .expr = $2 }); }
	;

translation_unit
	: declaration
	    { $$ = DUP_T(TopLevel, Decl, .decl = $1, .next = NULL); }
	| C_INCLUDE STRING_LITERAL
	    { $$ = DUP_T(TopLevel, CInclude, .c_include = $2, .next = NULL); }
	| translation_unit declaration
	    { $1->next = DUP_T(TopLevel, Decl, .decl = $2, .prev = $1, .next = NULL); $$ = $1->next; }
	| translation_unit C_INCLUDE STRING_LITERAL
	    { $1->next = DUP_T(TopLevel, CInclude, .c_include = $3, .prev = $1, .next = NULL); $$ = $1->next; }
	;

top_level
    : translation_unit
        { transpile($1); }

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
