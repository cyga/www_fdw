#include "postgres.h"

#include "access/reloptions.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_foreign_server.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/cost.h"
#include "parser/parsetree.h"
#include "storage/fd.h"
#include "utils/rel.h"
#include "utils/builtins.h"
#include "executor/spi.h"
#include "utils/fmgroids.h"
#include "catalog/pg_type.h"

#include "curl/curl.h"
#include "libjson-0.8/json.h"
#include "json_parser.h"

PG_MODULE_MAGIC;

/* wrapper for init code, not to duplicate it */
static void spi_init(void);

struct WWW_fdw_option
{
	const char	*optname;
	Oid		optcontext;	/* Oid of catalog in which option may appear */
};

/*
 * Valid options for this extension.
 *
 */
static struct WWW_fdw_option valid_options[] =
{
	{ "uri",		ForeignServerRelationId },
	{ "uri_select",	ForeignServerRelationId },
	{ "uri_insert",	ForeignServerRelationId },
	{ "uri_delete",	ForeignServerRelationId },
	{ "uri_update",	ForeignServerRelationId },
	{ "uri_callback",	ForeignServerRelationId },

	{ "method_select",	ForeignServerRelationId },
	{ "method_insert",	ForeignServerRelationId },
	{ "method_delete",	ForeignServerRelationId },
	{ "method_update",	ForeignServerRelationId },

	{ "request_serialize_callback",	ForeignServerRelationId },

	{ "response_type",	ForeignServerRelationId },
	{ "response_deserialize_callback",	ForeignServerRelationId },
	{ "response_iterate_callback",	ForeignServerRelationId },

	/* Sentinel */
	{ NULL,			InvalidOid }
};

typedef struct	WWW_fdw_options
{
	char*	uri;
	char*	uri_select;
	char*	uri_insert;
	char*	uri_delete;
	char*	uri_update;
	char*	uri_callback;
	char*	method_select;
	char*	method_insert;
	char*	method_delete;
	char*	method_update;
	char*	request_serialize_callback;
	char*	response_type;
	char*	response_deserialize_callback;
	char*	response_iterate_callback;
} WWW_fdw_options;

Oid	www_fdw_options_oid	= 0;
/* DEBUG check if all needed */
TupleDesc	www_fdw_options_tuple_desc;
AttInMetadata*	www_fdw_options_aim;
/* used to initialize OID above and return, basically wrapper for init code */
static Oid get_www_fdw_options_oid(void);

typedef struct Reply
{
	void		   *root;
	void		   *result;
	AttInMetadata  *attinmeta;
	int				row_index;
	WWW_fdw_options	*options;
} Reply;

static bool www_is_valid_option(const char *option, Oid context);
static void get_options(Oid foreigntableid, WWW_fdw_options *opts);

/*
 * SQL functions
 */
extern Datum www_fdw_validator(PG_FUNCTION_ARGS);
extern Datum www_fdw_handler(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(www_fdw_handler);
PG_FUNCTION_INFO_V1(www_fdw_validator);

/*
 * FDW callback routines
 */
static FdwPlan *www_plan(Oid foreigntableid,
						PlannerInfo *root,
						RelOptInfo *baserel);
static void www_explain(ForeignScanState *node, ExplainState *es);
static void www_begin(ForeignScanState *node, int eflags);
static TupleTableSlot *www_iterate(ForeignScanState *node);
static void www_rescan(ForeignScanState *node);
static void www_end(ForeignScanState *node);

static size_t json_write_data_to_parser(void *buffer, size_t size, size_t nmemb, void *userp);
static size_t json_write_data_to_buffer(void *buffer, size_t size, size_t nmemb, void *userp);

static bool
parse_parameter(char* name, char** var, DefElem* param)
{
	if( 0 == strcmp(param->defname, name) )
	{
		if (*var)
			ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("conflicting or redundant options: %s (%s)", name, defGetString(param))
				));

		*var = defGetString(param);
		return	true;
	}
	return	false;
}

