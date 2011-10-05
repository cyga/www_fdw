#!/bin/sh
TMP_DIR=/tmp
t=json_parser
GCC_OPTIONS="-O2 -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -fpic"
GCC_OPTIONS_DEBUG="-g -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -fpic"
gcc $GCC_OPTIONS -o $t $t.c ../libjson-0.8/json.o

for inf in $t.in.*
do
	outf=`echo $inf | sed 's#\.in\.#.out.#'`
	echo "===== TEST: $inf $outf ====="
	cat $inf | ./$t 2>&1 > "$TMP_DIR/$outf"
	diff -u "$TMP_DIR/$outf" $outf 
	rm -f "$TMP_DIR/$outf"
done
