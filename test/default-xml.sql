DROP EXTENSION IF EXISTS www_fdw CASCADE;
CREATE EXTENSION www_fdw;
CREATE SERVER www_fdw_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://localhost:7777', response_type 'xml');
CREATE USER MAPPING FOR current_user SERVER www_fdw_server_test;
CREATE FOREIGN TABLE www_fdw_test (
	id integer,                                                        
    b boolean,
    d date,
    t time,
    ts timestamp,
    title text,                                                        
    link text,
    snippet text
) SERVER www_fdw_server_test;
