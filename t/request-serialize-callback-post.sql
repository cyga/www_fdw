DROP EXTENSION IF EXISTS www_fdw CASCADE;
CREATE EXTENSION www_fdw;
CREATE SERVER www_server_test FOREIGN DATA WRAPPER www_fdw OPTIONS (uri 'http://localhost:7777', request_serialize_callback 'test_request_serialize_callback');
CREATE USER MAPPING FOR current_user SERVER www_server_test;
CREATE FOREIGN TABLE www_fdw_test (
	title text,                                                        
	link text,
	snippet text
) SERVER www_server_test;
CREATE TABLE IF NOT EXISTS post_data ( key text NOT NULL UNIQUE, value text );
DELETE FROM post_data;
INSERT INTO post_data VALUES ('data','title=post_title'),('content_type',null);
CREATE OR REPLACE FUNCTION test_request_serialize_callback(options WWWFdwOptions, quals text, INOUT url text, INOUT post WWWFdwPostParameters) AS $$
BEGIN
	RAISE DEBUG 'options parameter: %', options;
	RAISE DEBUG 'quals parameter: %', quals;
	RAISE DEBUG 'url parameter: %', url;
	RAISE DEBUG 'post parameter: %', post;
	post.post := true;
	post.data := (SELECT value FROM post_data WHERE key='data');
	post.content_type := (SELECT value FROM post_data WHERE key='content_type');
	RAISE DEBUG 'new post %', post;
END; $$ LANGUAGE PLPGSQL;