Test types
==========

There are 3 types of tests currently:

  * standard PGXS regression tests;
  * different test cases;
  * development json parser tests.

Standard postgres PGXS regression tests
---------------------------------------

Location:

  * `test/sql/` - input files
  * `test/expected/` - output files

Each test has same filename base, but differs in extension:

  * .sql - for input;
  * .out - for output.

Currently there is only one simple test. It do following:

  * check types (used in extension) creation;
  * check extension/server/user_mapping creation.

It's executed with `make testinstall` (after `make && make install`). Current user has to have access for extension creation.

Different test cases
--------------------

Location:

`test/test-*.sh`

Dependencies:

  * bash
  * perl
  * Mojo web framework. Can be installed with:
    ** `sudo sh -c "curl -L cpanmin.us | perl - Mojolicious"`
    ** For more details read at:
    ** http://mojolicio.us/

Current user has to have access for extension creation.

Each test file do following things:
  * starts up simple server;
  * initializes extension with current case properties;
  * runs number of queries and checks output.

`test/test-all.sh` - run all available tests.
It accepts options. Check them with -h or --help CLI option.
It outputs results for each test file it runs and stats for all executed files either.

There are tests those need postgres to be compiled with xml feature support (--libxml).
Also there are tests those need following json data type support:
http://git.postgresql.org/gitweb/?p=json-datatype.git;a=summary
All these dependent tests are skipped by default in `test/test-all.sh`. Look for CLI option to force them.

Development json parser tests
-----------------------------

Location:

`test/json_parser`

Can be executed with "json_parser.sh" script. It compiles json parser executable, runs it against all given input files and checks against corresponding output files. Proper output looks like:

    $ ./json_parser.sh 
    cc -O2 -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -g -I../..   -c -o json_parser.o json_parser.c
    json_parser.c: In function ‘main’:
    json_parser.c:15:2: warning: suggest parentheses around assignment used as truth value [-Wparentheses]
    cc   json_parser.o ../../libjson-0.8/json.o ../../src/json_parser.o   -o json_parser
    ===== TEST: json_parser.in.00 json_parser.out.00 =====
    ===== TEST: json_parser.in.01 json_parser.out.01 =====
    ===== TEST: json_parser.in.02 json_parser.out.02 =====
    ===== TEST: json_parser.in.03 json_parser.out.03 =====
    ===== TEST: json_parser.in.04 json_parser.out.04 =====
    ===== TEST: json_parser.in.05 json_parser.out.05 =====
    ===== TEST: json_parser.in.06 json_parser.out.06 =====
    ===== TEST: json_parser.in.07 json_parser.out.07 =====
    ===== TEST: json_parser.in.08 json_parser.out.08 =====
    ===== TEST: json_parser.in.09 json_parser.out.09 =====
    rm -f json_parser json_parser.o ../../libjson-0.8/json.o ../../src/json_parser.o
