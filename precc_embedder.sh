#!/bin/sh
printf 's|REPLACE ME|'    > tmp.sed
base64 prec_internal -w 0 >> tmp.sed
echo   '|'                >> tmp.sed

sed -f tmp.sed -i "$1"
status=$?

# Limit width
fold -w 120 "$1" > "$1".tmp
cp "$1".tmp "$1"
rm "$1".tmp

rm -f tmp.sed

exit "$status"
