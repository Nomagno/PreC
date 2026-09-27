main: prec_internal

install: main
	cp precc.sh ~/.local/bin/precc
	sed -i "s/^transpiler_code='REPLACE ME'/transpiler_code='REPLACED'/" ~/.local/bin/precc
	chmod +x ~/.local/bin/precc

	cp build/prec_internal ~/.local/bin/prec_internal
	chmod +x ~/.local/bin/prec_internal

	echo "preCC installed to ~/.local/bin/precc, with auxiliary file ~/.local/bin/prec_internal"
clean:
	rm -rf *.prec.c a.out *.o examples/a.out examples/*.o build/


prec_internal: prec_main.c build/prec.tab.c build/prec.tab.h build/lex.yy.c helpers/*.c helpers/*.h
	mkdir -p build/
	gcc -g -Wall -Wenum-conversion -Wswitch-enum prec_main.c helpers/*.c build/lex.yy.c build/prec.tab.c -lfl -o build/prec_internal

build/prec.tab.c build/prec.tab.h: prec.y
	mkdir -p build/
	bison -d -v -o build/prec.tab.c prec.y
build/lex.yy.c: prec.l
	mkdir -p build/
	flex -o build/lex.yy.c prec.l
