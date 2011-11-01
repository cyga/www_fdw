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
#include "utils/xml.h"

#include "curl/curl.h"
#include "../libjson-0.8/json.h"
#include "json_parser.h"

#include <libxml/parser.h>
#include <libxml/tree.h>


PG_MODULE_MAGIC;

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
	{ "request_serialize_type",	ForeignServerRelationId },

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
	char*	request_serialize_type;
	char*	response_type;
	char*	response_deserialize_callback;
	char*	response_iterate_callback;
} WWW_fdw_options;

typedef struct Reply
{
	HeapTuple		*tuples;	/* array with result tuples */
	uint32			ntuples;	/* number of results */
	int				tuple_index;
	WWW_fdw_options	*options;
	Oid			  	opts_type;
	Datum		  	opts_value;
} Reply;

typedef struct PostParameters
{
	bool			post;
	StringInfoData	data;
	StringInfoData	content_type;
} PostParameters;

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

#define DEBUG
#ifdef DEBUG
#define WHERESTR "[file %s, line %d]"
#define WHEREARG __FILE__, __LINE__
#define d(...) (elog(DEBUG1, WHERESTR, WHEREARG), elog(DEBUG1, __VA_ARGS__))
#else
#define d(...)
#endif

static void get_www_fdw_options(WWW_fdw_options *opts, Oid *opts_type, Datum *opts_value);
static void get_www_fdw_post_parameters(PostParameters *post, Oid *post_type, Datum *post_value);

static size_t json_write_data_to_parser(void *buffer, size_t size, size_t nmemb, void *userp);
static size_t xml_write_data_to_parser(void *buffer, size_t size, size_t nmemb, void *userp);
static size_t write_data_to_buffer(void *buffer, size_t size, size_t nmemb, void *userp);
static Datum make_text_data(StringInfoData *str);

/* wrappers for corresponding SPI_* calls. check for errors */
static void SPI_connect_wrapper(void);
static void SPI_finish_wrapper(void);

/*
 * parse_parameter
 * parse single passed parameter
 */
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

/*
 * www_fdw_validator
 * FDW callback realization
 */
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
	char		*request_serialize_type	= NULL;
	char		*response_type	= NULL;
	char		*response_deserialize_callback	= NULL;
	char		*response_iterate_callback	= NULL;

	d("www_fdw_validator routine");

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
				(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
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
		if(parse_parameter("request_serialize_type", &request_serialize_type, def)) continue;

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

/*
 * www_fdw_handler
 * setup FDW handlers/callbacks
 */
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

/*
 * percent_encode
 * used for url encoding
 */
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

	d("www_plan routine");

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
	d("www_explain routine");

	ExplainPropertyText("WWW API", "Request", es);
}

/*
 * describe_spi_code
 * return string description for spi function return code
 */
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
 * serialize request parameters using specified callback
 * currently following serialize of quals are supported:
 *  * same as with debug_print_parse
 *  * null (pass as it as null no matter on quals)
 *  * empty string otherwise (plus issue warning)
 * there is not finished branch for tree serialization into json/xml
 * but it started taking too much time and postponed for next versions (if any)
 */