Datum
www_fdw_validator(PG_FUNCTION_ARGS)
{
	List		*options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid			catalog = PG_GETARG_OID(1);
	ListCell	*cell;
	char		*uri	= NULL;
	char		*uri_select	= NULL;
	char		*uri_insert	= NULL;
	char		*uri_delete	= NULL;
	char		*uri_update	= NULL;
	char		*uri_callback	= NULL;
	char		*method_select	= NULL;
	char		*method_insert	= NULL;
	char		*method_delete	= NULL;
	char		*method_update	= NULL;
	char		*request_serialize_callback	= NULL;
	char		*response_type	= NULL;
	char		*response_deserialize_callback	= NULL;
	char		*response_iterate_callback	= NULL;

	/*
	 * Check that only options supported by this extension
	 * and allowed for the current object type, are given.
	 */
	foreach(cell, options_list)
	{
		DefElem	   *def = (DefElem *) lfirst(cell);

		if (!www_is_valid_option(def->defname, catalog))
		{
			struct WWW_fdw_option *opt;
			StringInfoData buf;

			/*
			 * Unknown option specified, complain about it. Provide a hint
			 * with list of valid options for the object.
			 */
			initStringInfo(&buf);
			for (opt = valid_options; opt->optname; opt++)
			{
				if (catalog == opt->optcontext)
					appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "",
							 opt->optname);
			}

			ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
				errmsg("invalid option \"%s\"", def->defname),
				errhint("Valid options in this context are: %s", buf.len ? buf.data : "<none>")
				));
		}

		if(parse_parameter("uri", &uri, def)) continue;
		if(parse_parameter("uri_select", &uri_select, def)) continue;
		if(parse_parameter("uri_insert", &uri_insert, def)) continue;
		if(parse_parameter("uri_delete", &uri_delete, def)) continue;
		if(parse_parameter("uri_update", &uri_update, def)) continue;
		if(parse_parameter("uri_callback", &uri_callback, def)) continue;
		if(parse_parameter("method_select", &method_select, def)) continue;
		if(parse_parameter("method_insert", &method_insert, def)) continue;
		if(parse_parameter("method_delete", &method_delete, def)) continue;
		if(parse_parameter("method_update", &method_update, def)) continue;
		if(parse_parameter("request_serialize_callback", &request_serialize_callback, def)) continue;
		if(parse_parameter("response_type", &response_type, def)) {
			if(
				0 != strcmp(response_type, "json")
				&&
				0 != strcmp(response_type, "xml")
				&&
				0 != strcmp(response_type, "other")
			)
			{
				ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("invalid value for response_type: %s", response_type)
					));
			}
			continue;
		};
		if(parse_parameter("response_deserialize_callback", &response_deserialize_callback, def)) continue;
		if(parse_parameter("response_iterate_callback", &response_iterate_callback, def)) continue;
	}

	PG_RETURN_VOID();
}

/*
 * Check if the provided option is one of the valid options.
 * context is the Oid of the catalog holding the object the option is for.
 */
static bool
www_is_valid_option(const char *option, Oid context)
{
	struct WWW_fdw_option *opt;

	for (opt = valid_options; opt->optname; opt++)
	{
		if (context == opt->optcontext && strcmp(opt->optname, option) == 0)
			return true;
	}
	return false;
}

Datum
www_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdwroutine = makeNode(FdwRoutine);

	/*
	 * Anything except Begin/Iterate is blank so far,
	 * but FDW interface assumes all valid function pointers.
	 */
	fdwroutine->PlanForeignScan = www_plan;
	fdwroutine->ExplainForeignScan = www_explain;
	fdwroutine->BeginForeignScan = www_begin;
	fdwroutine->IterateForeignScan = www_iterate;
	fdwroutine->ReScanForeignScan = www_rescan;
	fdwroutine->EndForeignScan = www_end;

	PG_RETURN_POINTER(fdwroutine);
}

