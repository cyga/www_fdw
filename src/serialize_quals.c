#include "serialize_quals.h"

static
char*
serialize_node_with_children_callback_json(char *name, List *params)
{
	/* TODO */
	return "";
}

static
char*
serialize_node_without_children_callback_json(char *name, List *params, char *value)
{
	/* TODO */
	return "";
}

static
char*
serialize_node_with_children_callback_xml(char *name, List *params)
{
	/* TODO */
	return "";
}

static
char*
serialize_node_without_children_callback_xml(Node *node, List *params, char *value)
{
	/* TODO */
	return "";
}

static
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

static
char*
serialize_node(Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb)
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
			char	*lcs	= serialize_node(subnode, nwc_cb, nwoc_cb);

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
			char	*lcs	= serialize_node(subnode, nwc_cb, nwoc_cb);

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
		return nwoc_cb("Const", NULL, serialize_const((Const*)node));
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
 * serialize_qual
 * qual - "implicitly-ANDed qual conditions"
 *
 * TODO:
 * iterate through list of quals
 * create json or xml structure
 * return it back as result
 * this same routine will be used with different callbacks for different types
 *
 */
static
char*
serialize_qual(List *qual, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb)
{
	if (NULL == qual)
		return "";

	if(0 == list_length(qual))
	{
		return "";
	}
	if(1 == list_length(qual))
	{
		d("serialize_qual 1==length");
		return serialize_node((Node*)lfirst(list_head(qual)), nwc_cb, nwoc_cb);
	}
	else
	{
		BoolExpr	expr;

		d("serialize_qual 1<length");

		expr.xpr.type	= T_BoolExpr;
		expr.boolop		= AND_EXPR;
		expr.args		= qual;
		expr.location	= -1; /* unknown */
		return serialize_node((Node*)&expr, nwc_cb, nwoc_cb);
	}

	return "";
}

#endif
