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

/* read description in header file (to keep in single place) */
void
serialize_node_with_children_callback_json(int *indent, char *name, List *params, StringInfo prefix, StringInfo suffix)
{
	char indent1 = next_indent(*indent),
		indent2 = next_indent(indent1),
		*p, *pname;
	ListCell *lc;
	unsigned char idx;
	bool first;

	d("serialize_node_with_children_callback_json: indent - %i, name - '%s'", *indent, name);

	appendStringInfo(prefix, "%s{%s%s\"name\":%s\"%s\",%s%s\"params\"%s:%s[%s",
					 get_indent(*indent),
					 get_newline(*indent),
					 get_indent(indent1),
					 get_space(*indent),
					 name,
					 get_newline(*indent),
					 get_indent(indent1),
					 get_space(*indent),
					 get_space(*indent),
					 get_newline(*indent));

	first = true;
	idx = 0;
	foreach (lc, params)
	{
		p = (char*)lfirst(lc);

		if(0 == idx) {
			pname = p;
			idx++;
		}
		else
		{
			if(first)
				first = false;
			else
				appendStringInfo(prefix, ",%s", get_newline(*indent));

			appendStringInfo(prefix, "%s\"%s\":%s\"%s\"",
							 get_indent(indent2),
							 pname,
							 get_space(*indent),
							 p);

			idx = 0;
		}
	}
	if(!first)
		appendStringInfo(prefix, "%s", get_newline(*indent) );

	appendStringInfo(prefix, "%s],%s%s\"children\":%s[%s",
					 get_indent(indent1),
					 get_newline(*indent),
					 get_indent(indent1),
					 get_space(*indent),
					 get_newline(*indent));

	appendStringInfo(suffix, "%s%s]%s%s}",
					 get_newline(*indent),
					 get_indent(indent1),
					 get_newline(*indent),
					 get_indent(*indent));

	*indent = indent2;
}

/* read description in header file (to keep in single place) */
char*
serialize_node_without_children_callback_json(int indent, char *name, List *params, char *value)
{
	StringInfoData str;
	char indent1 = next_indent(indent),
		indent2 = next_indent(indent1),
		*p, *pname;
	ListCell *lc;
	unsigned char idx;
	bool first;

	d("serialize_node_without_children_callback_json: indent - %i, name - '%s', value - '%s'", indent, name, value);

	initStringInfo(&str);
	appendStringInfo(&str, "%s{%s%s\"name\":%s\"%s\",%s%s\"params\"%s:%s[%s",
					 get_indent(indent),
					 get_newline(indent),
					 get_indent(indent1),
					 get_space(indent),
					 name,
					 get_newline(indent),
					 get_indent(indent1),
					 get_space(indent),
					 get_space(indent),
					 get_newline(indent));

	first = true;
	idx = 0;
	foreach (lc, params)
	{
		p = (char*)lfirst(lc);

		if(0 == idx)
		{
			pname = p;
			idx++;
		}
		else
		{
			if(first)
				first = false;
			else
				appendStringInfo(&str, ",%s", get_newline(indent));

			appendStringInfo(&str, "%s\"%s\":%s\"%s\"",
							 get_indent(indent2),
							 pname,
							 get_space(indent),
							 p);

			idx = 0;
		}
	}
	if(!first)
		appendStringInfo(&str, "%s", get_newline(indent) );

	appendStringInfo(&str, "%s],%s%s\"value\":%s\"%s\"%s%s}",
					 get_indent(indent1),
					 get_newline(indent),
					 get_indent(indent1),
					 get_space(indent),
					 value,
					 get_newline(indent),
					 get_indent(indent));

	return str.data;
}

/* read description in header file (to keep in single place) */
char*
serialize_list_separator_callback_json(int indent)
{
	StringInfoData str;

	initStringInfo(&str);
	appendStringInfo(&str, ",%s", get_newline(indent));

	return str.data;
}