static char *
percent_encode(unsigned char *s, int srclen)
{
	unsigned char  *end;
	StringInfoData  buf;
	int				len;

	initStringInfo(&buf);

	if (srclen < 0)
		srclen = strlen((char *) s);

	end = s + srclen;

	for (; s < end; s += len)
	{
		unsigned char  *utf;
		int				ulen;

		len = pg_mblen((const char *) s);

		if (len == 1)
		{
			if (('0' <= s[0] && s[0] <= '9') ||
				('A' <= s[0] && s[0] <= 'Z') ||
				('a' <= s[0] && s[0] <= 'z') ||
				(s[0] == '-') || (s[0] == '.') ||
				(s[0] == '_') || (s[0] == '~'))
			{
				appendStringInfoChar(&buf, s[0]);
				continue;
			}
		}

		utf = pg_do_encoding_conversion(s, len, GetDatabaseEncoding(), PG_UTF8);
		ulen = pg_encoding_mblen(PG_UTF8, (const char *) utf);
		while(ulen--)
		{
			appendStringInfo(&buf, "%%%2X", *utf);
			utf++;
		}
	}

	return buf.data;
}

/*
 * www_param
 *  check and create column=value string for column=value criteria in query
 *  raise error for operators differ from '='
 *  raise error for operators with not 2 arguments
 */
static char *
www_param(Node *node, TupleDesc tupdesc)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, OpExpr))
	{
		OpExpr	   *op = (OpExpr *) node;
		Node	   *left, *right, *tmp;
		Index		varattno;
		char	   *key, *val;
		StringInfoData	buf;

		if (list_length(op->args) != 2)
			ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("Operators with not 2 arguments aren't supported")));

		if (op->opfuncid != F_TEXTEQ)
			ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("Invalid operator, only '=' is supported")));

		left = list_nth(op->args, 0);
		right = list_nth(op->args, 1);
		if(!(
			(IsA(left, Var) && IsA(right, Const))
			||
			(IsA(left, Const) && IsA(right, Var))
		))
			ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("One operand supposed to be column another constant")));
		if(IsA(left, Const) && IsA(right, Var))
		{
			tmp		= left;
			left	= right;
			right	= tmp;
		}

		varattno = ((Var *) left)->varattno;
		Assert(0 < varattno && varattno <= tupdesc->natts);
		key = NameStr(tupdesc->attrs[varattno - 1]->attname);

		initStringInfo(&buf);
		val = TextDatumGetCString(((Const *) right)->constvalue);
		appendStringInfo(&buf, "%s=%s", percent_encode((unsigned char *) key, -1),
			percent_encode((unsigned char *) val, -1));
		return buf.data;
	}
	else
	{
		ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("Only simple WHERE statements are covered: column=const [AND column2=const2 ...]")));
	}

	ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("Strange error in parameter parser")));

	return NULL;
}

/*
 * www_plan
 *   Create a FdwPlan, which is empty for now.
 */
static FdwPlan *
www_plan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel)
{
	FdwPlan	   *fdwplan;

	fdwplan = makeNode(FdwPlan);
	fdwplan->fdw_private = NIL;

	return fdwplan;
}

/*
 * www_explain
 *   Produce extra output for EXPLAIN
 */
static void
www_explain(ForeignScanState *node, ExplainState *es)
{
	ExplainPropertyText("WWW API", "Request", es);
}