static
void
serialize_request_with_callback(WWW_fdw_options *opts, Oid opts_type, Datum opts_value, ForeignScanState *node, StringInfoData *url, PostParameters *post)
{
	int	res;
	StringInfoData	cmd, qualSer;
	Oid	argtypes[4];
	Datum argvalues[4], rpost;
	char nulls[4];
	char *rurl = NULL;
	HeapTupleHeader rpost_tuple_header;
	bool isnull;
	MemoryContext	mctxt = CurrentMemoryContext, spimctxt;

	argtypes[0] = opts_type;
	argvalues[0] = opts_value;
	nulls[0] = 0;

	if(0 == strcmp("log", opts->request_serialize_type))
	{
		initStringInfo(&qualSer);
		appendStringInfoString( &qualSer, nodeToString(node->ss.ps.plan->qual) );
		nulls[1] = ' ';
	}
	else if(0 == strcmp("null", opts->request_serialize_type))
	{
		nulls[1] = 'n';
	}
	else
	{
		/* set empty string and issue warning */
		initStringInfo(&qualSer);

		ereport(WARNING, 
			(
				errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
				errmsg("Invalid request_serialize_type: %s", opts->request_serialize_type)
			)
		);

		nulls[1] = ' ';
	}
	argtypes[1] = TEXTOID;
	argvalues[1] = 'n' == nulls[1] ? PointerGetDatum(NULL) : make_text_data(&qualSer);

	argtypes[2] = TEXTOID;
	argvalues[2] = make_text_data(url);
	nulls[2] = 0;

	get_www_fdw_post_parameters(post, &(argtypes[3]), &(argvalues[3]));
	nulls[3] = argvalues[3] ? 0 : 1;

	initStringInfo(&cmd);
	appendStringInfo(&cmd, "SELECT * FROM %s($1,$2,$3,$4)", opts->request_serialize_callback);

	SPI_connect_wrapper();
	res	= SPI_execute_with_args(cmd.data, 4, argtypes, argvalues, nulls, true, 0);

	if(0 > res)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Can't spi execute: %i (%s)", res, describe_spi_code(res))
			)
		);
	}

	if(0 == SPI_processed)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("No results were returned from response_iterate_callback '%s': %i", opts->response_iterate_callback, SPI_processed)
			)
		);
	}

	if(1 < SPI_processed)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("More than 1 result was returned from response_iterate_callback '%s': %i", opts->response_iterate_callback, SPI_processed)
			)
		);
	}

	rurl = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
	rpost = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2, &isnull);

	/* recreate data in correct memory context */
	spimctxt = MemoryContextSwitchTo(mctxt);

	resetStringInfo(url);
	appendStringInfoString(url, rurl);
	
	if(!isnull)
	{
		Datum datum;

		rpost_tuple_header = DatumGetHeapTupleHeader(rpost);
		datum = GetAttributeByName(rpost_tuple_header, "post", &isnull);
		if(isnull)
			post->post = false;
		else
			post->post = DatumGetBool(datum);

		resetStringInfo(&post->data);
		datum = GetAttributeByName(rpost_tuple_header, "data", &isnull);
		if(!isnull)
			appendStringInfoString(&post->data, TextDatumGetCString(datum));

		datum = GetAttributeByName(rpost_tuple_header, "content_type", &isnull);
		resetStringInfo(&post->content_type);
		if(!isnull)
			appendStringInfoString(&post->content_type, TextDatumGetCString(datum));
	}

	MemoryContextSwitchTo(spimctxt);

	SPI_finish_wrapper();
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
 * xml_check_result_array
 *	Check current node to be of following structure:
 *	node
 *		subnode
 *			attr1 > cdata
 *			...
 *			attrN > cdata
 *		...
 *		subnode
 *			attr1 > cdata
 *			...
 *			attrN > cdata
 *	Also node names are checked to match result columns.
 *	At least one name must match. Matching columns must have only one text node child.
 *	All subnodes must have same name.
*/
static
bool
xml_check_result_array(xmlNodePtr node, TupleDesc tuple_desc, xmlChar **attnames)
{
	xmlNodePtr	it		= NULL,
				itc		= NULL;
	const xmlChar *name	= NULL;
	int			i,n		= 0;
	bool		match	= false;

	if(NULL == node)
		return	false;

	/* check all node children have same name */
	for( it = node->children; NULL != it; it = it->next )
	{
		switch(it->type)
		{
			case XML_ELEMENT_NODE:
				if(NULL == name)
					name	= it->name;
				else
					/* check name of node to be the same */
					if(1 != xmlStrEqual(name, it->name))
						return	false;

				/* check children */
				n	= 0;
				for( itc = it->children; NULL != itc; itc = itc->next )
				{
					/* check sub elements names to match result columns */
					for (i = 0; i < tuple_desc->natts; i++)
					{
						if(1 == xmlStrEqual(attnames[i], itc->name))
						{
							/* don't check found value to be of ordinary type here - we can serialize it in the answer */

							/* TODO: check if we need such check:
							 * we need to have only single XML_TEXT_NODE among children
							if(itc->children)
							{

								if(XML_TEXT_NODE != itc->children->type)
									return	false;

								if(itc->children->next)
									return	false;
							}
							*/

							n++;
							break;
						}
					}
				}

				/* none of the children match result columns */
				if(0 == n)
					return	false;
				else
					match	= true;
				break;
			case XML_COMMENT_NODE:
				continue;
			case XML_ATTRIBUTE_NODE:
			case XML_TEXT_NODE:
			case XML_CDATA_SECTION_NODE:
			case XML_ENTITY_REF_NODE:
			case XML_ENTITY_NODE:
			case XML_PI_NODE:
			case XML_DOCUMENT_NODE:
			case XML_DOCUMENT_TYPE_NODE:
			case XML_DOCUMENT_FRAG_NODE:
			case XML_NOTATION_NODE:
			case XML_HTML_DOCUMENT_NODE:
			case XML_DTD_NODE:
			case XML_ELEMENT_DECL:
			case XML_ATTRIBUTE_DECL:
			case XML_ENTITY_DECL:
			case XML_NAMESPACE_DECL:
			case XML_XINCLUDE_START:
			case XML_XINCLUDE_END:
			case XML_DOCB_DOCUMENT_NODE:
				return	false;
		}
	}

	return	match;
}

