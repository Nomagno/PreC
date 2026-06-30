#include <stdio.h>
#include <stdlib.h>

extern char *yytext;
extern int  yyleng;
extern FILE *yyin;
extern int yyparse();
FILE *file;
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Correct use: %s file\n",argv[0]);
        exit(1);
    }
    FILE *file = fopen(argv[1],"r");
    if (file == 0) {
        fprintf(stderr, "Could not open %s\n",argv[1]);
        exit(1);
    }
    yyin = file;
    yyparse();
    fclose(file);
}