static
char*
describe_spi_code(int code)
{
	switch(code)
	{
		case SPI_ERROR_CONNECT:		return "ERROR CONNECT";
		case SPI_ERROR_COPY:			return "ERROR COPY";
		case SPI_ERROR_OPUNKNOWN:	return "ERROR OPUNKNOWN";
		case SPI_ERROR_UNCONNECTED:	return "ERROR UNCONNECTED";
		case SPI_ERROR_CURSOR:		return "ERROR CURSOR";
		case SPI_ERROR_ARGUMENT:		return "ERROR ARGUMENT";
		case SPI_ERROR_PARAM:		return "ERROR PARAM";
		case SPI_ERROR_TRANSACTION:	return "ERROR TRANSACTION";
		case SPI_ERROR_NOATTRIBUTE:	return "ERROR NOATTRIBUTE";
		case SPI_ERROR_NOOUTFUNC:	return "ERROR NOOUTFUNC";
		case SPI_ERROR_TYPUNKNOWN:	return "ERROR TYPUNKNOWN";

		case SPI_OK_CONNECT:			return "OK CONNECT";
		case SPI_OK_FINISH:			return "OK FINISH";
		case SPI_OK_FETCH:			return "OK FETCH";
		case SPI_OK_UTILITY:			return "OK UTILITY";
		case SPI_OK_SELECT:			return "OK SELECT";
		case SPI_OK_SELINTO:			return "OK SELINTO";
		case SPI_OK_INSERT:			return "OK INSERT";
		case SPI_OK_DELETE:			return "OK DELETE";
		case SPI_OK_UPDATE:			return "OK UPDATE";
		case SPI_OK_CURSOR:			return "OK CURSOR";
		case SPI_OK_INSERT_RETURNING: return "OK INSERT_RETURNING";
		case SPI_OK_DELETE_RETURNING: return "OK DELETE_RETURNING";
		case SPI_OK_UPDATE_RETURNING: return "OK UPDATE_RETURNING";
		case SPI_OK_REWRITTEN:		return "OK REWRITTEN";

		default:
			return	"undefined code";
	}

	return	"undefined code";
}

/*
 * serialize_request_parametersWithCallback
 *  serialize request parameters using specified callback
 *  TODO:
 *		1. how to pass quals? create special type for it?
 *		2. out parameters for: request string and others
 */
static void
serialize_request_with_callback(WWW_fdw_options *opts, ForeignScanState *node, StringInfoData *url)
{
	int	res;
	StringInfoData	cmd;
	Datum	values[1];
	Oid		argtypes[1]	= { get_www_fdw_options_oid() };
	char*	options[]	= {
		opts->uri,
		opts->uri_select,
		opts->uri_insert,
		opts->uri_delete,
		opts->uri_update,
		opts->uri_callback,

		opts->method_select,
		opts->method_insert,
		opts->method_delete,
		opts->method_update,

		opts->request_serialize_callback,

		opts->response_type,
		opts->response_deserialize_callback,
		opts->response_iterate_callback
	};

	initStringInfo(&cmd);
	// TODO: add 2nd parameter - qual converted correspondingly
	appendStringInfo(&cmd, "SELECT %s($1)", opts->request_serialize_callback);

	/* prepare options for the call */
	www_fdw_options_tuple_desc	= TypeGetTupleDesc(www_fdw_options_oid, NIL);
	www_fdw_options_aim			= TupleDescGetAttInMetadata(www_fdw_options_tuple_desc);
	values[0]	= HeapTupleGetDatum( BuildTupleFromCStrings(www_fdw_options_aim, options) );

	//res	= SPI_execute(opts->request_serialize_callback, true, 0);
	res	= SPI_execute_with_args(cmd.data, 1, argtypes, values, NULL, true, 0);
	if(0 > res)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Can't spi execute: %i (%s)", res, describe_spi_code(res))
			)
		);
	}
	else
	{
		// TODO: process results
		//SPI_tuptable->vals->
	}
}

/*
 * serialize_request_parameters
 *  serialize column=value sql conditions into column=value get parameters
 *  column & value are url encoded
 */
