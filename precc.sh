#!/bin/sh

# replace with the asolute path to the directory that contains prec_internal if installing this script
path=$(dirname "$0")

args=''

process() {
    file=$(basename "$1")
    ext=$2
    (cpp "$1" | grep -v '^# ')                  > "$file".tmp        \
    && ("$path"/prec_internal "$file".tmp)            > "$file".tmp."$ext" \
    && (cpp "$file".tmp."$ext" | grep -v '^# ') > "$file"."$ext"
}

cleanup() {
    file=$(basename "$1")
    ext=$2
    rm -f "$file".tmp "$file".tmp."$ext"
}

transpile=FALSE

if [ "$1" = "-transpile" ]; then
    shift
    transpile=TRUE
fi

for i in "$@"; do
    if echo "$i" | grep -q '.prec$'; then
        if ! process "$i" c; then exit 1; fi
        cleanup "$i" c
        if [ $transpile = FALSE ]; then
            args=$args' '\'"$(basename "$i")".c\'
        fi
    elif echo "$i" | grep -q '.preh$'; then
        if ! process "$i" h; then exit 1; fi
        cleanup "$i" h
        if [ $transpile = FALSE ]; then
            args=$args' '\'"$(basename "$i")".h\'
        fi
    else
        args=$args' '\'"$i"\'
    fi
done

if [ $transpile = FALSE ]; then
    echo "cc $args"
    eval "cc $args"
fi
