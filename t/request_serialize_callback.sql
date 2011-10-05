DROP EXTENSION IF EXISTS www_fdw CASCADE;
CREATE EXTENSION www_fdw;
CREATE SERVER www_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://nowhere.com', request_serialize_callback 'test_request_serialize_callback');
CREATE USER MAPPING FOR current_user SERVER www_server_test;
CREATE FOREIGN TABLE www_fdw_test (
	title text,                                                        
	link text,
	snippet text
) SERVER www_server_test;
CREATE OR REPLACE FUNCTION test_request_serialize_callback(options anyelement) RETURNS text AS $$
BEGIN
	RAISE NOTICE 'parameter: %', options;
	RETURN 'works';
END; $$ LANGUAGE PLPGSQL;

-- TODO: do tests here
