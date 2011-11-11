#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*)/.*#$1#;'`
source "$test_dir/test.sh"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

trap 'if [ -n "$spid" ]; then echo "killing server $spid"; kill $spid; fi; exit' 2 13 15

perl -Mojo -e'a("/"=> { json => []})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

$psql -f "$test_dir/request-serialize-callback-type-json.sql"

for inf in "$test_dir/request-serialize-callback-type-json/"*".in"; do
	r=`$psql -tA < "$inf" 2>&1 | grep -v CONTEXT:`
	outf=`echo $inf | sed 's/\.in$/.out/'`
	out=`cat "$outf"`
	in=`cat "$inf"`
	test "$r" "$out" "$in"
done

$psql -f "$test_dir/request-serialize-callback-type-json-indent.sql"

for inf in "$test_dir/request-serialize-callback-type-json-indent/"*".in"; do
	r=`$psql -tA < "$inf" 2>&1 | grep -v CONTEXT:`
	outf=`echo $inf | sed 's/\.in$/.out/'`
	out=`cat "$outf"`
	in=`cat "$inf"`
	test "$r" "$out" "$in"
done


kill $spid
