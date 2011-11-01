DROP EXTENSION IF EXISTS www_fdw CASCADE;
CREATE EXTENSION www_fdw;
CREATE SERVER www_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://localhost:7777', response_type 'other', response_deserialize_callback 'test_response_deserialize_callback');
CREATE USER MAPPING FOR current_user SERVER www_server_test;
CREATE FOREIGN TABLE www_fdw_test (
	title text,                                                        
	link text,
	snippet text
) SERVER www_server_test;
/* parse csv format */
CREATE OR REPLACE FUNCTION test_response_deserialize_callback(options WWWFdwOptions, response text) RETURNS SETOF www_fdw_test AS $$
DECLARE
	title text;
	link text;
	snippet text;
	rows text[];
	i integer;
	fields text[];
	r RECORD;
BEGIN
	RAISE DEBUG 'options parameter: %', options;
	RAISE DEBUG 'response parameter: %', response;
	rows := regexp_split_to_array(response, E'\\n');
	FOR i IN 1 .. array_length(rows,1) LOOP
	    fields := regexp_split_to_array(rows[i], E',');
	    title := fields[1];
	    link := fields[2];
	    snippet := fields[3];
	    r := ROW(title, link, snippet);
	    RETURN NEXT r;
	END LOOP;
END; $$ LANGUAGE PLPGSQL;