www_fdw
=======

This library contains a PostgreSQL extension,
a Foreign Data Wrapper (FDW) handler of PostgreSQL
which provides easy way for interacting with different web-services.

Installation
============

    $ export USE_PGXS=1
    $ make && make install
    $ psql -c "CREATE EXTENSION www_fdw" db

After that you need to create server for extension.
The simpliest example here is:

    $ cat test/default-json.sql
    DROP EXTENSION IF EXISTS www_fdw CASCADE;
    CREATE EXTENSION www_fdw;
    CREATE SERVER www_fdw_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://localhost:7777');
    CREATE USER MAPPING FOR current_user SERVER www_fdw_server_test;
    CREATE FOREIGN TABLE www_fdw_test (
        title text,
        link text,
        snippet text
    ) SERVER www_fdw_server_test;


For more examples check [github wiki](https://github.com/cyga/www_fdw/wiki/Examples).

For proper usage of deserialize callbacks you need to use 9.2 version (version with the patch [patches/foreign-table-as-argument-to-plpgsql_parse_cwordtype.patch](https://github.com/cyga/www_fdw/tree/master/patches/foreign-table-as-argument-to-plpgsql_parse_cwordtype.patch)). 

Documentation
=============

Up-to-date documenation can be found at [github](https://github.com/cyga/www_fdw/wiki/WWW-FDW-postgres-extension).

PostgreSQL server installation
==============================

If your response isn't of xml type or you don't plan to use xml parsing in response_deserialize_callback - don't bother about it.
Otherwise, in order to work with xml type (used in response_deserialize_callback) your installation has to support xml type. Usually it means building PostgreSQL with --with-libxml option.
If you plan to use response_deserialize_callback for xml but with own parsing mechanism - your callback will be passed with text parameter.

Depencencies
============

This module depends on

  * [libcurl](http://curl.haxx.se/libcurl/)
  * [libjson](http://projects.snarc.org/libjson/)

The source of libjson is included this module package and linked as a
static library, wheares libcurl is assumed installed in the system.
You may need additional development package, as libcurl-dev.
Consult your system and repository owner for more details.

Development
===========

* [source](http://github.com/cyga/www_fdw);
* [documentation](https://github.com/cyga/www_fdw/wiki/WWW-FDW-postgres-extension);
* [test/README.md](https://github.com/cyga/www_fdw/blob/master/test/README.md) - documentation on test system.

Author(s)
=========

Alex Sudakov <cygakob@gmail.com>

Copyright and License
=====================

This module is free software; you can redistribute it and/or modify it under
the [PostgreSQL License](http://www.opensource.org/licenses/postgresql).

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement is
hereby granted, provided that the above copyright notice and this paragraph
and the following two paragraphs appear in all copies.

In no event shall author(s) be liable to any party for direct,
indirect, special, incidental, or consequential damages, including
lost profits, arising out of the use of this software and its documentation,
even if author(s) has been advised of the possibility of such damage.

Author(s) specifically disclaims any warranties,
including, but not limited to, the implied warranties of merchantability and
fitness for a particular purpose. The software provided hereunder is on an "as is" basis, and author(s) has no obligations to provide maintenance,
support, updates, enhancements, or modifications.
