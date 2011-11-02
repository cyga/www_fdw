#include "serialize_quals.h"
#include "utils.h"
#include <string.h>

char* INDENT_STRING	= "\t";
size_t INDENT_STRING_LENGTH = 1;

void
set_indent_string(char *str)
{
	INDENT_STRING	= str;
	INDENT_STRING_LENGTH = strlen(INDENT_STRING);
}

const char MAX_INDENT = 128;

static
char*
get_indent(int indent)
{
	static char **indents = NULL;

	if(-1 == indent)
		return "";

	if(indent > MAX_INDENT)
	{
		char *big = (char*)palloc((indent*INDENT_STRING_LENGTH+1)*sizeof(char));
		char i;

		for(i=0; i<indent; i++)
			strncpy(big+i*INDENT_STRING_LENGTH,
					INDENT_STRING, INDENT_STRING_LENGTH);
		*(big+i*INDENT_STRING_LENGTH) = '\0';

		return big;
	}

	if(NULL == indents)
		indents = (char**)palloc(MAX_INDENT*sizeof(char*));

	if(NULL == indents[indent])
	{
		char i;

		indents[indent] = (char*)palloc((indent*INDENT_STRING_LENGTH+1)*sizeof(char));

		for(i=0; i<indent; i++)
			strncpy(indents[indent]+i*INDENT_STRING_LENGTH,
					INDENT_STRING, INDENT_STRING_LENGTH);
		*(indents[indent]+i*INDENT_STRING_LENGTH) = '\0';
	}

	return indents[indent];
}

static
char*
get_space(int indent)
{
	return -1 == indent ? "" : " ";
}

static
char*
get_newline(int indent)
{
	return -1 == indent ? "" : "\n";
}

static
char
next_indent(int indent)
{
	return -1 == indent ? indent : indent+1;
}

/*
 * read description in header file (to keep in single place)
 */
void
serialize_node_with_children_callback_json(int *indent, char *name, List *params, StringInfo prefix, StringInfo suffix)
{
	char indent1 = next_indent(*indent);
	ListCell *lc;
	NodeParameter	*p;

	appendStringInfo(prefix, "%s{%s%s\"name\":%s\"%s\",%s",
					 get_indent(*indent),
					 get_newline(*indent),
					 get_indent(indent1),
					 get_space(*indent),
					 name,
					 get_newline(*indent));

	foreach (lc, params)
	{
		p = (NodeParameter*)lfirst(lc);
		appendStringInfo(prefix, "%s\"%s\":%s\"%s\",%s",
						 get_indent(indent1),
						 p->name,
						 get_space(*indent),
						 p->value,
						 get_newline(*indent));
	}

	appendStringInfo(prefix, "%s\"children\":%s[%s",
					 get_indent(indent1),
					 get_space(*indent),
					 get_newline(*indent));

	appendStringInfo(suffix, "%s]%s%s}%s",
					 get_indent(indent1),
					 get_newline(*indent),
					 get_indent(*indent),
					 get_newline(*indent));

	*indent = next_indent(indent1);
}

/*
 * read description in header file (to keep in single place)
 */
char*
serialize_node_without_children_callback_json(int indent, char *name, List *params, char *value)
{
	/* TODO */
	return "";
}

/*
 * read description in header file (to keep in single place)
 */
void
serialize_node_with_children_callback_xml(int *indent, char *name, List *params, StringInfo prefix, StringInfo suffix)
{
	/* TODO */
}

/*
 * read description in header file (to keep in single place)
 */
char*
serialize_node_without_children_callback_xml(int indent, char *node, List *params, char *value)
{
	/* TODO */
	return "";
}

/*
 * read description in header file (to keep in single place)
 */
