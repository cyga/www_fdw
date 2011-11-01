#ifndef	JSON_PARSER_H
#define	JSON_PARSER_H
#include	<stdio.h>
#include	<stdlib.h>
#include	"libjson-0.8/json.h"

/* "#ifndef bool" doesn't work here */
#ifndef true
typedef char bool;
#endif

#ifndef true
#define true	((bool) 1)
#endif

#ifndef false
#define false	((bool) 0)
#endif

typedef	struct	JSONNode
{
	json_type	type;
	union {
		char*	val_string;
		int		val_int;
		float	val_float;
		bool	val_null;
		bool	val_bool;
		struct JSONNode*	val_array;
		struct JSONNode*	val_object;
	} val;
	uint32_t	length;	/* size of val_array|object array */
	char*		key;	/* active only if it belongs to object */
} JSONNode;

JSONNode*	json_new_object(void);
JSONNode*	json_new_array(void);
JSONNode*	json_new_value(int type, const char* data, uint32_t length);
JSONNode*	json_new_const(int type);
void*		json_tree_create_structure(int nesting, int is_object);
void*		json_tree_create_data(int type, const char *data, uint32_t length);
void		json_copy_node(JSONNode *from, JSONNode* to);
void		json_append_object(JSONNode *object, char *key, uint32_t key_length, void *obj);
void		json_append_array(JSONNode *array, void *obj);
int			json_tree_append(void *structure, char *key, uint32_t key_length, void *obj);
void		json_free_tree(JSONNode* t);
void		json_print_indent(const int indent);
void		json_print_tree(JSONNode* t, int indent, bool comma);
void		json_parser_init2(json_parser* parser, json_parser_dom* dom);
JSONNode*	json_result_tree(json_parser* parser);

#endif
