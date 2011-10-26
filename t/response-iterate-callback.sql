DROP EXTENSION IF EXISTS www_fdw CASCADE;
CREATE EXTENSION www_fdw;
CREATE SERVER www_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://localhost:7777', response_type 'json', response_iterate_callback 'test_response_iterate_callback');
CREATE USER MAPPING FOR current_user SERVER www_server_test;
CREATE FOREIGN TABLE www_fdw_test (
	title text,                                                        
	link text,
	snippet text
) SERVER www_server_test;
CREATE OR REPLACE FUNCTION test_response_iterate_callback(options WWWFdwOptions, INOUT tuple www_fdw_test) AS $$
BEGIN
	RAISE DEBUG 'options parameter: %', options;
	RAISE DEBUG 'tuple parameter: %', tuple;
	tuple.title := tuple.title || ' hello world of callbacks';
END; $$ LANGUAGE PLPGSQL;