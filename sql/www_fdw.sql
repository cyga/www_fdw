-- TODO
CREATE EXTENSION www_fdw;

SELECT count(*) FROM twitter;

SELECT count(*) FROM twitter WHERE q = '#postgresql';

CREATE TEMP TABLE twtest (id int, from_user text);
INSERT INTO twtest(from_user)
	SELECT from_user FROM twitter WHERE q = '#postgres' LIMIT 1;
SELECT true FROM twtest INNER JOIN
	twitter USING(from_user) WHERE q = '#postgres';

