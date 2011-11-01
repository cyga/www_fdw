# available only if following json extension is installed:
# http://git.postgresql.org/gitweb/?p=json-datatype.git;a=summary

#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*/).*#$1#;'`
source "$test_dir/test.sh"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

trap 'if [ -n "$spid" ]; then echo "killing server $spid"; kill $spid; fi; exit' 2 13 15 

$psql -f "$test_dir/json-response-deserialize-callback.sql"

perl -Mojo -e'sub f{{title=>"t"ne$_[0]?$_[0]:"t".$_{t}++,link=>"l"ne$_[1]?$_[1]:"l".$_{l}++,snippet=>"s"ne$_[2]?$_[2]:"s".$_{s}++}} a("/"=>sub{undef%_;$c=shift;$t=$c->param("title")//"t";$l=$c->param("link")//"l";$s=$c->param("snippet")//"s";$c->render_json({nrows=>2,rows=>[f($t,$l,$s),f($t,$l,$s)]})})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0\nt1|l1|s1' "$sql"

# same query (on purpose):
sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0\nt1|l1|s1' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0' "$sql"

# same query (on purpose):
sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0' "$sql"

sql="select * from www_fdw_test order by link"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0\nt1|l1|s1' "$sql"

sql="select * from www_fdw_test order by link desc"
r=`$psql -tA -c"$sql"`
test "$r" $'t1|l1|s1\nt0|l0|s0' "$sql"

sql="select * from www_fdw_test order by link limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0' "$sql"

sql="select * from www_fdw_test order by link limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'t0|l0|s0' "$sql"

sql="select * from www_fdw_test where title='TITLE'"
r=`$psql -tA -c"$sql"`
test "$r" $'TITLE|l0|s0\nTITLE|l1|s1' "$sql"

sql="select * from www_fdw_test where title='TITLE' order by snippet"
r=`$psql -tA -c"$sql"`
test "$r" $'TITLE|l0|s0\nTITLE|l1|s1' "$sql"

sql="select * from www_fdw_test where title='TITLE' order by snippet desc"
r=`$psql -tA -c"$sql"`
test "$r" $'TITLE|l1|s1\nTITLE|l0|s0' "$sql"

sql="select * from www_fdw_test where title='TITLE' order by snippet desc limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'TITLE|l1|s1' "$sql"

sql="select * from www_fdw_test where title='TITLE' and link='LINK'"
r=`$psql -tA -c"$sql"`
test "$r" $'TITLE|LINK|s0\nTITLE|LINK|s1' "$sql"

kill $spid