/* read description in header file (to keep in single place) */
void
serialize_node_with_children_callback_xml(int *indent, char *name, List *params, StringInfo prefix, StringInfo suffix)
{
	char indent1 = next_indent(*indent);
	char indent2 = next_indent(indent1);
	ListCell *lc;
	char *p, *pname;
	unsigned char idx;

	d("serialize_node_with_children_callback_xml: indent - %i, name - '%s'", *indent, name);

	appendStringInfo(prefix, "%s<node name=\"%s\">%s%s<params>%s",
					 get_indent(*indent),
					 name,
					 get_newline(*indent),
					 get_indent(indent1),
					 get_newline(*indent));

	idx = 0;
	foreach (lc, params)
	{
		p = (char*)lfirst(lc);

		if(0 == idx)
		{
			pname = p;
			idx++;
		}
		else
		{
			appendStringInfo(prefix, "%s<param name=\"%s\" value=\"%s\"/>%s",
							 get_indent(indent2),
							 pname,
							 p,
							 get_newline(*indent));
			idx = 0;
		}
	}

	appendStringInfo(prefix, "%s</params>%s%s<children>%s",
					 get_indent(indent1),
					 get_newline(*indent),
					 get_indent(indent1),
					 get_newline(*indent));

	appendStringInfo(suffix, "%s</children>%s%s</node>%s",
					 get_indent(indent1),
					 get_newline(*indent),
					 get_indent(*indent),
					 get_newline(*indent));

	*indent = indent2;
}

/* read description in header file (to keep in single place) */
char*
serialize_node_without_children_callback_xml(int indent, char *name, List *params, char *value)
{
	StringInfoData str;
	char indent1 = next_indent(indent);
	char indent2 = next_indent(indent1);
	ListCell *lc;
	char *p, *pname;
	unsigned char idx;

	d("serialize_node_without_children_callback_xml: indent - %i, name - '%s', value - '%s'", indent, name, value);

	initStringInfo(&str);
	appendStringInfo(&str, "%s<node name=\"%s\" value=\"%s\">%s%s<params>%s",
					 get_indent(indent),
					 name,
					 value,
					 get_newline(indent),
					 get_indent(indent1),
					 get_newline(indent));

	idx = 0;
	foreach (lc, params)
	{
		p = (char*)lfirst(lc);

		if(0 == idx)
		{
			pname = p;
			idx++;
		}
		else
		{
			appendStringInfo(&str, "%s<param name=\"%s\" value=\"%s\"/>%s",
							 get_indent(indent2),
							 pname,
							 p,
							 get_newline(indent));
			idx = 0;
		}
	}

	appendStringInfo(&str, "%s</params>%s%s</node>%s",
					 get_indent(indent1),
					 get_newline(indent),
					 get_indent(indent),
					 get_newline(indent));

	return str.data;
}

/* read description in header file (to keep in single place) */
char*
serialize_list_separator_callback_xml(int indent)
{
	return get_newline(indent);
}