static void
serialize_request_parameters(ForeignScanState* node, StringInfoData *url)
{
	if (node->ss.ps.plan->qual)
	{
		bool		param_first = true;
		ListCell   *lc;
		List	   *quals = list_copy(node->ss.ps.qual);

		foreach (lc, quals)
		{
			ExprState	   *state = lfirst(lc);

			char *param = www_param((Node *) state->expr,
							node->ss.ss_currentRelation->rd_att);

			if (param)
			{
				if (param_first)
				{
					appendStringInfoChar(url, '?');
					param_first	= false;
				}
				else
					appendStringInfoChar(url, '&');
				appendStringInfoString(url, param);

				/* take it from original qual */
				node->ss.ps.qual = list_delete(node->ss.ps.qual, (void *) state);
			}
			else
				ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("Unknown qual")));
		}
	}
}

/*
 * json_check_result_array
 *	currently checks all elements to be objects
*/
static
bool
json_check_result_array(JSONNode* root)
{
	int	i;

	for( i=0; i<root->length; i++ )
		if(JSON_OBJECT_BEGIN != root->val.val_array[i].type)
			return	false;

	return	true;
}

/*
 * json_get_result_array_in_tree
 *	find the result array in json response structure
 *	check json_check_result_array for valid array description
*/
static
JSONNode*
json_get_result_array_in_tree(JSONNode* root)
{
	JSONNode*	suspect	= NULL;
	List		*curr	= list_make1(root),
				*next	= NULL;
	ListCell*	cell;
	int			i;

	while(curr) {
		foreach(cell, curr)
		{
			suspect	= (JSONNode*)(cell->data.ptr_value);
			switch (suspect->type)
			{
				case JSON_OBJECT_BEGIN:
					for( i=0; i<suspect->length; i++ )
						next	= lappend( next, (void*)(&(suspect->val.val_object[i])) );
					break;
				case JSON_ARRAY_BEGIN:
					/* check array to be valid answer */
					if(json_check_result_array(suspect))
					{
						list_free(curr);
						list_free(next);
						return	suspect;
					}
					for( i=0; i<suspect->length; i++ )
						next	= lappend( next, (void*)(&(suspect->val.val_array[i])) );
					break;
				case JSON_STRING:
				case JSON_INT:
				case JSON_FLOAT:
				case JSON_NULL:
				case JSON_TRUE:
				case JSON_FALSE:
					break;
				/* for removing warning */
				case JSON_NONE:
				case JSON_KEY:
				case JSON_ARRAY_END:
				case JSON_OBJECT_END:
					break;
			}
		}

		list_free(curr);
		curr	= next;
		next	= NULL;
	}

	return	NULL;
}

/*
 * www_begin
 *   Query search API and setup result
 */