/*
 * xml_get_result_array_in_doc
 *	search for a result in parsed xml
*/
static
xmlNodePtr
xml_get_result_array_in_doc(xmlDocPtr doc, TupleDesc tuple_desc)
{
	xmlNodePtr	suspect	= NULL,
				it		= NULL;
	List		*curr	= NULL,
				*next	= NULL;
	ListCell*	cell;
	xmlChar**	attnames;
	int			i;

	if(NULL == doc)
		return	NULL;

	/* fill all document nodes as current list for check */
	for( it = doc->children; NULL != it; it = it->next )
	{
		if(XML_ELEMENT_NODE != it->type)
			continue;
		curr	= lappend( curr, (void*)(it) );
	}

	/* prepare attnames in xmlChar*, to save time for strings comparisons in xml_check_result_array */
	attnames	= (xmlChar**)palloc(tuple_desc->natts * sizeof(xmlChar*));
	for( i=0; i<tuple_desc->natts; i++ )
		attnames[i]	= xmlCharStrndup(tuple_desc->attrs[i]->attname.data, strlen(tuple_desc->attrs[i]->attname.data));

	while(curr) {
		foreach(cell, curr)
		{
			suspect	= (xmlNodePtr)(cell->data.ptr_value);
			switch (suspect->type)
			{
				case XML_ELEMENT_NODE:
					/* check node to be valid answer */
					if(xml_check_result_array(suspect, tuple_desc, attnames))
					{
						/* free structures */
						list_free(curr);
						list_free(next);
						for( i=0; i<tuple_desc->natts; i++ )
							free(attnames[i]);
						return	suspect;
					}
					for( it = suspect->children; NULL != it; it = it->next )
					{
						if(XML_ELEMENT_NODE != it->type)
							continue;
						next	= lappend( next, (void*)(it) );
					}
					break;
				default:
					break;
			}
		}

		list_free(curr);
		curr	= next;
		next	= NULL;
	}

	/* free structures */
	for( i=0; i<tuple_desc->natts; i++ )
		free(attnames[i]);

	return	NULL;
}

/*
 * json_check_result_array
 *	Currently checks all elements to be objects.
 *	And checks if at least one key in the object matches result columns.
*/
static
bool
json_check_result_array(JSONNode* root, TupleDesc tuple_desc)
{
	int	i,j,k,n;

	for( i=0; i<root->length; i++ )
	{
		if(JSON_OBJECT_BEGIN != root->val.val_array[i].type)
			return	false;

		/* check if at least one key matchs result columns */
		n	= 0;
		for( j=0; j<root->val.val_array[i].length && 0==n; j++ )
		{
			for( k=0; k<tuple_desc->natts && 0==n; k++ )
			{
				if(0 == namestrcmp(&tuple_desc->attrs[k]->attname, root->val.val_array[i].val.val_object[j].key))
				{
					/* don't check found value to be of ordinary type here - we can serialize it in the answer */

					n++;
					break;
				}
			}
		}

		if(0 == n)
			return	false;
	}

	return	true;
}

