prec: prec_main.c prec.tab.c prec.tab.h lex.yy.c helpers/*.c helpers/*.h
	gcc -g lex.yy.c prec_main.c prec.tab.c helpers/*.c -lfl -o prec
prec.tab.c prec.tab.h: prec.y
	bison -d -v prec.y
lex.yy.c:prec.l
	flex prec.l
clean:
	rm -f prec lex.yy.c prec.tab.c prec.tab.h prec.output
