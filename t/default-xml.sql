DROP EXTENSION IF EXISTS www_fdw CASCADE;
CREATE EXTENSION www_fdw;
CREATE SERVER www_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://localhost:7777', response_type 'xml');
CREATE USER MAPPING FOR current_user SERVER www_server_test;
CREATE FOREIGN TABLE www_fdw_test (
	title text,                                                        
	link text,
	snippet text
) SERVER www_server_test;

-- TODO: do tests here
