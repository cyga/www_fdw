#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*/).*#$1#;'`
source "$test_dir/test.sh"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

############ request_serialize_type=default (log)

$psql -f "$test_dir/request-serialize-callback.sql"

perl -Mojo -e'a("/"=>sub{$c=shift;$t=$c->param("title");$c->render_json({nrows=>2,rows=>[{title=>$t,link=>"l0",snippet=>"s0"},{title=>$t,link=>"l1",snippet=>"s1"}]})})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'titel|l0|s0\ntitel|l1|s1' "$sql"

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

############ request_serialize_type=null

$psql -f "$test_dir/request-serialize-callback-type-null.sql"

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'titel2|l0|s0\ntitel2|l1|s1' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'titel2|l0|s0' "$sql"

sql="select * from www_fdw_test order by link"
r=`$psql -tA -c"$sql"`
test "$r" $'titel2|l0|s0\ntitel2|l1|s1' "$sql"

sql="select * from www_fdw_test order by link desc"
r=`$psql -tA -c"$sql"`
test "$r" $'titel2|l1|s1\ntitel2|l0|s0' "$sql"

sql="select * from www_fdw_test order by link limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'titel2|l0|s0' "$sql"

############ request_serialize_type=default
############ post

$psql -f "$test_dir/request-serialize-callback-post.sql"

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title|l0|s0\npost_title|l1|s1' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title|l0|s0' "$sql"

sql="select * from www_fdw_test order by link"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title|l0|s0\npost_title|l1|s1' "$sql"

sql="select * from www_fdw_test order by link desc"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title|l1|s1\npost_title|l0|s0' "$sql"

sql="select * from www_fdw_test order by link limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title|l0|s0' "$sql"

kill $spid

perl -Mojo -e'a("/"=>sub{$c=shift;$t=$c->param("title")//"t";$l=$c->param("link")//"l";$s=$c->param("snippet")//"s";$c->render_json({nrows=>2,rows=>[{title=>$t,link=>$l,snippet=>$s},{title=>$t,link=>$l,snippet=>$s}]})})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

$psql -c"UPDATE post_data SET value='title=post_title2&link=post_link2&snippet=post_snippet2' WHERE key='data'"

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2\npost_title2|post_link2|post_snippet2' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2' "$sql"

sql="select * from www_fdw_test order by link"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2\npost_title2|post_link2|post_snippet2' "$sql"

sql="select * from www_fdw_test order by link desc"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2\npost_title2|post_link2|post_snippet2' "$sql"

sql="select * from www_fdw_test order by link limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2' "$sql"

$psql -c"UPDATE post_data SET value='application/x-www-form-urlencoded' WHERE key='content_type'"

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2\npost_title2|post_link2|post_snippet2' "$sql"

sql="select * from www_fdw_test limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2' "$sql"

sql="select * from www_fdw_test order by link"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2\npost_title2|post_link2|post_snippet2' "$sql"

sql="select * from www_fdw_test order by link desc"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2\npost_title2|post_link2|post_snippet2' "$sql"

sql="select * from www_fdw_test order by link limit 1"
r=`$psql -tA -c"$sql"`
test "$r" $'post_title2|post_link2|post_snippet2' "$sql"

kill $spid

