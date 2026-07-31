#!/bin/sh

args=''

# ``make install"" will replace this with "REPLACED"
transpiler_code='REPLACE ME'

error_handling() {
    was_there_error=FALSE;
    IFS=$(echo)
    while read -r line; do
        if echo "$line" | grep -q "^$1:"; then
            was_there_error=TRUE
        else
            was_there_error=FALSE
        fi
        echo "$line" 1>&2
    done
    if [ $was_there_error = TRUE ]; then
        exit 1
    fi
}

transpile() {
    if [ "$transpiler_code" = 'REPLACE ME' ]; then
        transpiler=./prec_internal
    else
        transpiler=prec_internal
    fi

    if ! command -v "$transpiler" >/dev/null 2>&1; then
        echo "$transpiler: not found, revise your PreC source directory or PATH (might have to run 'make' or 'make install')"
        exit 1;
    fi

    tmp1=$(mktemp)

    tmp2=$(mktemp)
    mv "$tmp2" "$tmp2"."$ext"
    tmp2=$tmp2.$ext

    ($c_preprocessor "$1" | grep -v '^# ')         > "$tmp1"   \
    && ("$transpiler" "$tmp1") 3>&2 2>&1 1>"$output" | sed "s|^$tmp1:|$1:|g" | error_handling "$1"
}

transpile_flag=FALSE
fsyntax_only_flag=FALSE

c_compiler=cc
c_preprocessor=cpp

if [ "$1" = "-prec_custom_cc" ]; then
    c_compiler=$2
    shift
    shift
fi

if [ "$1" = "-prec_custom_cpp" ]; then
    c_preprocessor=$2
    shift
    shift
fi

if [ "$1" = "-transpile" ]; then
    shift
    transpile_flag=TRUE
fi

# If -fsyntax-only, delete the output file after GCC has processed it
if [ "$1" = "-fsyntax-only" ]; then
    fsyntax_only_flag=TRUE
fi

cleanup() {
    rm -f "$tmp1" "$tmp2"
    if [ "$transpiler" != "./prec_internal" ]; then
        rm -f "$transpiler";
    fi
}

process() {
    output=$(basename "$i").$ext

    if ! transpile "$i"; then
        cleanup
        exit 1;
    fi
    cleanup

    args=$args' '\'"$output"\'
}

for i in "$@"; do
    case "$i" in
      *.prec)
        ext=c
        process
        ;;
      *.preh)
        ext=h
        process
        ;;
      *)
        args=$args' '\'"$i"\'
        ;;
    esac
done

if [ $transpile_flag = FALSE ]; then
    echo "$c_compiler $args"
    eval "$c_compiler $args"
fi

if [ $fsyntax_only_flag = TRUE ]; then
    for i in $args; do
        case "$i" in
          \'*.prec.c\')
            eval "rm -f -- $i"
            ;;
          \'*.preh.h\')
            eval "rm -f -- $i"
            ;;
          *)
            ;;
        esac
    done
fi
