#!/bin/sh

args=''

if [ $1 = "-transpile" ]; then
    shift
    for i in $@; do
        if echo $i | grep -q '.prec$'; then
            locali=$(basename $i)
            (cpp $i | grep -v '^# ' ) > $locali.tmp &&
            (./prec_internal $locali.tmp; ) > $locali.tmp.c &&
            (cpp $locali.tmp.c | grep -v '^# ' ) > $locali.c &&
            rm $locali.tmp && rm $locali.tmp.c
        elif echo $i | grep -q '.preh$'; then
            locali=$(basename $i)
            (cpp $i | grep -v '^# ' ) > $locali.tmp &&
            (./prec_internal $locali.tmp; ) > $locali.tmp.h &&
            (cpp $locali.tmp.h | grep -v '^# ' ) > $locali.h &&
            rm $locali.tmp && rm $locali.tmp.h
        fi
    done
else
    for i in $@; do
        if echo $i | grep -q '.prec$'; then
            locali=$(basename $i)
            (cpp $i | grep -v '^# ' ) > $locali.tmp &&
            (./prec_internal $locali.tmp; ) > $locali.tmp.c &&
            (cpp $locali.tmp.c | grep -v '^# ' ) > $locali.c &&
            rm $locali.tmp && rm $locali.tmp.c
            if [ $? -ne 0 ]; then
                exit
            fi
            args=$args" "$locali.c
        elif echo $i | grep -q '.preh$'; then
            locali=$(basename $i)
            (cpp $i | grep -v '^# ' ) > $locali.tmp &&
            (./prec_internal $locali.tmp; ) > $locali.tmp.h &&
            (cpp $locali.tmp.h | grep -v '^# ' ) > $locali.h &&
            rm $locali.tmp && rm $locali.tmp.h
            if [ $? -ne 0 ]; then
                exit
            fi
            args=$args" "$locali.h
        else 
            args=$args" "$i
        fi
    done
    echo cc $args
    cc $args
fi
