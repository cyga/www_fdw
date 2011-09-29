/* contrib/www_fdw/www_fdw--1.0.sql */

-- create wrapper with validator and handler
CREATE OR REPLACE FUNCTION www_fdw_validator (text[], oid)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION www_fdw_handler ()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER www_fdw
VALIDATOR www_fdw_validator HANDLER www_fdw_handler;

CREATE SERVER www_server FOREIGN DATA WRAPPER www_fdw;
-- DEBUG
--OPTIONS (callback 'select * from g limit z 1');

CREATE USER MAPPING FOR current_user SERVER www_server;

CREATE FOREIGN TABLE www_fdw_test (
	title	text,
	link	text,
	snippet	text
) SERVER www_server;