/*
 * json_get_result_array_in_tree
 *	find the result array in json response structure
 *	check json_check_result_array for valid array description
*/
static
JSONNode*
json_get_result_array_in_tree(JSONNode* root, TupleDesc tuple_desc)
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
					if(json_check_result_array(suspect, tuple_desc))
					{
						/* free structure */
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

static
Datum
make_text_data(StringInfoData *str)
{
	text *t	= (text*)palloc(VARHDRSZ + str->len);
	SET_VARSIZE(t, VARHDRSZ + str->len);
	memcpy(t->vl_dat, str->data, str->len);

	return PointerGetDatum(t);
}

/*
 * call_response_deserialize_callback
 * call to response_deserialize_callback for "xml" response_type
 * prepare/setup reply structure from it's answer
 */
static
Reply*
call_response_deserialize_callback(ForeignScanState *node, WWW_fdw_options *opts, Oid opts_type, Datum opts_value, StringInfoData *buffer)
{
	int		res, i,j, natts;
	StringInfoData	cmd;
	Oid		opts_argtypes[2]	= { opts_type, TEXTOID };
	Datum	opts_values[2];
	Reply	*reply;
	Relation		rel;
	AttInMetadata	*attinmeta;
	char			**values	= NULL,
		**names	= NULL;
	int16			*columns;
	MemoryContext	mctxt = CurrentMemoryContext, spimctxt;
	bool			use_libxml	= false;
	xmltype			*xml;

	opts_values[0]	= opts_value;

	rel = node->ss.ss_currentRelation;
	attinmeta = TupleDescGetAttInMetadata(rel->rd_att);

	SPI_connect_wrapper();

	opts_values[1]	= make_text_data(buffer);

#ifdef USE_LIBXML
	d("compiled with xml support, passing xml data type to callback");
	use_libxml = true;
#else
	d("compiled without xml support, passing text data type to callback");
#endif

	if(use_libxml && 0 == strcmp("xml", opts->response_type))
	{
		opts_argtypes[1] = XMLOID;
		/* parses xml variable
		 * it parses/checks xml string and returns casted variable
		 */
		xml	= xmlparse((text*)DatumGetPointer(opts_values[1]), XMLOPTION_DOCUMENT, true);
		opts_values[1]	= XmlPGetDatum(xml);
	}

	/* do callback */
	initStringInfo(&cmd);
	appendStringInfo(&cmd, "SELECT * FROM %s($1,$2)", opts->response_deserialize_callback);

	d("calling response_deserialize_callback: '%s'", opts->response_deserialize_callback);

	res	= SPI_execute_with_args(cmd.data, 2, opts_argtypes, opts_values, NULL, true, 0);
	if(0 > res)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Can't execute response_deserialize_callback '%s': %i (%s)", opts->response_deserialize_callback, res, describe_spi_code(res))
			)
		);
	}

	/* save result to output parameters */
	/* allocate it in proper memory context */
	reply		= (Reply*)SPI_palloc(sizeof(Reply));
	reply->ntuples	= SPI_processed;
	reply->tuple_index	= 0;
	reply->options		= opts;
	reply->opts_type	= opts_type;
	reply->opts_value	= opts_value;
	/* copy tuples structure: there can be further calls to SPI_exec* */
	reply->tuples	= (HeapTuple*)SPI_palloc(reply->ntuples * sizeof(HeapTuple));

	/* find correspondence between returned columns and columns we need to return */
	natts = rel->rd_att->natts;
	columns	= (int16*)palloc(natts * sizeof(int16));
	names	= (char**)palloc(SPI_tuptable->tupdesc->natts * sizeof(char*));
	for( i=1; i<=SPI_tuptable->tupdesc->natts; i++ )
		names[i-1]	= SPI_fname(SPI_tuptable->tupdesc, i);

	/* find column places */
	for( i=0; i<natts; i++ )
	{
		columns[i]	= -1;

		for( j=0; j<SPI_tuptable->tupdesc->natts; j++ )
		{
			if(0 == namestrcmp(&rel->rd_att->attrs[i]->attname, names[j]))
			{
				columns[i]	= j+1;
				break;
			}
		}
	}
	pfree(names);

	/* fill column values */
	values = (char **) palloc(sizeof(char *) * natts);
	for( i=0; i<reply->ntuples; i++ )
	{
		for( j=0; j<natts; j++ )
		{
			if(-1 == columns[j])
				values[j]	= NULL;
			else
				values[j]	= SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, columns[j]);
		}

		/* build tuple in proper memory context */
		spimctxt = MemoryContextSwitchTo(mctxt);
		reply->tuples[i] = BuildTupleFromCStrings(attinmeta, values);
		MemoryContextSwitchTo(spimctxt);

		/* free strings immediately, w/o losing track of them */
		for( j=0; j<natts; j++ )
			if(NULL != values[j])
				SPI_pfree(values[j]);
	}
	pfree(values);

	pfree(columns);

	SPI_finish_wrapper();

	return	reply;
}

/*
 * get_www_fdw_options
 * get Oid for options type, set up Datum value for it properly
 * will be used to pass it to callbacks
 */