static void
www_begin(ForeignScanState *node, int eflags)
{
	WWW_fdw_options	*opts;
	CURL			*curl;
	char			curl_error_buffer[CURL_ERROR_SIZE+1]	= {0};
	CURLcode		ret;
	StringInfoData	url;
	json_parser		json_parserr;
	json_parser_dom json_dom;
	StringInfoData	json_buffer;
	Relation		rel;
	AttInMetadata	*attinmeta;
	Reply			*reply;

	/*
	 * Do nothing in EXPLAIN
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

	spi_init();

	opts	= (WWW_fdw_options*)palloc(sizeof(WWW_fdw_options));
	get_options( RelationGetRelid(node->ss.ss_currentRelation), opts );

	initStringInfo(&url);
	appendStringInfo(&url, "%s%s", opts->uri, opts->uri_select);

	if(opts->request_serialize_callback)
	{
		/* call specified callback for forming request */
		/* TODO */
		/* serialize_request_with_callback(opts, node, &url, &struct4otherParams); */
		serialize_request_with_callback(opts, node, &url);
	}
	else
	{
		serialize_request_parameters(node, &url);
	}

	elog(DEBUG1, "Url for request: '%s'", url.data);

	/* TODO interacting with the server */
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.data);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_buffer);

	/* prepare parsers */
	if( 0 == strcmp(opts->response_type, "json") )
	{
		if(opts->response_deserialize_callback)
		{
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_write_data_to_buffer);
			initStringInfo(&json_buffer);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_buffer);
		}
		else
		{
			json_parser_init2(&json_parserr, &json_dom);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, json_write_data_to_parser);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_parserr);
		}
	}
	else if( 0 == strcmp(opts->response_type, "xml") )
	{
		/* TODO */
	}
	else if( 0 == strcmp(opts->response_type, "other") )
	{
		/* TODO */
	}
	else
	{
		ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("Invalid value for response_type option: %s", opts->response_type)
			));
	}

	ret = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if(ret) {
		ereport(ERROR,
			(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
			errmsg("Can't get a response from server: %s", curl_error_buffer)
			));
	}

	/* process parsed results */
	if( 0 == strcmp(opts->response_type, "json") )
	{
		if(opts->response_deserialize_callback)
		{
			/* TODO
			 *	call callback with accumulated json response
			 *	save returned set of records,
			 *	for further iterating them in iterate routine
			*/
		}
		else
		{
			/* get result in parsed response tree
			 * and save it for further processing in results iterations
			*/

			JSONNode	*root	= json_result_tree(&json_parserr),
						*result;

			if(!root)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
					errmsg("Can't parse server's json response, parser error code: %i", ret)
					));

			elog(DEBUG1, "JSON response was parsed");

			result	= json_get_result_array_in_tree(root);
			if(!result)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
					errmsg("Can't find result in parsed server's json response")
					));

			elog(DEBUG1, "Result array was found in json response");

			rel = node->ss.ss_currentRelation;
			attinmeta = TupleDescGetAttInMetadata(rel->rd_att);

			/* save result */
			reply = (Reply*)palloc(sizeof(Reply));
			reply->root = root;
			reply->result = result;
			reply->attinmeta = attinmeta;
			reply->row_index = 0;
			reply->options = opts;
			node->fdw_state = (void *) reply;

			json_parser_free(&json_parserr);

			/* TODO: where to free up json parsed tree?
			 * rewrite it using palloc?
			*/
			/*json_free_tree(root);*/
		}
	}
	else if( 0 == strcmp(opts->response_type, "xml") )
	{
		/* TODO */
	}
	else if( 0 == strcmp(opts->response_type, "other") )
	{
		/* TODO */
	}
}

static
char*
json2string(JSONNode* json)
{
	StringInfoData buf;

	switch (json->type)
	{
		case JSON_OBJECT_BEGIN:
		case JSON_ARRAY_BEGIN:
			return	NULL;
			break;
		case JSON_STRING:
			return	json->val.val_string;
		case JSON_INT:
			initStringInfo(&buf);
			appendStringInfo(&buf, "%i", json->val.val_int);
			return	buf.data;
		case JSON_FLOAT:
			initStringInfo(&buf);
			appendStringInfo(&buf, "%f", json->val.val_float);
			return	buf.data;
		case JSON_NULL:
			return	"null";
		case JSON_TRUE:
			return	"true";
		case JSON_FALSE:
			return	"false";
		/* for removing warning */
		case JSON_NONE:
		case JSON_KEY:
		case JSON_ARRAY_END:
		case JSON_OBJECT_END:
			break;
	}
	return	NULL;
}

/*
 * www_iterate
 *   return row per each call
 */
