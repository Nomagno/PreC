#!/bin/sh

# replace with the asolute path to the directory that contains prec_internal if installing this script
path=$(dirname "$0")

args=''

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
    transpiler="$path"/prec_internal
    if [ ! -e "$transpiler" ]; then
        echo "$transpiler: not found, revise your PreC source directory (might have to run \`make\`)"
        exit 1;
    fi

    tmp1=$(mktemp)

    tmp2=$(mktemp)
    mv "$tmp2" "$tmp2"."$ext"
    tmp2=$tmp2.$ext

    (cpp "$1" | grep -v '^# ')         > "$tmp1"   \
    && ("$transpiler" "$tmp1") 3>&2 2>&1 1>"$output" | sed "s|^$tmp1:|$1:|g" | error_handling "$1"
}

transpile_flag=FALSE

if [ "$1" = "-transpile" ]; then
    shift
    transpile_flag=TRUE
fi

process() {
    output=$(basename "$i").$ext

    if ! transpile "$i"; then
        rm -f "$tmp1" "$tmp2"
        exit 1;
    fi
    rm -f "$tmp1" "$tmp2"

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
    echo "cc $args"
    eval "cc $args"
fi