char*
serialize_const(Const *c)
{
	/* TODO: check it */
	StringInfoData str;

	d("serialize_const");

	initStringInfo(&str);

	if(c->constisnull)
	{
		appendStringInfo(&str, "null");
		return str.data;
	}

	switch(c->consttype)
	{
		case TEXTOID:
			appendStringInfoString(&str, TextDatumGetCString(c->constvalue));
			return str.data;
		case BOOLOID:
			appendStringInfoChar(&str, DatumGetBool(c->constvalue) ? 't' : 'f');
		case CHAROID:
			appendStringInfoChar(&str, DatumGetChar(c->constvalue));
			return str.data;
		case INT2OID:
			appendStringInfo(&str, "%hi", (short int)DatumGetInt16(c->constvalue));
			return str.data;
		case INT4OID:
			appendStringInfo(&str, "%i", DatumGetInt32(c->constvalue));
			return str.data;
		case INT8OID:
			appendStringInfo(&str, "%li", (long int)DatumGetInt64(c->constvalue));
			return str.data;
		case FLOAT4OID:
			appendStringInfo(&str, "%f", DatumGetFloat4(c->constvalue));
			return str.data;
		case FLOAT8OID:
			appendStringInfo(&str, "%f", DatumGetFloat8(c->constvalue));
			return str.data;
		default:
			appendStringInfo(&str, "'unhandled constant oid: %i'", c->consttype);
			return str.data;
	}
	return str.data;
}

/*
 * read description in header file (to keep in single place)
 */
char*
serialize_node(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb)
{
	if (NULL == node)
		return "";

	if(IsA(node, List))
	{
		ListCell   *lc;
		List	   *nodes = list_copy((List*)node);
		StringInfoData res;
		bool		first = true;

		d("serialize_node List");

		initStringInfo(&res);

		foreach (lc, nodes)
		{
			Node	*subnode= lfirst(lc);
			char	*lcs	= serialize_node(indent, subnode, nwc_cb, nwoc_cb);

			if(first)
				first = false;
			else
				/* TODO: use type callback */
				appendStringInfoString(&res, " AND ");

			/* TODO: use type callback */
			appendStringInfoString(&res, lcs);
		}

		return res.data;
	}
	else if (IsA(node, OpExpr))
	{
		OpExpr	   *op = (OpExpr *)node;
		ListCell   *lc;
		List	   *operands = list_copy(op->args),
			*params = NULL;
		StringInfoData res;
		/*char opno[3], opfuncid[3], opresulttype[3];*/

		d("serialize_node OpExpr");

		initStringInfo(&res);

		/* TODO: use callback */
		/* init res with operator description */
		/*if (op->opfuncid != F_TEXTEQ)*/

		/* TODO DEBUGk HERE: call to callbacks */
		params = lappend(params, "opno");
		/*params = lappend(params, op->opno);*/
		/*appendStringInfoString(&res, nwc_cb("OpExpr"));*/

		foreach (lc, operands)
		{
			Node	*subnode= lfirst(lc);
			char	*lcs	= serialize_node(indent, subnode, nwc_cb, nwoc_cb);

			d("got string: :%s:", lcs);
			/* TODO: use callback */
			appendStringInfoString(&res, lcs);
		}

		return res.data;
	}
	else if(IsA(node, BoolExpr))
	{
		/*BoolExpr *expr	= (BoolExpr*)node;*/
		/* TODO */
		/*AND_EXPR, OR_EXPR, NOT_EXPR*/
		d("serialize_node BoolExpr");

		return "";
	}
	else if(IsA(node, Const))
	{
		d("serialize_node Const");
		return nwoc_cb(indent, "Const", NULL, serialize_const((Const*)node));
	}
	else if(IsA(node, Var))
	{
		/*Var *v = (Var*)node;*/
		/* TODO */
		d("serialize_node Var");

		return "";
	}
	else
	{
		/* TODO add basic info about this unhandled type */
		d("serialize_node else");

		return "";
	}

	return "";
}

/*
 * read description in header file (to keep in single place)
 */
char*
serialize_quals(bool human_readable, List *qual, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb)
{
	char indent = human_readable ? 0 : -1;

	if (NULL == qual)
		return "";

	if(0 == list_length(qual))
	{
		d("serialize_qual 0==length(qual)");

		return "";
	}
	if(1 == list_length(qual))
	{
		d("serialize_qual 1==length(qual)");

		return serialize_node(indent, (Node*)lfirst(list_head(qual)), nwc_cb, nwoc_cb);
	}
	else
	{
		BoolExpr	expr;

		d("serialize_qual 1<length(qual)");

		expr.xpr.type	= T_BoolExpr;
		expr.boolop		= AND_EXPR;
		expr.args		= qual;
		expr.location	= -1; /* unknown */
		return serialize_node(indent, (Node*)&expr, nwc_cb, nwoc_cb);
	}

	return "";
}
