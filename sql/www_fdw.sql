/* contrib/www_fdw/www_fdw--1.0.sql */

-- type needed for callbacks
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
	request_user_agent					text,
	request_serialize_callback			text,
	request_serialize_type				text,
	request_serialize_human_readable	text,
	response_type						text,
	response_deserialize_callback		text,
	response_iterate_callback			text,
	ssl_cert text,
	ssl_key text,
	cainfo text,
	proxy text,
	cookie text
        username text,
        password text
);
-- type needed for returning post options in serialize_request_callback
CREATE TYPE WWWFdwPostParameters AS (
	post		boolean,
	data		text,
	content_type	text
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
