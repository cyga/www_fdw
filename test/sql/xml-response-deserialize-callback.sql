DROP EXTENSION IF EXISTS www_fdw CASCADE;
CREATE EXTENSION www_fdw;
CREATE SERVER www_fdw_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://localhost:7777', response_type 'xml', response_deserialize_callback 'test_response_deserialize_callback');
CREATE USER MAPPING FOR current_user SERVER www_fdw_server_test;
CREATE FOREIGN TABLE www_fdw_test (
	title text,                                                        
	link text,
	snippet text
) SERVER www_fdw_server_test;
CREATE OR REPLACE FUNCTION test_response_deserialize_callback(options WWWFdwOptions, response xml) RETURNS SETOF www_fdw_test AS $$
DECLARE
	rows RECORD;
	title text;
	link text;
	snippet text;
	r RECORD;
BEGIN
	RAISE DEBUG 'options parameter: %', options;
	RAISE DEBUG 'response parameter: %', response;
	FOR rows IN SELECT unnest(xpath('/doc/rows/row', response)) LOOP
	    title := (xpath('/row/title/text()', rows.unnest))[1];
	    link := (xpath('/row/link/text()', rows.unnest))[1];
	    snippet := (xpath('/row/snippet/text()', rows.unnest))[1];
	    r := ROW(title, link, snippet);
	    RETURN NEXT r;
	END LOOP;
END; $$ LANGUAGE PLPGSQL;