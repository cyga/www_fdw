#!/bin/bash
t=`echo $0 | perl -pe's#(.*/).*#$1/test.sh#;'`
source "$t"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

perl -Mojo -e'a("/" => {json => {nrows=>2,rows=>[{title=>"t0",link=>"l0",snippet=>"s0"},{title=>"t1",link=>"l1",snippet=>"s1"}]}})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits
r=`$psql -tA -c'select * from www_fdw_test'`
test "$r" $'t0|l0|s0\nt1|l1|s1'
kill $spid

perl -Mojo -e'a("/" => {json => {nrows=>2,smth=>[1],rows=>[{title=>1},{title=>2},{title=>3}]}})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits
r=`$psql -tA -c'select title from www_fdw_test'`
test "$r" $'1\n2\n3'
kill $spid
