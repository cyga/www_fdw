#!/bin/bash
test_dir=`echo $0 | perl -pe's#(.*)/.*#$1#;'`
source "$test_dir/test.sh"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

trap 'if [ -n "$spid" ]; then echo "killing server $spid"; kill $spid; fi; exit' 2 13 15

$psql -f "$test_dir/request-serialize-callback-type-json.sql"

perl -Mojo -e'a("/"=> { json => []})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits

sql="select * from www_fdw_test"
r=`$psql -tA -c"$sql" 2>&1`
test "$r" $'INFO:  quals parameter: \nCONTEXT:  SQL statement "SELECT * FROM test_request_serialize_callback($1,$2,$3,$4)"' "$sql"

# TODO DEBUGk HERE: use separate files (in separate folder)
# to check diffs
# qa/fix serialize
sql="select * from www_fdw_test where title='TITLE'"
r=`$psql -tA -c"$sql" 2>&1`
test "$r" $'INFO:  quals parameter: {"name":"OpExpr","params":["opno":"98","opfuncid":"67","opresulttype":"16","opretset":"","opcollid":"0","inputcollid":"100","location":"38"],"children":[{"name":"Var","params":["varno":"1","varattno":"1","vartype":"25","vartypmod":"-1","varcollid":"100","varlevelsup":"0","varnoold":"1","varoattno":"1","location":"33"],"value":""},{"name":"Const","params":[],"value":"TITLE"}]}\nCONTEXT:  SQL statement "SELECT * FROM test_request_serialize_callback($1,$2,$3,$4)"' "$sql"

kill $spid
