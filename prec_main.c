#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

extern char *yytext;
extern int  yyleng;
extern FILE *yyin;
extern int yyparse();

FILE *file;
char *filename = NULL;
char *pretty_filename = NULL;
bool disable_linetranslation = false;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Correct use: %s file [optional pretty filename]\n",argv[0]);
        exit(1);
    }

    FILE *file = fopen(argv[1],"r");

    if (file == 0) {
        fprintf(stderr, "Could not open %s\n",argv[1]);
        exit(1);
    }

    filename = argv[1];

    if (argc >= 3)
        pretty_filename = argv[2];

    if (argc >= 4 && strcmp(argv[3], "DISABLE_LINEPRINTING") == 0)
        disable_linetranslation = true;

    yyin = file;
    yyparse();
    fclose(file);
}
