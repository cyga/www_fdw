#!/bin/bash
t=`echo $0 | perl -pe's#(.*/).*#$1/test.sh#;'`
source "$t"

bin=`pg_config --bindir`
psql="$bin/psql"

waits=3

perl -Mojo -e'a("/" => {text => "<doc><nrows>3</nrows><rows><row><title>3</title></row></rows></doc>"})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits
r=`$psql -tA -c'select * from www_fdw_test'`
test "$r" $'3||'
kill $spid

perl -Mojo -e'a("/" => {text => "<doc><nrows>3</nrows><rows><row><title>3</title><snippet>SNIPPeT</snippet></row><row><title>TiTuL</title><snippet>SNIPPeT2</snippet></row></rows></doc>"})->start' daemon --listen http://*:7777 &
spid=$!
sleep $waits
r=`$psql -tA -c'select * from www_fdw_test'`
test "$r" $'3||SNIPPeT\nTiTuL||SNIPPeT2'
kill $spid

