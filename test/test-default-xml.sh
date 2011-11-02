#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*)/.*#$1#;'`
source "$test_dir/test.sh"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

trap 'if [ -n "$spid" ]; then echo "killing server $spid"; kill $spid; fi; exit' 2 13 15

$psql -f "$test_dir/default-xml.sql"

perl -Mojo -e'a("/" => {text => "<doc><nrows>3</nrows><rows><row><title>3</title></row></rows></doc>"})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

# check same query (on purpose):
sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

# check same query (on purpose):
sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

sql="select * from www_fdw_test order by title"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

sql="select * from www_fdw_test order by title desc"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

sql="select * from www_fdw_test order by title limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

kill $spid

perl -Mojo -e'a("/" => {text => "<doc><nrows>3</nrows><rows><row><title>3</title><snippet>SNIPPeT</snippet></row><row><title>TiTuL</title><snippet>SNIPPeT2</snippet></row></rows></doc>"})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'3||SNIPPeT\nTiTuL||SNIPPeT2' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||SNIPPeT' "$sql"

sql="select * from www_fdw_test order by snippet"
r=`$psql -tA -c"$sql"`
test "$r" $'3||SNIPPeT\nTiTuL||SNIPPeT2' "$sql"

sql="select * from www_fdw_test order by snippet desc"
r=`$psql -tA -c"$sql"`
test "$r" $'TiTuL||SNIPPeT2\n3||SNIPPeT' "$sql"

sql="select * from www_fdw_test order by title limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||SNIPPeT' "$sql"

kill $spid

