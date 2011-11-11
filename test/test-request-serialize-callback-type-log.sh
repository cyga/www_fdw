#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*)/.*#$1#;'`
source "$test_dir/test.sh"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

trap 'if [ -n "$spid" ]; then echo "killing server $spid"; kill $spid; fi; exit' 2 13 15

$psql -f "$test_dir/request-serialize-callback-type-log.sql"

perl -Mojo -e'a("/"=>sub{$c=shift;$t=$c->param("title");$c->render_json({nrows=>2,rows=>[{title=>$t,link=>"l0",snippet=>"s0"},{title=>$t,link=>"l1",snippet=>"s1"}]})})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'titel|l0|s0\ntitel|l1|s1' "$sql"

# same query (on purpose):
sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'titel|l0|s0\ntitel|l1|s1' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'titel|l0|s0' "$sql"

# same query (on purpose):
sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'titel|l0|s0' "$sql"

sql="select * from www_fdw_test order by link"
r=`$psql -tA -c"$sql"`
test "$r" $'titel|l0|s0\ntitel|l1|s1' "$sql"

sql="select * from www_fdw_test order by link desc"
r=`$psql -tA -c"$sql"`
test "$r" $'titel|l1|s1\ntitel|l0|s0' "$sql"

sql="select * from www_fdw_test order by link limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'titel|l0|s0' "$sql"

kill $spid

# clean up
$psql -c"DROP EXTENSION IF EXISTS www_fdw CASCADE"