static
void
get_www_fdw_options(WWW_fdw_options *opts, Oid *opts_type, Datum *opts_value)
{
	Datum	data[1];
	bool	isnull[1]	= {false};
	int		res;
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
		opts->request_serialize_type,

		opts->response_type,
		opts->response_deserialize_callback,
		opts->response_iterate_callback
	};
	TupleDesc		tuple_desc;
	AttInMetadata*	aim;
	MemoryContext	mctxt = CurrentMemoryContext, spimctxt;

	SPI_connect_wrapper();
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
		/* Oid is typedef for unsigned int, value will be copied */
		*opts_type	= (Oid)(data[0]);

		tuple_desc	= TypeGetTupleDesc(*opts_type, NIL);
		aim			= TupleDescGetAttInMetadata(tuple_desc);

		/* switch to memory context before spi for saving value */
		spimctxt = MemoryContextSwitchTo(mctxt);
		*opts_value	= HeapTupleGetDatum( BuildTupleFromCStrings(aim, options) );
		MemoryContextSwitchTo(spimctxt);

		SPI_finish_wrapper();
		return;
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

	SPI_finish_wrapper();
}

/*
 * get_www_fdw_post_parameters
 * get Oid for WWWFdwPostParameters type, set up Datum value for it properly
 * will be used to pass it to callbacks
 */
static
void
get_www_fdw_post_parameters(PostParameters *post, Oid *post_type, Datum *post_value)
{
	Datum	data[1];
	bool	isnull[1]	= {false};
	int		res;
	char*	postparams[]	= {
		post->post ? "t" : "f",
		post->data.data,
		post->content_type.data
	};
	TupleDesc		tuple_desc;
	AttInMetadata*	aim;
	MemoryContext	mctxt = CurrentMemoryContext, spimctxt;

	SPI_connect_wrapper();
	res	= SPI_execute("SELECT t.oid,t.typname,t.typnamespace FROM pg_type t join pg_namespace ns ON t.typnamespace=ns.oid WHERE ns.nspname=current_schema() AND t.typname='wwwfdwpostparameters'", true, 0);
	if(0 > res)
	{
		ereport(ERROR,
				(
					errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Can't identify WWWFdwPostParameters OID: %i (%s)", res, describe_spi_code(res))
					)
			);
	}

	if(1 == SPI_processed)
	{
		heap_deform_tuple(*(SPI_tuptable->vals), SPI_tuptable->tupdesc, data, isnull);
		/* Oid is typedef for unsigned int, value will be copied */
		*post_type	= (Oid)(data[0]);

		tuple_desc	= TypeGetTupleDesc(*post_type, NIL);
		aim			= TupleDescGetAttInMetadata(tuple_desc);

		/* switch to memory context before spi for saving value */
		spimctxt = MemoryContextSwitchTo(mctxt);
		*post_value	= HeapTupleGetDatum( BuildTupleFromCStrings(aim, postparams) );
		MemoryContextSwitchTo(spimctxt);

		SPI_finish_wrapper();
		return;
	}
	else
	{
		ereport(ERROR,
				(
					errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Can't identify WWWFdwPostParameters OID: not exactly 1 result was returned (%d)", SPI_processed)
					)
			);
	}

	SPI_finish_wrapper();
}

/*
 * json2string
 * convert JSONNode* element to string
 */
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
 * prepare_xml_result
 * go through parsed result and prepare reply/result structure
 */
