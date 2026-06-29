D			[0-9]
L			[a-zA-Z_]
H			[a-fA-F0-9]
E			([Ee][+-]?{D}+)
P                       ([Pp][+-]?{D}+)
FS			(f|F|l|L)
IS                      ((u|U)|(u|U)?(l|L|ll|LL)|(l|L|ll|LL)(u|U))

%{
#include <stdio.h>
int yylex(void);
int yyerror(const char *s);

#include "y.tab.h"

void count(void);
void comment(void);
%}

%%
"/*"			{ comment(); }
"//"[^\n]*              { /* consume //-comment */ }


"#c_include"    { count(); return(C_INCLUDE); }
"not"		    { count(); return(NOT); }
"and"		    { count(); return(AND); }
"xor"	    	{ count(); return(XOR); }
"or"    		{ count(); return(OR); }
"break"			{ count(); return(BREAK); }
"case"			{ count(); return(CASE); }
"mut"			{ count(); return(MUT); }
"continue"		{ count(); return(CONTINUE); }
"default"		{ count(); return(DEFAULT); }
"do"			{ count(); return(DO); }
"else"			{ count(); return(ELSE); }
"enum"			{ count(); return(ENUM); }
"extern"		{ count(); return(EXTERN); }
"for"			{ count(); return(FOR); }
"goto"			{ count(); return(GOTO); }
"if"			{ count(); return(IF); }
"restrict"		{ count(); return(RESTRICT); }
"return"		{ count(); return(RETURN); }
"sizeof"		{ count(); return(SIZEOF); }
"static"		{ count(); return(STATIC); }
"struct"		{ count(); return(STRUCT); }
"switch"		{ count(); return(SWITCH); }
"union"			{ count(); return(UNION); }
"volatile"		{ count(); return(VOLATILE); }
"while"			{ count(); return(WHILE); }
"bool"			{ count(); return(BOOL); }
"u8"			{ count(); return(U8); }
"i8"			{ count(); return(I8); }
"u16"			{ count(); return(U16); }
"i16"			{ count(); return(I16); }
"u32"			{ count(); return(U32); }
"i32"			{ count(); return(I32); }
"u64"			{ count(); return(U64); }
"f32"			{ count(); return(F32); }
"f64"			{ count(); return(F64); }
"void"			{ count(); return(VOID); }

{L}({L}|{D})*		{ count(); return(IDENTIFIER); }

0[xX]{H}+{IS}?		{ count(); return(INT_CONSTANT); }
0[0-7]*{IS}?		{ count(); return(INT_CONSTANT); }
[1-9]{D}*{IS}?		{ count(); return(INT_CONSTANT); }
L?'(\\.|[^\\'\n])+'	{ count(); return(INT_CONSTANT); }

{D}+{E}{FS}?		{ count(); return(FLOAT_CONSTANT); }
{D}*"."{D}+{E}?{FS}?	{ count(); return(FLOAT_CONSTANT); }
{D}+"."{D}*{E}?{FS}?	{ count(); return(FLOAT_CONSTANT); }
0[xX]{H}+{P}{FS}?	{ count(); return(FLOAT_CONSTANT); }
0[xX]{H}*"."{H}+{P}{FS}?     { count(); return(FLOAT_CONSTANT); }
0[xX]{H}+"."{H}*{P}{FS}?     { count(); return(FLOAT_CONSTANT); }


L?\"(\\.|[^\\"\n])*\"	{ count(); return(STRING_LITERAL); }

"..."			{ count(); return(ELLIPSIS); }
">>"			{ count(); return(RIGHT_OP); }
"<<"			{ count(); return(LEFT_OP); }
"++"			{ count(); return(INC_OP); }
"--"			{ count(); return(DEC_OP); }
"->"			{ count(); return(PTR_OP); }
"&&"			{ count(); return(AND_OP); }
"||"			{ count(); return(OR_OP); }
"<="			{ count(); return(LE_OP); }
">="			{ count(); return(GE_OP); }
"=="			{ count(); return(EQ_OP); }
"!="			{ count(); return(NE_OP); }
";"			{ count(); return(';'); }
("{")		{ count(); return('{'); }
("}")		{ count(); return('}'); }
","			{ count(); return(','); }
":"			{ count(); return(':'); }
"="			{ count(); return('='); }
"("			{ count(); return('('); }
")"			{ count(); return(')'); }
("[")		{ count(); return('['); }
("]")		{ count(); return(']'); }
"$"			{ count(); return('$'); }
"."			{ count(); return('.'); }
"!"			{ count(); return('!'); }
"~"			{ count(); return('~'); }
"-"			{ count(); return('-'); }
"+"			{ count(); return('+'); }
"*"			{ count(); return('*'); }
"@"			{ count(); return('@'); }
"&"			{ count(); return('&'); }
"^"			{ count(); return('^'); }
"/"			{ count(); return('/'); }
"%"			{ count(); return('%'); }
"<"			{ count(); return('<'); }
">"			{ count(); return('>'); }
"?"			{ count(); return('?'); }

[ \t\v\n\f]		{ count(); }
.			{ /* Add code to complain about unmatched characters */ }

%%

int yywrap(void)
{
	return 1;
}


void comment(void)
{
	char c, prev = 0;
  
	while ((c = input()) != 0)      /* (EOF maps to 0) */
	{
		if (c == '/' && prev == '*')
			return;
		prev = c;
	}
	yyerror("unterminated comment");
}


int column = 0;

void count(void)
{
	int i;

	for (i = 0; yytext[i] != '\0'; i++)
		if (yytext[i] == '\n')
			column = 0;
		else if (yytext[i] == '\t')
			column += 8 - (column % 8);
		else
			column++;

	ECHO;
}
