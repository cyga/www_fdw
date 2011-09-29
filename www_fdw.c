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

#include "curl/curl.h"
#include "libjson-0.8/json.h"

PG_MODULE_MAGIC;

#define PROCID_TEXTEQ 67

struct WWWFdwOption
{
	const char	*optname;
	Oid		optcontext;	/* Oid of catalog in which option may appear */
};

/*
 * Valid options for mysql_fdw.
 *
 */
static struct WWWFdwOption valid_options[] =
{
	/* TODO */
	{ "callback",		ForeignServerRelationId },

	/* Sentinel */
	{ NULL,			InvalidOid }
};

typedef struct ResultRoot
{
	struct ResultArray	   *results;
} ResultRoot;

typedef struct ResultArray
{
	int					index;
	struct Tweet	   *elements[512];
} ResultArray;

typedef struct Tweet
{
	char	   *id;
	char	   *text;
	char	   *from_user;
	char	   *from_user_id;
	char	   *to_user;
	char	   *to_user_id;
	char	   *iso_language_code;
	char	   *source;
	char	   *profile_image_url;
	char	   *created_at;
} Tweet;

typedef struct WWWReply
{
	ResultRoot	   *root;
	AttInMetadata  *attinmeta;
	int				rownum;
	char		   *q;
} WWWReply;

static bool wwwIsValidOption(const char *option, Oid context);

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
static FdwPlan *wwwPlan(Oid foreigntableid,
						PlannerInfo *root,
						RelOptInfo *baserel);
static void wwwExplain(ForeignScanState *node, ExplainState *es);
static void wwwBegin(ForeignScanState *node, int eflags);
static TupleTableSlot *wwwIterate(ForeignScanState *node);
static void wwwReScan(ForeignScanState *node);
static void wwwEnd(ForeignScanState *node);

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp);
static void *create_structure(int nesting, int is_object);
static void *create_data(int type, const char *data, uint32_t length);
static int append(void *structure, char *key, uint32_t key_length, void *obj);


Datum
www_fdw_validator(PG_FUNCTION_ARGS)
{
	List		*options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid		catalog = PG_GETARG_OID(1);
	char		*callback = NULL;
	ListCell	*cell;
	int			res;

	/*
	 * Check that only options supported by mysql_fdw,
	 * and allowed for the current object type, are given.
	 */
	foreach(cell, options_list)
	{
		DefElem	   *def = (DefElem *) lfirst(cell);

		if (!wwwIsValidOption(def->defname, catalog))
		{
			struct WWWFdwOption *opt;
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

		if (strcmp(def->defname, "callback") == 0)
		{
			if (callback)
				ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("conflicting or redundant options: address (%s)", defGetString(def))
					));

			callback = defGetString(def);

			/* DEBUG */
			res	= SPI_connect();
			if(SPI_OK_CONNECT != res)
			{
				ereport(
					ERROR,
					(
						errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("can't spi connect: %i", res)
					)
				);
			}
			res	= SPI_execute(callback, false, 0);
			/* if(SPI_OK_SELECT != res) */
			if(0 > res)
			{
				ereport(
					ERROR,
					(
						errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("can't spi execute: %i", res)
					)
				);
			}
			else
			{
				ereport(
					NOTICE,
					(
						errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("number of rows: %i", SPI_processed)
					)
				);
				//SPI_tuptable->vals->
			}
			res	= SPI_finish();
			if(SPI_OK_FINISH != res)
			{
				ereport(
					ERROR,
					(
						errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("can't spi finish: %i", res)
					)
				);
			}
			res	= SPI_execute(callback, false, 1);
		}
		else if (strcmp(def->defname, "TODO") == 0)
		{
			/* if (svr_port) */
			/* 	ereport(ERROR, */
			/* 		(errcode(ERRCODE_SYNTAX_ERROR), */
			/* 		errmsg("conflicting or redundant options: port (%s)", defGetString(def)) */
			/* 		)); */

			/* svr_port = atoi(defGetString(def)); */
		}
	}

	PG_RETURN_VOID();
}

/*
 * Check if the provided option is one of the valid options.
 * context is the Oid of the catalog holding the object the option is for.
 */