static
Reply*
prepare_xml_result(ForeignScanState *node, WWW_fdw_options *opts, Oid opts_type, Datum opts_value, xmlDocPtr doc)
{
	xmlNodePtr	result	= NULL,
		it = NULL,
		itc= NULL;
	Relation	rel;
	AttInMetadata	*attinmeta;
	Reply			*reply;
	xmlChar			**attnames;
	int				i,j, natts;
	char			**values	= NULL;

	rel = node->ss.ss_currentRelation;
	attinmeta = TupleDescGetAttInMetadata(rel->rd_att);

	result	= xml_get_result_array_in_doc(doc, rel->rd_att);
	if(!result)
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
				 errmsg("Can't find result in parsed server's xml response")
					));

	d("Result array was found in xml response");

	/* save result */
	reply = (Reply*)palloc(sizeof(Reply));
	reply->tuple_index = 0;
	reply->options = opts;
	reply->opts_type = opts_type;
	reply->opts_value = opts_value;

	/* calculate number of results */
	reply->ntuples = 0;
	for( it = result->children; NULL != it; it = it->next )
		reply->ntuples++;

	reply->tuples = (HeapTuple*)palloc(reply->ntuples * sizeof(HeapTuple));

	/* prepare result column names in xmlChar* for proper comparison */
	natts = rel->rd_att->natts;
	attnames	= (xmlChar**)palloc(natts * sizeof(xmlChar*));
	for (i = 0; i < natts; i++)
		attnames[i]	= xmlCharStrndup(rel->rd_att->attrs[i]->attname.data, strlen(rel->rd_att->attrs[i]->attname.data));

	/* find column places */
	values = (char **) palloc(sizeof(char *) * natts);
	for( it = result->children, j=0 ; NULL != it; it = it->next, j++ )
	{
		for (i = 0; i < natts; i++)
		{
			for( itc = it->children; NULL != itc; itc = itc->next )
				if(1 == xmlStrEqual(attnames[i], itc->name))
					break;

			if(itc)
				values[i]	= (char*)itc->children->content;
			else
				values[i]	= NULL;
		}

		reply->tuples[j] = BuildTupleFromCStrings(attinmeta, values);
	}
	pfree(values);

	pfree(attnames);

	return	reply;
}

/*
 * prepare_json_result
 * go through parsed result and prepare reply/result structure
 */
