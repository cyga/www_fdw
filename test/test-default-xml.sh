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

sql="select title,link,snippet from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

# check same query (on purpose):
sql="select title,link,snippet from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

sql="select title,link,snippet from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

# check same query (on purpose):
sql="select title,link,snippet from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

sql="select title,link,snippet from www_fdw_test order by title"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

sql="select title,link,snippet from www_fdw_test order by title desc"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

sql="select title,link,snippet from www_fdw_test order by title limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||' "$sql"

kill $spid

perl -Mojo -e'a("/" => {text => "<doc><nrows>3</nrows><rows><row><id>12</id><b>true</b><d>2012-11-20</d><t>00:01:02</t><ts>2012-06-16 22:02:13</ts><title>3</title><snippet>SNIPPeT</snippet></row><row><id>13</id><b>false</b><d>2012-11-21</d><t>00:01:03</t><ts>2012-06-16 22:02:14</ts><title>TiTuL</title><snippet>SNIPPeT2</snippet></row></rows></doc>"})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select title,link,snippet from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'3||SNIPPeT\nTiTuL||SNIPPeT2' "$sql"

sql="select title,link,snippet from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||SNIPPeT' "$sql"

sql="select title,link,snippet from www_fdw_test order by snippet"
r=`$psql -tA -c"$sql"`
test "$r" $'3||SNIPPeT\nTiTuL||SNIPPeT2' "$sql"

sql="select title,link,snippet from www_fdw_test order by snippet desc"
r=`$psql -tA -c"$sql"`
test "$r" $'TiTuL||SNIPPeT2\n3||SNIPPeT' "$sql"

sql="select title,link,snippet from www_fdw_test order by title limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'3||SNIPPeT' "$sql"

sql="select id from www_fdw_test where id=12 limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'12' "$sql"

# TODO not implemented yet
# sql="select b from www_fdw_test where b=true limit 1"
# r=`$psql -tA -c"$sql"`
# test "$r" $'true' "$sql"

sql="select d from www_fdw_test where d='2012-11-20' limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'2012-11-20' "$sql"

sql="select t from www_fdw_test where t='00:01:02' limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'00:01:02' "$sql"

sql="select ts from www_fdw_test where ts='2012-06-16 22:02:13' limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'2012-06-16 22:02:13' "$sql"

kill $spid

# clean up
$psql -c"DROP EXTENSION IF EXISTS www_fdw CASCADE"