static bool
wwwIsValidOption(const char *option, Oid context)
{
	struct WWWFdwOption *opt;

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
	fdwroutine->PlanForeignScan = wwwPlan;
	fdwroutine->ExplainForeignScan = wwwExplain;
	fdwroutine->BeginForeignScan = wwwBegin;
	fdwroutine->IterateForeignScan = wwwIterate;
	fdwroutine->ReScanForeignScan = wwwReScan;
	fdwroutine->EndForeignScan = wwwEnd;

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

static char *
www_param(Node *node, TupleDesc tupdesc)
{
	if (node == NULL)
		return NULL;

	if (IsA(node, OpExpr))
	{
		OpExpr	   *op = (OpExpr *) node;
		Node	   *left, *right;
		Index		varattno;
		char	   *key, *val;

		if (list_length(op->args) != 2)
			return NULL;
		left = list_nth(op->args, 0);
		if (!IsA(left, Var))
			return NULL;
		varattno = ((Var *) left)->varattno;
		Assert(0 < varattno && varattno <= tupdesc->natts);
		key = NameStr(tupdesc->attrs[varattno - 1]->attname);

		if (strcmp(key, "q") == 0)
		{
			right = list_nth(op->args, 1);
			if (op->opfuncid != PROCID_TEXTEQ)
				elog(ERROR, "invalid operator");

			if (IsA(right, Const))
			{
				StringInfoData	buf;

				initStringInfo(&buf);
				val = TextDatumGetCString(((Const *) right)->constvalue);
				appendStringInfo(&buf, "q=%s",
					percent_encode((unsigned char *) val, -1));
				return buf.data;
			}
			else
				elog(ERROR, "www_fdw: parameter q must be a constant");
		}
	}

	return NULL;
}
/*
 * wwwPlan
 *   Create a FdwPlan, which is empty for now.
 */
static FdwPlan *
wwwPlan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel)
{
	FdwPlan	   *fdwplan;

	fdwplan = makeNode(FdwPlan);
	fdwplan->fdw_private = NIL;

	return fdwplan;
}

/*
 * wwwExplain
 *   Produce extra output for EXPLAIN
 */
static void
wwwExplain(ForeignScanState *node, ExplainState *es)
{
	ExplainPropertyText("WWW API", "Search", es);
}

/*
 * wwwBegin
 *   Query search API and setup result
 */
static void
wwwBegin(ForeignScanState *node, int eflags)
{
	CURL		   *curl;
	int				ret;
	json_parser		parser;
	json_parser_dom helper;
	ResultRoot	   *root;
	Relation		rel;
	AttInMetadata  *attinmeta;
	WWWReply   *reply;
	StringInfoData	url;
	char		   *param_q = NULL;

	/*
	 * Do nothing in EXPLAIN
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

	initStringInfo(&url);
	appendStringInfoString(&url, "http://search.www.com/search.json");

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
					appendStringInfoChar(&url, '?');
				else
					appendStringInfoChar(&url, '&');
				appendStringInfoString(&url, param);
				if (param[0] == 'q' && param[1] == '=')
					param_q = &param[2];

				/* take it from original qual */
				node->ss.ps.qual = list_delete(node->ss.ps.qual, (void *) state);
			}
//			else
//				elog(ERROR, "Unknown qual");
		}
//		node->ss.ps.qual = NIL;
	}

	json_parser_dom_init(&helper, create_structure, create_data, append);
	json_parser_init(&parser, NULL, json_parser_dom_callback, &helper);

	elog(DEBUG1, "requesting %s", url.data);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.data);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser);
	ret = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	rel = node->ss.ss_currentRelation;
	attinmeta = TupleDescGetAttInMetadata(rel->rd_att);

	root = (ResultRoot *) helper.root_structure;

	/* status != 200, or other similar error */
	if (!root)
		elog(INFO, "Failed fetching response from %s", url.data);

#ifdef NOT_USE
	if (root->results)
	{
		int i;
		for(i = 0; i < root->results->index; i++)
		{
			Tweet *tweet = root->results->elements[i];
			printf("%02d[%s]: %s\n", (i + 1), tweet->from_user, tweet->text);
		}
	}
#endif

	reply = (WWWReply *) palloc(sizeof(WWWReply));
	reply->root = root;
	reply->attinmeta = attinmeta;
	reply->rownum = 0;
	reply->q = param_q;
	node->fdw_state = (void *) reply;

	json_parser_free(&parser);
}

/*
 * wwwIterate
 *   Return a www per call
 */
static TupleTableSlot *
wwwIterate(ForeignScanState *node)
{
	TupleTableSlot	   *slot = node->ss.ss_ScanTupleSlot;
	WWWReply	   *reply = (WWWReply *) node->fdw_state;
	ResultRoot		   *root = reply->root;
	Tweet			   *tweet;
	HeapTuple			tuple;
	Relation			rel = node->ss.ss_currentRelation;
	int					i, natts;
	char			  **values;
	MemoryContext		oldcontext;

	if (!root || !(root->results && reply->rownum < root->results->index))
	{
		ExecClearTuple(slot);
		return slot;
	}
	tweet = root->results->elements[reply->rownum];
	natts = rel->rd_att->natts;
	values = (char **) palloc(sizeof(char *) * natts);
	for (i = 0; i < natts; i++)
	{
		Name	attname = &rel->rd_att->attrs[i]->attname;

		if (namestrcmp(attname, "id") == 0 && tweet->id)
			values[i] = tweet->id;
		else if (namestrcmp(attname, "text") == 0 && tweet->text)
			values[i] = tweet->text;
		else if (namestrcmp(attname, "from_user") == 0 && tweet->from_user)
			values[i] = tweet->from_user;
		else if (namestrcmp(attname, "from_user_id") == 0 && tweet->from_user_id)
			values[i] = tweet->from_user_id;
		else if (namestrcmp(attname, "to_user") == 0 && tweet->to_user)
			values[i] = tweet->to_user;
		else if (namestrcmp(attname, "to_user_id") == 0 && tweet->to_user_id)
			values[i] = tweet->to_user_id;
		else if (namestrcmp(attname, "iso_language_code") == 0 && tweet->iso_language_code)
			values[i] = tweet->iso_language_code;
		else if (namestrcmp(attname, "source") == 0 && tweet->source)
			values[i] = tweet->source;
		else if (namestrcmp(attname, "profile_image_url") == 0 && tweet->profile_image_url)
			values[i] = tweet->profile_image_url;
		else if (namestrcmp(attname, "created_at") == 0 && tweet->created_at)
			values[i] = tweet->created_at;
		else if (namestrcmp(attname, "q") == 0)
			values[i] = reply->q;
		else
			values[i] = NULL;
	}
	oldcontext = MemoryContextSwitchTo(node->ss.ps.ps_ExprContext->ecxt_per_query_memory);
	tuple = BuildTupleFromCStrings(reply->attinmeta, values);
	MemoryContextSwitchTo(oldcontext);
	ExecStoreTuple(tuple, slot, InvalidBuffer, true);
	reply->rownum++;

	return slot;
}

