#!/bin/sh
TMP_DIR=/tmp
t=json_parser

make

for inf in $t.in.*
do
	outf=`echo $inf | sed 's#\.in\.#.out.#'`
	echo "===== TEST: $inf $outf ====="
	cat $inf | ./$t 2>&1 > "$TMP_DIR/$outf"
	diff -u "$TMP_DIR/$outf" $outf 
	rm -f "$TMP_DIR/$outf"
done

make clean