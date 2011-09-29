CREATE OR REPLACE FUNCTION f(e anyelement, idx bigint) RETURNS g AS $$
DECLARE
	r	g;
BEGIN
	BEGIN
		IF idx > array_length(e.items, 1) THEN
			RETURN	NULL;
		END IF;
		r.title	:= e.items[idx].title;
		r.link	:= e.items[idx].link;
		r.snippet	:= e.items[idx].snippet;
	EXCEPTION
		WHEN OTHERS THEN
			RAISE EXCEPTION 'error occured, sqlstate: %', SQLSTATE;
	END;
	RETURN	r;
END; $$ LANGUAGE PLPGSQL;

/*
select f( ROW( ARRAY[ ('TITLE','LINK','SNIPPET')::t_item, ('TITLE2','LINK2','SNIPPET2')::t_item ] )::t_answer, 1);
select f( ROW( ARRAY[ ('TITLE','LINK','SNIPPET')::t_item, ('TITLE2','LINK2','SNIPPET2')::t_item ] )::t_answer, 2);
select f( ROW( ARRAY[ ('TITLE','LINK','SNIPPET')::t_item, ('TITLE2','LINK2','SNIPPET2')::t_item ] )::t_answer, 3);
select f( 1, 1);
*/