static TupleTableSlot *
www_iterate(ForeignScanState *node)
{
	TupleTableSlot	*slot	= node->ss.ss_ScanTupleSlot;
	Reply			*reply	= (Reply*)node->fdw_state;
	HeapTuple		tuple;
	Relation		rel = node->ss.ss_currentRelation;
	int				i,j, natts;
	char			**values;
	MemoryContext	oldcontext;
	JSONNode		obj;

	if( 0 == strcmp(reply->options->response_type, "json") )
	{
		JSONNode	*root	= (JSONNode*)reply->root,
					*result	= (JSONNode*)reply->result;

		/* no results or results finished */
		if(!root || !result || !(reply->row_index < result->length))
		{
			ExecClearTuple(slot);
			return slot;
		}

		obj = result->val.val_array[reply->row_index++];
		natts = rel->rd_att->natts;
		values = (char **) palloc(sizeof(char *) * natts);
		for (i = 0; i < natts; i++)
		{
			Name	attname = &rel->rd_att->attrs[i]->attname;

			for( j=0; j<obj.length; j++ )
				if(0 == namestrcmp(attname, obj.val.val_object[j].key))
					break;

			if(j < obj.length)
			{
				values[i]	= json2string(&(obj.val.val_object[j]));
			}
			else
			{
				values[i]	= NULL;
			}
		}
		oldcontext = MemoryContextSwitchTo(node->ss.ps.ps_ExprContext->ecxt_per_query_memory);
		tuple = BuildTupleFromCStrings(reply->attinmeta, values);
		MemoryContextSwitchTo(oldcontext);
		ExecStoreTuple(tuple, slot, InvalidBuffer, true);

		return slot;
	}
	else if( 0 == strcmp(reply->options->response_type, "xml") )
	{
		/* TODO */
	}
	else if( 0 == strcmp(reply->options->response_type, "other") )
	{
		/* TODO */
	}
	else
	{
		ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("Invalid value for response_type option: %s", reply->options->response_type)
			));
	}

	ereport(ERROR,
		(errcode(ERRCODE_SYNTAX_ERROR),
		errmsg("Invalid code branch launched")
		));

	/* remove warning */
	ExecClearTuple(slot);
	return slot;
}

/*
 * www_rescan
 */
static void
www_rescan(ForeignScanState *node)
{
	Reply	   *reply = (Reply *) node->fdw_state;
	reply->row_index	= 0;
}

static void
www_end(ForeignScanState *node)
{
	/* intentionally left blank */
}

/*
 * json_write_data_to_parser
 *	parse json chunk by chunk
*/
static size_t
json_write_data_to_parser(void *buffer, size_t size, size_t nmemb, void *userp)
{
	int			segsize = size * nmemb;
	json_parser *parser = (json_parser *) userp;
	int			ret;

	ret = json_parser_string(parser, buffer, segsize, NULL);
	if (ret)
		ereport(ERROR,
			(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
			errmsg("Can't parser server's json response, parser error code: %i", ret)
			));

	return segsize;
}

/*
 * json_write_data_to_buffer
 *	accumulate json response in buffer for further processing
*/
static size_t
json_write_data_to_buffer(void *buffer, size_t size, size_t nmemb, void *userp)
{
	appendBinaryStringInfo((StringInfoData*)userp, buffer, size*nmemb);

	return size*nmemb;
}

/*
 * Fetch the options for a mysql_fdw foreign table.
 */
