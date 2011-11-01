#ifndef SERIALIZE_QUALS_H
#define SERIALIZE_QUALS_H

/* TODO: check what we need from following includes: */
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

typedef char* (*SerializeNodeWithChildrenCallback)(char *name, List *params);
typedef char* (*SerializeNodeWithoutChildrenCallback)(char *name, List *params, char *value);

static
char*
serialize_node_with_children_callback_json(char *name, List *params);

static
char*
serialize_node_without_children_callback_json(char *name, List *params, char *value);

static
char*
serialize_node_with_children_callback_xml(char *name, List *params);

static
char*
serialize_node_without_children_callback_xml(Node *node, List *params, char *value);

static
char*
serialize_const(Const *c);

char*
serialize_node(Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb);

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
char*
serialize_qual(List *qual, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb);

#endif