static
Reply*
prepare_json_result(ForeignScanState *node, WWW_fdw_options *opts, Oid opts_type, Datum opts_value, JSONNode *root)
{
	JSONNode		*result, json_obj;
	Relation		rel;
	AttInMetadata	*attinmeta;
	Reply			*reply;
	int				i,j,k, natts;
	char			**values	= NULL;

	rel = node->ss.ss_currentRelation;
	attinmeta = TupleDescGetAttInMetadata(rel->rd_att);

	result	= json_get_result_array_in_tree(root, rel->rd_att);
	if(!result)
		ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
				 errmsg("Can't find result in parsed server's json response")
					));

	d("Result array was found in json response");

	/* prepare result */
	reply = (Reply*)palloc(sizeof(Reply));
	reply->tuple_index = 0;
	reply->options = opts;
	reply->opts_type = opts_type;
	reply->opts_value = opts_value;
	reply->ntuples	= result->length;
	reply->tuples	= (HeapTuple*)palloc(result->length * sizeof(HeapTuple));

	natts = rel->rd_att->natts;
	values = (char **) palloc(sizeof(char *) * natts);
	for( i=0; i<result->length; i++ )
	{
		/* find column places */
		json_obj = result->val.val_array[i];
		for( j=0; j<natts; j++ )
		{
			for( k=0; k<json_obj.length; k++ )
				if(0 == namestrcmp(&rel->rd_att->attrs[j]->attname, json_obj.val.val_object[k].key))
					break;

			if(k < json_obj.length)
				values[j]	= json2string(&(json_obj.val.val_object[k]));
			else
				values[j]	= NULL;
		}

		reply->tuples[i] = BuildTupleFromCStrings(attinmeta, values);
	}
	pfree(values);

	return	reply;
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
	xmlParserCtxtPtr	xml_parserr	= NULL;
	StringInfoData	buffer;
	Oid				opts_type	= 0;
	Datum			opts_value	= 0;
	PostParameters	post;
	struct curl_slist	*curl_opts = NULL;

	d("www_begin routine");

	/*
	 * Do nothing in EXPLAIN
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

	d("www_begin routine, not explain only call");

	opts	= (WWW_fdw_options*)palloc(sizeof(WWW_fdw_options));
	get_options( RelationGetRelid(node->ss.ss_currentRelation), opts );

	initStringInfo(&url);
	appendStringInfo(&url, "%s%s", opts->uri, opts->uri_select);

	/* initialize options type and value if any of callback specified */
	if(
		opts->request_serialize_callback
		||
		opts->response_deserialize_callback
		||
		opts->response_iterate_callback
	)
	{
		get_www_fdw_options(opts, &opts_type, &opts_value);
	}

	post.post = false;
	if(opts->request_serialize_callback)
	{
		/* call specified callback for forming request */
		initStringInfo(&post.data);
		initStringInfo(&post.content_type);
		serialize_request_with_callback(opts, opts_type, opts_value, node, &url, &post);
	}
	else
	{
		serialize_request_parameters(node, &url);
	}

	d("Url for request: '%s'", url.data);

	/* interacting with the server */
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.data);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_buffer);
	if(post.post)
	{
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		if(0 < post.content_type.len)
		{
			curl_slist_append(curl_opts, "Content-type:");
			curl_slist_append(curl_opts, post.content_type.data);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_opts);
		}
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.data.data);
	}

	/* prepare parsers */
	if( 0 == strcmp(opts->response_type, "json") )
	{
		if(opts->response_deserialize_callback)
		{
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_to_buffer);
			initStringInfo(&buffer);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
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
		if(opts->response_deserialize_callback)
		{
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_to_buffer);
			initStringInfo(&buffer);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		}
		else
		{
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, xml_write_data_to_parser);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xml_parserr);
		}
	}
	else if( 0 == strcmp(opts->response_type, "other") )
	{
		if(opts->response_deserialize_callback)
		{
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_to_buffer);
			initStringInfo(&buffer);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		}
		else
			ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
					 errmsg("No response_deserialize_callback for response_type='other'")
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
	if(curl_opts)
		curl_slist_free_all(curl_opts);

	/* process parsed results */
	if( 0 == strcmp(opts->response_type, "json") )
	{
		if(opts->response_deserialize_callback)
		{
			node->fdw_state = (void*)call_response_deserialize_callback(node, opts, opts_type, opts_value, &buffer);
		}
		else
		{
			JSONNode	*root	= json_result_tree(&json_parserr);

			if(!root)
				ereport(ERROR,
						(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
						 errmsg("Can't parse server's json response, parser error code: %i", ret)
							));

			d("JSON response was parsed");

			node->fdw_state = (void*)prepare_json_result(node, opts, opts_type, opts_value, root);

			json_parser_free(&json_parserr);
			/* all data from tree was trully copid, free it up: */
			json_free_tree(root);
		}
	}
	else if( 0 == strcmp(opts->response_type, "xml") )
	{
		if(opts->response_deserialize_callback)
		{
			node->fdw_state = (void*)call_response_deserialize_callback(node, opts, opts_type, opts_value, &buffer);
		}
		else
		{
			/* get result in parsed response tree
			 * and save it for further processing in results iterations
			*/

			xmlDocPtr	doc		= NULL;
			int			res;

			/* there is no more input, indicate the parsing is finished */
		    xmlParseChunk(xml_parserr, curl_error_buffer, 0, 1);

			doc	= xml_parserr->myDoc;
			res	= xml_parserr->wellFormed;

			xmlFreeParserCtxt(xml_parserr);

			if(!res)
				ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
					errmsg("Response xml isn't well formed")
					));

			d("Xml response was parsed");

			node->fdw_state = (void*)prepare_xml_result(node, opts, opts_type, opts_value, doc);

			/* result data was trully copid: free it up */
			xmlFreeDoc(doc);
		}
	}
	else if( 0 == strcmp(opts->response_type, "other") )
	{
		/* checked already that we have response_deserialize_callback */
		node->fdw_state = (void*)call_response_deserialize_callback(node, opts, opts_type, opts_value, &buffer);
	}
}

static
HeapTuple
call_response_iterate_callback(ForeignScanState *node, WWW_fdw_options *opts, Oid opts_type, Datum opts_value, HeapTuple tuple)
{
	int res;
	StringInfoData cmd;
	Oid		opts_argtypes[2] = {
		opts_type,
		/* doesn't work here, type isn't set for it: */
		/*HeapTupleGetOid(tuple)*/
		node->ss.ss_currentRelation->rd_att->tdtypeid
	};
	Datum	opts_values[2] = { opts_value, HeapTupleGetDatum(tuple) };
	HeapTuple rtuple = NULL;

	SPI_connect_wrapper();

	/* do callback */
	initStringInfo(&cmd);
	appendStringInfo(&cmd, "SELECT * FROM %s($1,$2)", opts->response_iterate_callback);

	res	= SPI_execute_with_args(cmd.data, 2, opts_argtypes, opts_values, NULL, true, 0);
	if(0 > res)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Can't execute response_iterate_callback '%s': %i (%s)", opts->response_iterate_callback, res, describe_spi_code(res))
			)
		);
	}

	if(0 == SPI_processed)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("No results were returned from response_iterate_callback '%s'", opts->response_iterate_callback)
			)
		);
	}

	if(1 < SPI_processed)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("More than 1 result was returned from response_iterate_callback '%s': %i", opts->response_iterate_callback, SPI_processed)
			)
		);
	}

	/* "make a copy of a row in the upper executor context": */
	rtuple = SPI_copytuple(SPI_tuptable->vals[0]);

	SPI_finish_wrapper();

	return rtuple;
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
	MemoryContext	oldcontext;

	d("www_iterate routine");

	/* no results or results finished */
	if(!reply || !reply->tuples || reply->tuple_index >= reply->ntuples)
	{
		ExecClearTuple(slot);
		return slot;
	}

	/* copy tuple, increment iterator */
	oldcontext = MemoryContextSwitchTo(node->ss.ps.ps_ExprContext->ecxt_per_query_memory);
	if(reply->options->response_iterate_callback)
	{
		d("call response_iterate_callback");

		tuple = call_response_iterate_callback(
			node,
			reply->options,
			reply->opts_type,
			reply->opts_value,
			reply->tuples[reply->tuple_index++]);
	}
	else
		tuple = heap_copytuple(reply->tuples[reply->tuple_index++]);
	MemoryContextSwitchTo(oldcontext);
	ExecStoreTuple(tuple, slot, InvalidBuffer, true);

	return slot;
}

