#ifndef SERIALIZE_QUALS_H
#define SERIALIZE_QUALS_H

#include "postgres.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "commands/explain.h"

/* set_indent_string
 * set indent string, which is used for forming indent
 */
void
set_indent_string(char *str);

/*
 * callback types 
 * indent - indent level, -1 - no indent (not "human readible", preserves spacing)
 *   indent is changed by callback, in order to show next expected intend
 * name - node name
 * params - consists of variables of Property type 
 * prefix,suffix - output results, prefix and suffix
 * node without children return whole string,
 * prefix and suffix don't make sense for it
*/
typedef void (*SerializeNodeWithChildrenCallback)(int *indent, char *name, List *params, StringInfo prefix, StringInfo suffix);
typedef char* (*SerializeNodeWithoutChildrenCallback)(int indent, char *name, List *params, char *value);
typedef char* (*SerializeListSeparatorCallback)(int indent);

/* serialize_node_with_children_callback_json
 * prefix := '{"name":name,children:['
 * suffix := ']}'
 */
void
serialize_node_with_children_callback_json(int *indent, char *name, List *param, StringInfo prefix, StringInfo suffix);

/* serialize_node_with_children_callback_json
 * check indent meaning in callbacks description
 * returns '{"name":"NAME","value":"VALUE"}'
 */
char*
serialize_node_without_children_callback_json(int indent, char *name, List *params, char *value);

/* serialize_list_separator_callback_json
 * returns separator string between list members
 * returns ","
 */
char*
serialize_list_separator_callback_json(int indent);

/* serialize_node_with_children_callback_json
 * check indent meaning in callbacks description
 * prefix := '<node name="name"><params><param name="NAME" value="VALUE">...</params><children>'
 * suffix := '</children></node>'
 */
void
serialize_node_with_children_callback_xml(int *indent, char *name, List *params, StringInfo prefix, StringInfo suffix);

/* serialize_node_with_children_callback_json
 * check indent meaning in callbacks description
 * returns '<node name="name" value="value"><params><param name="NAME" value="VALUE"/>...</params></node>'
 */
char*
serialize_node_without_children_callback_xml(int indent, char *name, List *params, char *value);

/* serialize_list_separator_callback_xml
 * returns separator string between list members
 * returns ""
 */
char*
serialize_list_separator_callback_xml(int indent);

/* serialize_const
 * returns stirng representation of the specified const
 */
char*
serialize_const(Const *c);

/* serialize_const
 * returns stirng representation of the specified node
 * indent - indent level, -1 - no indent at all
 * callbacks - for having 1 funciton for json/xml types
 */
char*
serialize_node(int indent, Node *node, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb);

/*
 * serialize_qual
 * qual - "implicitly-ANDed qual conditions"
 *
 * iterate through list of quals
 * create json or xml structure
 * return it back as result
 * this same routine will be used with different callbacks
 * for different types (json, xml)
 */
char*
serialize_quals(bool human_readable, List *qual, SerializeNodeWithChildrenCallback nwc_cb, SerializeNodeWithoutChildrenCallback nwoc_cb, SerializeListSeparatorCallback ls_cb);

#endif