/* read description in header file (to keep in single place) */
char*
serialize_const(Const *c)
{
	StringInfoData str;

	d("serialize_const");

	initStringInfo(&str);

	if(c->constisnull)
		return "null";

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

static
char*
oid_to_string(Oid oid)
{
	/* 32 is over than represenation unsigned int in decimal chars */
	char *r = (char*)palloc(32*sizeof(char));
	sprintf( r, "%u", oid);
	return r;
}

static
char*
int_to_string(int i)
{
	/* 32 is over than represenation unsigned int in decimal chars */
	char *r = (char*)palloc(32*sizeof(char));
	sprintf( r, "%i", i);
	return r;
}

static
char*
attrnumber_to_string(AttrNumber attrn)
{
	return int_to_string((int)attrn);
}

static
char*
index_to_string(Index idx)
{
	/* 32 is over than represenation unsigned int in decimal chars */
	char *r = (char*)palloc(32*sizeof(char));
	sprintf( r, "%u", idx);
	return r;
}

static
char*
bool_to_string(bool b)
{
	char *r = (char*)palloc(2*sizeof(char));
	r[0] = b ? 't' : 'f';
	r[1] = '\0';
	return r;
}

static
char*
serialize_node_list(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
{
	ListCell   *lc;
	List	   *children = ((List*)node);
	StringInfoData prefix, suffix, childrens;
	bool		first = true;
	int			indentc = indent;
	List		*params = NULL;
	char		*sep = NULL;

	d("serialize_node List");

	initStringInfo(&prefix);
	initStringInfo(&suffix);
	initStringInfo(&childrens);

	/* no params for List */
	/*params	= lappend(params, (void*));*/
	nwc_cb(&indentc, "List", params, &prefix, &suffix);

	foreach (lc, children)
	{
		Node	*child	= lfirst(lc);

		if(!first)
		{
			if(NULL == sep)
				sep = ls_cb(indentc);

			appendStringInfoString(&childrens, sep);
		}
		else
			first = false;

		appendStringInfoString(&childrens, serialize_node(indentc, child, nwc_cb, nwoc_cb, ls_cb));
	}

	appendBinaryStringInfo(&prefix, childrens.data, childrens.len);
	appendBinaryStringInfo(&prefix, suffix.data, suffix.len);

	return prefix.data;
}

static
char*
serialize_node_opexpr(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
{
	OpExpr	   *op = (OpExpr *)node;
	ListCell   *lc;
	List	   *params = NULL;
	StringInfoData prefix, suffix, childrens;
	bool		first = true;
	int			indentc = indent;
	char		*sep = NULL;

	d("serialize_node OpExpr");

	initStringInfo(&prefix);
	initStringInfo(&suffix);
	initStringInfo(&childrens);

	/* create parameters */
	params = lappend(params, "opno");
	params = lappend(params, oid_to_string(op->opno));
	params = lappend(params, "opfuncid");
	params = lappend(params, oid_to_string(op->opfuncid));
	params = lappend(params, "opresulttype");
	params = lappend(params, oid_to_string(op->opresulttype));
	params = lappend(params, "opretset");
	params = lappend(params, bool_to_string(op->opretset));
	params = lappend(params, "opcollid");
	params = lappend(params, oid_to_string(op->opcollid));
	params = lappend(params, "inputcollid");
	params = lappend(params, oid_to_string(op->inputcollid));
	params = lappend(params, "location");
	params = lappend(params, int_to_string(op->location));
	nwc_cb(&indentc, "OpExpr", params, &prefix, &suffix);

	foreach (lc, op->args)
	{
		Node	*child	= lfirst(lc);

		if(!first)
		{
			if(NULL == sep)
				sep = ls_cb(indentc);

			appendStringInfoString(&childrens, sep);
		}
		else
			first = false;

		appendStringInfoString(&childrens, serialize_node(indentc, child, nwc_cb, nwoc_cb, ls_cb));
	}

	appendBinaryStringInfo(&prefix, childrens.data, childrens.len);
	appendBinaryStringInfo(&prefix, suffix.data, suffix.len);

	return prefix.data;
}

static
char*
serialize_node_funcexpr(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
{
	FuncExpr   *f = (FuncExpr *)node;
	ListCell   *lc;
	List	   *params = NULL;
	StringInfoData prefix, suffix, childrens;
	bool		first = true;
	int			indentc = indent;
	char		*sep = NULL;

	d("serialize_node FuncExpr");

	initStringInfo(&prefix);
	initStringInfo(&suffix);
	initStringInfo(&childrens);

	/* create parameters */
	params = lappend(params, "funcid");
	params = lappend(params, oid_to_string(f->funcid));
	params = lappend(params, "funcresulttype");
	params = lappend(params, oid_to_string(f->funcresulttype));
	params = lappend(params, "funcretset");
	params = lappend(params, bool_to_string(f->funcretset));
	/* skip funcformat */
	params = lappend(params, "funccollid");
	params = lappend(params, oid_to_string(f->funccollid));
	params = lappend(params, "inputcollid");
	params = lappend(params, oid_to_string(f->inputcollid));
	params = lappend(params, "location");
	params = lappend(params, int_to_string(f->location));
	nwc_cb(&indentc, "FuncExpr", params, &prefix, &suffix);

	foreach (lc, f->args)
	{
		Node	*child	= lfirst(lc);

		if(!first)
		{
			if(NULL == sep)
				sep = ls_cb(indentc);

			appendStringInfoString(&childrens, sep);
		}
		else
			first = false;

		appendStringInfoString(&childrens, serialize_node(indentc, child, nwc_cb, nwoc_cb, ls_cb));
	}

	appendBinaryStringInfo(&prefix, childrens.data, childrens.len);
	appendBinaryStringInfo(&prefix, suffix.data, suffix.len);

	return prefix.data;
}

static
char*
serialize_node_boolexpr(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
{
	BoolExpr   *op = (BoolExpr*)node;
	ListCell   *lc;
	List	   *params = NULL;
	StringInfoData prefix, suffix, childrens;
	bool		first = true;
	int			indentc = indent;
	char		*sep = NULL,
		*boolop = NULL;

	d("serialize_node BoolExpr");

	initStringInfo(&prefix);
	initStringInfo(&suffix);
	initStringInfo(&childrens);

	/* create parameters */
	switch(op->boolop)
	{
		case AND_EXPR:
			boolop = "AND";
			break;
		case OR_EXPR:
			boolop = "OR";
			break;
		case NOT_EXPR:
			boolop = "NOT";
			break;
		default:
			boolop = "ERROR";
	}
	params = lappend(params, "boolop");
	params = lappend(params, boolop);
	params = lappend(params, "location");
	params = lappend(params, int_to_string(op->location));
	nwc_cb(&indentc, "BoolExpr", params, &prefix, &suffix);

	foreach (lc, op->args)
	{
		Node	*child	= lfirst(lc);

		if(!first)
		{
			if(NULL == sep)
				sep = ls_cb(indentc);

			appendStringInfoString(&childrens, sep);
		}
		else
			first = false;

		appendStringInfoString(&childrens, serialize_node(indentc, child, nwc_cb, nwoc_cb, ls_cb));
	}

	appendBinaryStringInfo(&prefix, childrens.data, childrens.len);
	appendBinaryStringInfo(&prefix, suffix.data, suffix.len);

	return prefix.data;
}

static
char*
serialize_node_nulltest(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
{
	NullTest   *p = (NullTest*)node;
	List	   *params = NULL;
	StringInfoData prefix, suffix, childrens;
	int			indentc = indent;
	char		*nulltesttype = NULL;

	d("serialize_node NullTest");

	initStringInfo(&prefix);
	initStringInfo(&suffix);
	initStringInfo(&childrens);

	/* create parameters */
	switch(p->nulltesttype)
	{
		case IS_NULL:
			nulltesttype = "IS_NULL";
			break;
		case IS_NOT_NULL:
			nulltesttype = "IS_NOT_NULL";
			break;
		default:
			nulltesttype = "ERROR";
	}
	params = lappend(params, "nulltesttype");
	params = lappend(params, nulltesttype);
	params = lappend(params, "argisrow");
	params = lappend(params, bool_to_string(p->argisrow));
	nwc_cb(&indentc, "NullTest", params, &prefix, &suffix);

	appendStringInfoString(&prefix, serialize_node(indentc, (Node*)p->arg, nwc_cb, nwoc_cb, ls_cb));

	appendBinaryStringInfo(&prefix, suffix.data, suffix.len);

	return prefix.data;
}

static
char*
serialize_node_var(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
{
	Var *v = (Var*)node;
	List	   *params = NULL;

	d("serialize_node Var");

	/* create parameters */
	params = lappend(params, "varno");
	params = lappend(params, index_to_string(v->varno));
	params = lappend(params, "varattno");
	params = lappend(params, attrnumber_to_string(v->varattno));
	params = lappend(params, "vartype");
	params = lappend(params, oid_to_string(v->vartype));
	params = lappend(params, "vartypmod");
	params = lappend(params, int_to_string(v->vartypmod));
	params = lappend(params, "varcollid");
	params = lappend(params, oid_to_string(v->varcollid));
	params = lappend(params, "varlevelsup");
	params = lappend(params, index_to_string(v->varlevelsup));
	params = lappend(params, "varnoold");
	params = lappend(params, index_to_string(v->varnoold));
	params = lappend(params, "varoattno");
	params = lappend(params, attrnumber_to_string(v->varoattno));
	params = lappend(params, "location");
	params = lappend(params, int_to_string(v->location));

	return nwoc_cb(indent, "Var", params, "");
}

static
char*
serialize_node_param(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
{
	Param	*p = (Param*)node;
	List	*params = NULL;
	char	*pstr;

	d("serialize_node Param");

	/* create parameters */
	switch(p->paramkind)
	{
		case PARAM_EXTERN:
			pstr = "PARAM_EXTERN";
			break;
		case PARAM_EXEC:
			pstr = "PARAM_EXEC";
		case PARAM_SUBLINK:
			pstr = "PARAM_SUBLINK";
			break;
		default:
			pstr = "ERROR";
	}
	params = lappend(params, "paramkind");
	params = lappend(params, pstr);
	params = lappend(params, "paramid");
	params = lappend(params, int_to_string(p->paramid));
	params = lappend(params, "paramtype");
	params = lappend(params, oid_to_string(p->paramtype));
	params = lappend(params, "paramtypmod");
	params = lappend(params, int_to_string(p->paramtypmod));
	params = lappend(params, "paramcollid");
	params = lappend(params, oid_to_string(p->paramcollid));
	params = lappend(params, "location");
	params = lappend(params, int_to_string(p->location));

	return nwoc_cb(indent, "Var", params, "");
}

/* read description in header file (to keep in single place) */
char*
serialize_node(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
{
	d("serialize_node");

	if (NULL == node)
		return "";

	if(IsA(node, List))
	{
		return serialize_node_list(indent, node, nwc_cb, nwoc_cb, ls_cb);
	}
	else if (IsA(node, OpExpr))
	{
		return serialize_node_opexpr(indent, node, nwc_cb, nwoc_cb, ls_cb);
	}
	else if (IsA(node, FuncExpr))
	{
		return serialize_node_funcexpr(indent, node, nwc_cb, nwoc_cb, ls_cb);
	}
	else if(IsA(node, BoolExpr))
	{
		return serialize_node_boolexpr(indent, node, nwc_cb, nwoc_cb, ls_cb);
	}
	else if(IsA(node, NullTest))
	{
		return serialize_node_nulltest(indent, node, nwc_cb, nwoc_cb, ls_cb);
	}
	else if(IsA(node, Const))
	{
		d("serialize_node Const");
		return nwoc_cb(indent, "Const", NULL, serialize_const((Const*)node));
	}
	else if(IsA(node, Var))
	{
		return serialize_node_var(indent, node, nwc_cb, nwoc_cb, ls_cb);
	}
	else if(IsA(node, Param))
	{
		return serialize_node_param(indent, node, nwc_cb, nwoc_cb, ls_cb);
	}
/*
 TODO: implement following tags here (tags for primitive nodes):
	T_Alias = 300,
	T_RangeVar,
	T_Expr,
	+T_Var,
	+T_Const,
	+T_Param,
	T_Aggref,
	T_WindowFunc,
	T_ArrayRef,
	+T_FuncExpr,
	T_NamedArgExpr,
	+T_OpExpr,
	T_DistinctExpr,
	T_NullIfExpr,
	T_ScalarArrayOpExpr,
	+T_BoolExpr,
	T_SubLink,
	T_SubPlan,
	T_AlternativeSubPlan,
	T_FieldSelect,
	T_FieldStore,
	T_RelabelType,
	T_CoerceViaIO,
	T_ArrayCoerceExpr,
	T_ConvertRowtypeExpr,
	T_CollateExpr,
	T_CaseExpr,
	T_CaseWhen,
	T_CaseTestExpr,
	T_ArrayExpr,
	T_RowExpr,
	T_RowCompareExpr,
	T_CoalesceExpr,
	T_MinMaxExpr,
	T_XmlExpr,
	+T_NullTest,
	T_BooleanTest,
	T_CoerceToDomain,
	T_CoerceToDomainValue,
	T_SetToDefault,
	T_CurrentOfExpr,
	T_TargetEntry,
	T_RangeTblRef,
	T_JoinExpr,
	T_FromExpr,
	T_IntoClause,
*/
	else
	{
		List	   *params = NULL;

		d("serialize_node else");

		/* create parameters */
		params = lappend(params, "type");
		params = lappend(params, int_to_string(nodeTag(node)));

		return nwoc_cb(indent, "UNHANDLED", params, "");
	}

	return "";
}

/* read description in header file (to keep in single place) */
char*
serialize_quals(bool human_readable, List *qual, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb)
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

		return serialize_node(indent, (Node*)lfirst(list_head(qual)), nwc_cb, nwoc_cb, ls_cb);
	}
	else
	{
		BoolExpr	expr;

		d("serialize_qual 1<length(qual)");

		expr.xpr.type	= T_BoolExpr;
		expr.boolop		= AND_EXPR;
		expr.args		= qual;
		expr.location	= -1; /* unknown */
		return serialize_node(indent, (Node*)&expr, nwc_cb, nwoc_cb, ls_cb);
	}

	return "";
}