/*
 * www_rescan
 * FDW rescan handler/callback
 */
static void
www_rescan(ForeignScanState *node)
{
	Reply	   *reply = (Reply *) node->fdw_state;

	d("www_rescan routine");

	reply->tuple_index	= 0;
}

/*
 * www_end
 * FDW end handler/callback
 */
static void
www_end(ForeignScanState *node)
{
	d("www_end routine");
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
			errmsg("Can't parse server's json response, parser error code: %i", ret)
			));

	return segsize;
}

/*
 * xml_write_data_to_parser
 *	parse xml chunk by chunk
*/
static size_t
xml_write_data_to_parser(void *buffer, size_t size, size_t nmemb, void *userp)
{
	int			segsize = size * nmemb;
	xmlParserCtxtPtr *ctxt = (xmlParserCtxtPtr*) userp;
	int			ret;

	if(NULL == *ctxt) {
		/* parser wasn't initialized,
		 * because we are waiting first chunk for encoding analyze
		*/

		/*
		* Create a progressive parsing context, the 2 first arguments
		* are not used since we want to build a tree and not use a SAX
		* parsing interface. We also pass the first bytes of the document
		* to allow encoding detection when creating the parser but this
		* is optional.
		*/
		*ctxt = xmlCreatePushParserCtxt(NULL, NULL, buffer, segsize, NULL);
		if (*ctxt == NULL) {
			xmlErrorPtr	err	= xmlCtxtGetLastError(*ctxt);
			ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
				errmsg("Can't parse server's xml response: %s", err ? err->message : "")
				));
		}

		return segsize;
	}

	ret = xmlParseChunk(*ctxt, buffer, segsize, 0);
	if (ret)
	{
		xmlErrorPtr	err	= xmlCtxtGetLastError(*ctxt);
		ereport(ERROR,
			(errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
			errmsg("Can't parse server's xml response, parser error code: %i, message: %s", ret, err ? err->message : "")
			));
	}

	return segsize;
}

/*
 * write_data_to_buffer
 *	accumulate json response in buffer for further processing
*/
static size_t
write_data_to_buffer(void *buffer, size_t size, size_t nmemb, void *userp)
{
	StringInfoData	*b	= (StringInfoData*)userp;
	int				s	= size*nmemb;

	appendBinaryStringInfo(b, buffer, s);

	return s;
}

/*
 * get_options
 * fetch the options for a mysql_fdw foreign table.
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
	opts->request_serialize_type	= NULL;
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

		if (strcmp(def->defname, "request_serialize_type") == 0)
			opts->request_serialize_type	= defGetString(def);

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

	if (!opts->request_serialize_type) opts->request_serialize_type	= "log";

	if (!opts->response_type) opts->response_type	= "json";

	/* Check we have mandatory options */
	if (!opts->uri)
		ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("At least uri option must be specified")
			));
}

static
void
SPI_connect_wrapper()
{
	int res	= SPI_connect();
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

static
void
SPI_finish_wrapper()
{
	int res	= SPI_finish();
 	if(SPI_OK_FINISH != res)
	{
		ereport(ERROR,
			(
				errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Can't spi finish: %i (%s)", res, describe_spi_code(res))
			)
		);
	}
}
