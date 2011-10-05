#!/bin/sh
perl -Mojo -e'a("/" => {json => {nrows=>2,rows=>[{title=>"t0",link=>"l0",snippet=>"s0"},{title=>"t1",link=>"l1",snippet=>"s1"}]}})->start' daemon --listen http://*:7777
