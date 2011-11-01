/* checking types: */
SELECT t.typname FROM pg_type t join pg_namespace ns ON t.typnamespace=ns.oid WHERE ns.nspname=current_schema() AND t.typname='wwwfdwoptions';
SELECT t.typname FROM pg_type t join pg_namespace ns ON t.typnamespace=ns.oid WHERE ns.nspname=current_schema() AND t.typname='wwwfdwpostparameters';

/* check exension: */
CREATE EXTENSION www_fdw;
CREATE SERVER www_fdw_server FOREIGN DATA WRAPPER www_fdw;
CREATE USER MAPPING FOR current_user SERVER www_fdw_server;
DROP EXTENSION www_fdw CASCADE;