/* contrib/www_fdw/www_fdw--1.0.sql */

-- type needed for callback
CREATE TYPE WWWFdwOptions AS (
	uri									text,
	uri_select							text,
	uri_insert							text,
	uri_delete							text,
	uri_update							text,
	uri_callback						text,
	method_select						text,
	method_insert						text,
	method_delete						text,
	method_update						text,
	request_serialize_callback			text,
	response_type						text,
	response_deserialize_callback		text,
	response_iterate_callback			text
);

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
--TODO: leave good examples here
-- DEBUG
--OPTIONS (callback 'select * from g limit z 1');

CREATE USER MAPPING FOR current_user SERVER www_server;

/*
CREATE FOREIGN TABLE www_fdw_test (
	title	text,
	link	text,
	snippet	text
) SERVER www_server;
*/
