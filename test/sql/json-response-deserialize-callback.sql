DROP EXTENSION IF EXISTS www_fdw CASCADE;
CREATE EXTENSION www_fdw;
CREATE SERVER www_fdw_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://localhost:7777', response_type 'json', response_deserialize_callback 'test_response_deserialize_callback');
CREATE USER MAPPING FOR current_user SERVER www_fdw_server_test;
CREATE FOREIGN TABLE www_fdw_test (
	title text,                                                        
	link text,
	snippet text
) SERVER www_fdw_server_test;
/* based on http://git.postgresql.org/gitweb/?p=json-datatype.git;a=summary */
CREATE OR REPLACE FUNCTION test_response_deserialize_callback(options WWWFdwOptions, response text) RETURNS SETOF www_fdw_test AS $$
DECLARE
	row json;
	i integer;
	title text;
	link text;
	snippet text;
	r RECORD;
BEGIN
	RAISE DEBUG 'options parameter: %', options;
	RAISE DEBUG 'response parameter: %', response;

	i := 0;
	LOOP
		row := json_get(response::json, '$["rows"]['||i||']');
		EXIT WHEN row IS NULL;
		-- parser doesn't take care of quotes in "string"
		title := trim(both '"' from json_get(row, '["title"]')::text);
		link := trim(both '"' from json_get(row, '["link"]')::text);
		snippet := trim(both '"' from json_get(row, '["snippet"]')::text);
		r := ROW(title, link, snippet);
		RETURN NEXT r;
		i := i + 1;
	END LOOP;
END; $$ LANGUAGE PLPGSQL;