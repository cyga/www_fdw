twitter\_fdw
============

This library contains a single PostgreSQL extension, a Foreign Data
Wrapper (FDW) handler of PostgreSQL which fetches text messages from
Twitter over the Internet and returns as a table. 

Installation
------------

    $ export USE_PGXS=1
    $ make && make install
    $ psql -c "CREATE EXTENSION www_fdw" db

The CREATE EXTENSION statement creates not only FDW handlers but also
Data Wrapper, Foreign Server, User Mapping and twitter table.

Usage
-----

After installation, simply query from `twitter` table.

    db=# SELECT from_user, created_at, text FROM twitter WHERE q = '#postgresql';

The layout of `twitter` table is as below:

                   Foreign table "public.twitter"
          Column       |            Type             | Modifiers 
    -------------------+-----------------------------+-----------
     id                | bigint                      | 
     text              | text                        | 
     from_user         | text                        | 
     from_user_id      | bigint                      | 
     to_user           | text                        | 
     to_user_id        | bigint                      | 
     iso_language_code | text                        | 
     source            | text                        | 
     profile_image_url | text                        | 
     created_at        | timestamp without time zone | 
     q                 | text                        | 
    Server: www_service

The column `q` is a virtual column that is passed to API as a
query string if the column is used with `=` operator as
WHERE q = '#sometext'. You can put any text as defined in the API
parameter `q`. Note the query string is percent-encoded by the module.
The other columns are mapped to the corresponding property name of
each tweet item in the API result. For more detail on these values,
see the API document.

Depencency
----------

This module depends on

  * [libcurl](http://curl.haxx.se/libcurl/)
  * [libjson](http://projects.snarc.org/libjson/)
  * [Twitter API](http://apiwiki.twitter.com/w/page/22554679/Twitter-API-Documentation)

The source of libjson is included this module package and linked as a
static library, wheares libcurl is assumed installed in the system.
You may need additional development package, as `libcurl-dev` in yum.
Consult your system and repository owner for more detail.

Author
------

Hitoshi Harada <umi.tanuki@gmail.com>

Copyright and License
---------------------

Copyright (c) Hitoshi Harada

This module is free software; you can redistribute it and/or modify it under
the [PostgreSQL License](http://www.opensource.org/licenses/postgresql).

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement is
hereby granted, provided that the above copyright notice and this paragraph
and the following two paragraphs appear in all copies.

In no event shall Hitoshi Harada be liable to any party for direct,
indirect, special, incidental, or consequential damages, including
lost profits, arising out of the use of this software and its documentation,
even if Hitoshi Harada has been advised of the possibility of such damage.

Hitoshi Harada specifically disclaims any warranties,
including, but not limited to, the implied warranties of merchantability and
fitness for a particular purpose. The software provided hereunder is on an "as
is" basis, and Hitoshi Harada has no obligations to provide maintenance,
support, updates, enhancements, or modifications.
