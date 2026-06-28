prec: prec.l prec.y
	flex prec.l
	bison -dy prec.y --report=states,itemsets -Wno-yacc
	gcc y.tab.c lex.yy.c -o prec
clean:
	rm -f prec lex.yy.c y.tab.c y.tab.h y.output
