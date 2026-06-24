%expect 1

%token IDENTIFIER CONSTANT STRING_LITERAL SIZEOF
%token PTR_OP INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP

%token EXTERN STATIC RESTRICT MUT VOLATILE 
%token BOOL U8 I8 U16 I16 U32 I32 U64 I64 F32 F64 VOID
%token STRUCT UNION ENUM
%token ELLIPSIS

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

%{
    int yylex(void);
    int yyerror(const char *s);
%}

%%

primary_expression
	: IDENTIFIER
	| CONSTANT
	| STRING_LITERAL
	| '(' expression ')'
	;

postfix_expression
	: primary_expression
	| postfix_expression '[' expression ']'
	| postfix_expression '(' ')'
	| postfix_expression '(' argument_expression_list ')'
	| postfix_expression '.' IDENTIFIER
	| postfix_expression PTR_OP IDENTIFIER
	| postfix_expression INC_OP
	| postfix_expression DEC_OP
	| '(' type ')' '{' initializer_list '}'
	| '(' type ')' '$' compound_statement
	;

argument_expression_list
	: assignment_expression
	| argument_expression_list ',' assignment_expression
	;

unary_expression
	: postfix_expression
	| INC_OP unary_expression
	| DEC_OP unary_expression
	| NOT cast_expression
	| '&' cast_expression
	| '^' cast_expression
	| '~' cast_expression
	| '!' cast_expression
	| SIZEOF unary_expression
	| SIZEOF '(' type ')'
	;

cast_expression
	: unary_expression
	| '(' type ')' cast_expression
	;

arithmetic_expression
    : cast_expression
    | arithmetic_expression '*' arithmetic_expression
    | arithmetic_expression '/' arithmetic_expression
    | arithmetic_expression '%' arithmetic_expression
    | arithmetic_expression '+' arithmetic_expression
    | arithmetic_expression '-' arithmetic_expression
    | arithmetic_expression LEFT_OP arithmetic_expression
    | arithmetic_expression RIGHT_OP arithmetic_expression
    | arithmetic_expression '<' arithmetic_expression
    | arithmetic_expression '>' arithmetic_expression
    | arithmetic_expression LE_OP arithmetic_expression
    | arithmetic_expression GE_OP arithmetic_expression
    | arithmetic_expression EQ_OP arithmetic_expression
    | arithmetic_expression NE_OP arithmetic_expression
    | arithmetic_expression AND arithmetic_expression
    | arithmetic_expression XOR arithmetic_expression
    | arithmetic_expression OR arithmetic_expression
    | arithmetic_expression AND_OP arithmetic_expression
    | arithmetic_expression OR_OP arithmetic_expression
    ;

conditional_expression
	: arithmetic_expression
	| arithmetic_expression '?' expression ':' arithmetic_expression
	;

/*Missing: check it's actually constant lol*/
constant_expression :
    | conditional_expression
    ;

assignment_expression
	: conditional_expression
	| unary_expression '=' assignment_expression
	;

expression
	: assignment_expression
	| expression ',' assignment_expression
	;

declaration
    : storage_class declaration
    | type ';'
    | type var_list ';'
    ;

var_list
    : id_decl
    | var_list ',' id_decl
    ;

id_decl :
    IDENTIFIER
    | IDENTIFIER '=' initializer;

initializer
    : assignment_expression
    | '{' initializer_list '}'
    | '$' compound_statement
    ;

storage_class
    :
    /*| empty*/
    EXTERN
    | STATIC
    ;

type
    : base_type
    | type_qualifier
    ;

initializer_list
	: initializer
	| designation initializer
	| initializer_list ',' initializer
	| initializer_list ',' designation initializer
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

base_type
	: VOID
	| BOOL
    | U8
    | I8
    | U16
    | I16
    | U32
    | I32
    | U64
    | I64
    | F32
    | F64
	| struct_or_union_specifier
	| enum_specifier
	;

struct_or_union_specifier
	: struct_or_union /*TODO: COMPLETE*/
	;

struct_or_union
	: STRUCT
	| UNION
	;

enum_specifier
	: ENUM '{' enumerator_list '}'
	| ENUM IDENTIFIER '{' enumerator_list '}'
	| ENUM IDENTIFIER
	;

enumerator_list
	: enumerator
	| enumerator_list ',' enumerator
	;

enumerator
	: IDENTIFIER
	| IDENTIFIER '=' constant_expression
	;

type_qualifier
	: MUT
	| RESTRICT
	| VOLATILE
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
	| compound_statement /*TODO: remove this and allow block literals*/
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
