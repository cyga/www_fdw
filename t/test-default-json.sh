#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*/).*#$1#;'`
source "$test_dir/test.sh"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

$psql -f "$test_dir/default-json.sql"

perl -Mojo -e'a("/" => {json => {nrows=>2,rows=>[{title=>"t0",link=>"l0",snippet=>"s0"},{title=>"t1",link=>"l1",snippet=>"s1"}]}})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0\nt1|l1|s1' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0' "$sql"

sql="select * from www_fdw_test order by title"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0\nt1|l1|s1' "$sql"

sql="select * from www_fdw_test order by title desc"
r=`$psql -tA -c"$sql"`
test "$r" $'t1|l1|s1\nt0|l0|s0' "$sql"

sql="select * from www_fdw_test order by title limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0' "$sql"

kill $spid

perl -Mojo -e'a("/" => {json => {nrows=>2,smth=>[1],rows=>[{title=>1},{title=>2},{title=>3}]}})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select title from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'1\n2\n3' "$sql"

sql="select title from www_fdw_test limit 2"
r=`$psql -tA -c"$sql"`
test "$r" $'1\n2' "$sql"

sql="select title from www_fdw_test order by title"
r=`$psql -tA -c"$sql"`
test "$r" $'1\n2\n3' "$sql"

sql="select title from www_fdw_test order by title desc"
r=`$psql -tA -c"$sql"`
test "$r" $'3\n2\n1' "$sql"

sql="select title from www_fdw_test order by title desc limit 2"
r=`$psql -tA -c"$sql"`
test "$r" $'3\n2' "$sql"

kill $spid