/*
 * wwwReScan
 */
static void
wwwReScan(ForeignScanState *node)
{
	WWWReply	   *reply = (WWWReply *) node->fdw_state;

	reply->rownum = 0;
}

static void
wwwEnd(ForeignScanState *node)
{
	/* intentionally left blank */
}

static size_t
write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	int			segsize = size * nmemb;
	json_parser *parser = (json_parser *) userp;
	int			ret;

	ret = json_parser_string(parser, buffer, segsize, NULL);
	if (ret){
		elog(ERROR, "json_parser failed");
	}

	return segsize;
}

/*
 * since create_structure() raise error on returning NULL,
 * dummy pointer will be returned if the result can be discarded.
 */
static void *dummy_p = (void *) "dummy";

static void *
create_structure(int nesting, int is_object)
{
	if (is_object)
	{
		if (nesting == 0)
		{
			ResultRoot	   *root;

			root = (ResultRoot *) palloc0(sizeof(ResultRoot));
			return (void *) root;
		}
		else if (nesting == 2)
		{
			Tweet	   *tweet;

			tweet = (Tweet *) palloc0(sizeof(Tweet));
			return (void *) tweet;
		}
		return dummy_p;
	}
	else
	{
		if (nesting == 1)
		{
			ResultArray	   *array;

			array = (ResultArray *) palloc(sizeof(ResultArray));
			array->index = 0;
			return array;
		}
	}

	return dummy_p;
}

static void *
create_data(int type, const char *data, uint32_t length)
{
	switch(type)
	{
	case JSON_STRING:
	case JSON_INT:
	case JSON_FLOAT:
		return (void *) data;

	case JSON_NULL:
	case JSON_TRUE:
	case JSON_FALSE:
		break;
	}

	return NULL;
}

#define TWEETCOPY(structure, key, obj) \
do{ \
	Tweet  *tweet = (Tweet *) (structure); \
	int		len = strlen((char *) (obj)); \
	if (len > 0) \
	{ \
		tweet->key = (char *) palloc(sizeof(char) * (len + 1)); \
		strcpy(tweet->key, (obj)); \
	} \
} while(0)

static int
append(void *structure, char *key, uint32_t key_length, void *obj)
{
	if (key != NULL)
	{
		/* discard any unnecessary data */
		if (structure == dummy_p)
			return 0;
		if (strcmp(key, "results") == 0)
		{
			/* root.results = array; */
			((ResultRoot *) structure)->results = (ResultArray *) obj;
		}
		else if (strcmp(key, "id") == 0 && obj)
			TWEETCOPY(structure, id, obj);
		else if (strcmp(key, "text") == 0 && obj)
			TWEETCOPY(structure, text, obj);
		else if(strcmp(key, "from_user") == 0 && obj)
			TWEETCOPY(structure, from_user, obj);
		else if(strcmp(key, "from_user_id") == 0 && obj)
			TWEETCOPY(structure, from_user_id, obj);
		else if(strcmp(key, "to_user") == 0 && obj)
			TWEETCOPY(structure, to_user, obj);
		else if(strcmp(key, "to_user_id") == 0 && obj)
			TWEETCOPY(structure, to_user_id, obj);
		else if(strcmp(key, "iso_language_code") == 0)
			TWEETCOPY(structure, iso_language_code, obj);
		else if(strcmp(key, "source") == 0 && obj)
			TWEETCOPY(structure, source, obj);
		else if(strcmp(key, "profile_image_url") == 0 && obj)
			TWEETCOPY(structure, profile_image_url, obj);
		else if(strcmp(key, "created_at") == 0 && obj)
			TWEETCOPY(structure, created_at, obj);
	}
	else
	{
		/*
		 * array.push(tweet);
		 * an array that is not dummy_p must be root.results
		 */
		ResultArray *array = (ResultArray *) structure;

		if (array != dummy_p)
			array->elements[array->index++] = (Tweet *) obj;
	}
	return 0;
}
