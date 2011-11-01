#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*/).*#$1#;'`
source "$test_dir/test.sh"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

trap 'if [ -n "$spid" ]; then echo "killing server $spid"; kill $spid; fi; exit' 2 13 15

$psql -f "$test_dir/sql/response-iterate-callback.sql"

perl -Mojo -e'a("/" => {json => {nrows=>2,rows=>[{title=>"t0",link=>"l0",snippet=>"s0"},{title=>"t1",link=>"l1",snippet=>"s1"}]}})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'t0 hello world of callbacks|l0|s0\nt1 hello world of callbacks|l1|s1' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'t0 hello world of callbacks|l0|s0' "$sql"

sql="select * from www_fdw_test order by title"
r=`$psql -tA -c"$sql"`
test "$r" $'t0 hello world of callbacks|l0|s0\nt1 hello world of callbacks|l1|s1' "$sql"

sql="select * from www_fdw_test order by title desc"
r=`$psql -tA -c"$sql"`
test "$r" $'t1 hello world of callbacks|l1|s1\nt0 hello world of callbacks|l0|s0' "$sql"

sql="select * from www_fdw_test order by title limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'t0 hello world of callbacks|l0|s0' "$sql"

kill $spid