static void
get_options(Oid foreigntableid, WWW_fdw_options *opts)
{
	ForeignTable	*f_table;
	ForeignServer	*f_server;
	UserMapping	*f_mapping;
	List		*options;
	ListCell	*lc;

	/*
	 * Extract options from FDW objects.
	 */
	f_table = GetForeignTable(foreigntableid);
	f_server = GetForeignServer(f_table->serverid);
	f_mapping = GetUserMapping(GetUserId(), f_table->serverid);

	options = NIL;
	options = list_concat(options, f_table->options);
	options = list_concat(options, f_server->options);
	options = list_concat(options, f_mapping->options);

	/* init options */
	opts->uri	= NULL;
	opts->uri_select	= NULL;
	opts->uri_insert	= NULL;
	opts->uri_delete	= NULL;
	opts->uri_update	= NULL;
	opts->uri_callback	= NULL;
	opts->method_select	= NULL;
	opts->method_insert	= NULL;
	opts->method_delete	= NULL;
	opts->method_update	= NULL;
	opts->request_serialize_callback	= NULL;
	opts->response_type	= NULL;
	opts->response_deserialize_callback	= NULL;
	opts->response_iterate_callback		= NULL;

	/* Loop through the options, and get the server/port */
	foreach(lc, options)
	{
		DefElem *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "uri") == 0)
			opts->uri	= defGetString(def);

		if (strcmp(def->defname, "uri_select") == 0)
			opts->uri_select	= defGetString(def);

		if (strcmp(def->defname, "uri_insert") == 0)
			opts->uri_insert	= defGetString(def);

		if (strcmp(def->defname, "uri_delete") == 0)
			opts->uri_delete	= defGetString(def);

		if (strcmp(def->defname, "uri_update") == 0)
			opts->uri_update	= defGetString(def);

		if (strcmp(def->defname, "method_select") == 0)
			opts->method_select	= defGetString(def);

		if (strcmp(def->defname, "method_insert") == 0)
			opts->method_insert	= defGetString(def);

		if (strcmp(def->defname, "method_delete") == 0)
			opts->method_delete	= defGetString(def);

		if (strcmp(def->defname, "method_update") == 0)
			opts->method_update	= defGetString(def);

		if (strcmp(def->defname, "request_serialize_callback") == 0)
			opts->request_serialize_callback	= defGetString(def);

		if (strcmp(def->defname, "response_type") == 0)
			opts->response_type	= defGetString(def);

		if (strcmp(def->defname, "response_deserialize_callback") == 0)
			opts->response_deserialize_callback	= defGetString(def);

		if (strcmp(def->defname, "response_iterate_callback") == 0)
			opts->response_iterate_callback	= defGetString(def);
	}

	/* Default values, if required */
	if (!opts->uri_select) opts->uri_select	= "";
	if (!opts->uri_insert) opts->uri_insert	= "";
	if (!opts->uri_delete) opts->uri_delete	= "";
	if (!opts->uri_update) opts->uri_update	= "";

	if (!opts->method_select) opts->method_select	= "GET";
	if (!opts->method_insert) opts->method_insert	= "PUT";
	if (!opts->method_delete) opts->method_delete	= "DELETE";
	if (!opts->method_update) opts->method_update	= "POST";

	if (!opts->response_type) opts->response_type	= "json";

	/* Check we have mandatory options */
	if (!opts->uri)
		ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("At least uri option must be specified")
			));
}

static
Oid
get_www_fdw_options_oid(void)
{
	if(0 == www_fdw_options_oid) {
		Datum	data[1];
		bool	isnull[1]	= {false};
		int		res;

		res	= SPI_execute("SELECT t.oid,t.typname,t.typnamespace FROM pg_type t join pg_namespace ns ON t.typnamespace=ns.oid WHERE ns.nspname=current_schema() AND t.typname='wwwfdwoptions'", true, 0);
		if(0 > res)
		{
			ereport(ERROR,
				(
					errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Can't identify WWW_fdw_options OID: %i (%s)", res, describe_spi_code(res))
				)
			);
		}

		if(1 == SPI_processed)
		{
			heap_deform_tuple(*(SPI_tuptable->vals), SPI_tuptable->tupdesc, data, isnull);
			elog(DEBUG1, "Oid: %d",(int)data[0]);
			www_fdw_options_oid	= (Oid)(data[0]);
		}
		else
		{
			ereport(ERROR,
				(
					errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Can't identify WWW_fdw_options OID: not exactly 1 result was returned (%d)", SPI_processed)
				)
			);
		}
	}

	return	www_fdw_options_oid;
}

/* DEBUG
WARNING:  transaction left non-empty SPI stack
HINT:  Check for missing "SPI_finish" calls.
*/
/* wrapper for init code, not to duplicate it */
static
void
spi_init(void)
{
	/* looks like we can't rely on initialization it once */
	int	res	= SPI_connect();
	if(SPI_OK_CONNECT != res)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Can't spi connect: %i (%s)", res, describe_spi_code(res))
			)
		);
	}
}
