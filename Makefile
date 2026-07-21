prec_internal: prec_main.c prec.tab.c prec.tab.h lex.yy.c helpers/*.c helpers/*.h
	gcc -Oz -Wno-unused-result lex.yy.c prec_main.c prec.tab.c helpers/*.c -lfl -o prec_internal
prec.tab.c prec.tab.h: prec.y
	bison -d -v prec.y
lex.yy.c:prec.l
	flex prec.l
install: prec_internal
	cp precc.sh ~/.local/bin/precc
	chmod +x ~/.local/bin/precc
	./precc_embedder.sh ~/.local/bin/precc
	echo "preCC installed to ~/.local/bin/precc"
clean:
	rm -f prec_internal lex.yy.c prec.tab.c prec.tab.h prec.output examples/*.c examples/*.h examples/a.out a.out *.prec.c *.o
