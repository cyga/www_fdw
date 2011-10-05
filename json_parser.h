#ifndef	JSON_PARSER_H
#define	JSON_PARSER_H
#include	<stdio.h>
#include	<stdlib.h>
#include	"libjson-0.8/json.h"

#ifndef bool
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

JSONNode*
json_new_object()
{
	JSONNode*	obj	= (JSONNode*)malloc(sizeof(JSONNode));
	obj->type	= JSON_OBJECT_BEGIN;
	obj->length	= 0;
	obj->key	= NULL;
	return	obj;
}
JSONNode*
json_new_array()
{
	JSONNode*	obj	= (JSONNode*)malloc(sizeof(JSONNode));
	obj->type	= JSON_ARRAY_BEGIN;
	obj->length	= 0;
	obj->key	= NULL;
	return	obj;
}
JSONNode*
json_new_value(int type, const char* data, uint32_t length)
{
	int	i;
	JSONNode*	obj	= (JSONNode*)malloc(sizeof(JSONNode));
	obj->type	= type;
	obj->key	= NULL;
	obj->length	= -1;
	switch(type)
	{
        case JSON_STRING:
			obj->val.val_string	= malloc(length*sizeof(char)+1);
			for( i=0; i<length; i++ )
				obj->val.val_string[i]	= data[i];
			obj->val.val_string[i]	= '\0';
			break;
        case JSON_INT:
			obj->val.val_int	= atoi(data);
			break;
        case JSON_FLOAT:
			obj->val.val_float	= atof(data);
			break;
	}
	return	obj;
}
JSONNode*
json_new_const(int type)
{
	JSONNode*	obj	= (JSONNode*)malloc(sizeof(JSONNode));
	obj->type	= type;
	obj->length	= -1;
	switch(type)
	{
        case JSON_NULL:
			obj->val.val_null	= true;
			break;
        case JSON_TRUE:
			obj->val.val_bool	= true;
			break;
        case JSON_FALSE:
			obj->val.val_bool	= false;
			break;
	}
	return	obj;
}

void*
json_tree_create_structure(int nesting, int is_object)
{
	if (is_object)
			return json_new_object();
	else
			return json_new_array();
}

void*
json_tree_create_data(int type, const char *data, uint32_t length)
{
	switch (type) {
		case JSON_STRING:
		case JSON_INT:
		case JSON_FLOAT:
				return json_new_value(type, data, length);
		case JSON_NULL:
		case JSON_TRUE:
		case JSON_FALSE:
				return json_new_const(type);
	}
	return	NULL;
}

void
json_copy_node(JSONNode *from, JSONNode* to)
{
	to->type	= from->type;
	to->val		= from->val;
	to->length	= from->length;
	to->key		= from->key;
}

void
json_append_object(JSONNode *object, char *key, uint32_t key_length, void *obj)
{
	int	i,j;
	JSONNode*	obj_ptr	= malloc( (object->length+1) * sizeof(JSONNode) );

	/* copy current elements */
	for( i=0; i<object->length; i++ )
		json_copy_node(&(object->val.val_object[i]), &(obj_ptr[i]));
	/* copy new element */
	json_copy_node((JSONNode*)obj, &(obj_ptr[i]));
	/* set key name (copy string) */
	obj_ptr[i].key	= (char*)(malloc(key_length*sizeof(char) + 1));
	for( j=0; j<key_length; j++ )
		obj_ptr[i].key[j]	= key[j];
	obj_ptr[i].key[j]	= '\0';
	/* free old array */
	if(object->length) free(object->val.val_object);
	/* setup new array */
	object->val.val_object	= obj_ptr;
	/* increment array length */
	object->length++;
}

void
json_append_array(JSONNode *array, void *obj)
{
	int	i;

	JSONNode*	arr_ptr	= malloc( (array->length+1) * sizeof(JSONNode) );
	/* copy current elements */
	for( i=0; i<array->length; i++ )
		json_copy_node(&(array->val.val_array[i]), &(arr_ptr[i]));
	/* copy new element */
	json_copy_node((JSONNode*)obj, &(arr_ptr[i]));
	/* free old array */
	if(array->length) free(array->val.val_object);
	/* setup new array */
	array->val.val_array	= arr_ptr;
	/* increment array length */
	array->length++;
}

int
json_tree_append(void *structure, char *key, uint32_t key_length, void *obj)
{
        if (key != NULL) {
                JSONNode *object = structure;
                json_append_object(object, key, key_length, obj);
        } else {
                JSONNode *array = structure;
                json_append_array(array, obj);
        }

		return	1;
}

void
json_free_tree(JSONNode* t)
{
	int	i	= 0;

	if(t->key) free(t->key);

	switch (t->type) {
		case JSON_OBJECT_BEGIN:
			for( i=0; i<t->length; i++ )
				json_free_tree(&(t->val.val_object[i]));
			free(t->val.val_object);
			break;
		case JSON_ARRAY_BEGIN:
			for( i=0; i<t->length; i++ )
				json_free_tree(&(t->val.val_array[i]));
			free(t->val.val_array);
			break;
		case JSON_STRING:
			if(t->val.val_string)	free(t->val.val_string);
			break;
		/* nothing to do */
		case JSON_INT:
		case JSON_FLOAT:
		case JSON_NULL:
		case JSON_TRUE:
		case JSON_FALSE:
		/* not used, here for removing warning */
		case JSON_NONE:
		case JSON_KEY:
		case JSON_ARRAY_END:
		case JSON_OBJECT_END:
			break;
	}
}

void
json_print_indent(const int indent)
{
	int	i;
	for( i=0; i<indent; i++ )
		printf("\t");
}

void
json_print_tree(JSONNode* t, int indent, bool comma)
{
	int	i	= 0;

	switch (t->type) {
		case JSON_OBJECT_BEGIN:
			json_print_indent(indent);
			if(t->key)
				printf("\"%s\": ", t->key);
			printf("{\n");
			for( i=0; i<t->length; i++ )
				json_print_tree(&(t->val.val_object[i]), indent+1, i<t->length-1);
			json_print_indent(indent);
			printf("}%s\n", comma ? "," : "" );
			break;
		case JSON_ARRAY_BEGIN:
			json_print_indent(indent);
			if(t->key)
				printf("\"%s\": ", t->key);
			printf("[\n");
			for( i=0; i<t->length; i++ )
				json_print_tree(&(t->val.val_array[i]), indent+1, i<t->length-1);
			json_print_indent(indent);
			printf("]%s\n", comma ? "," : "" );
			break;
		case JSON_STRING:
			json_print_indent(indent);
			if(t->key)
				printf("\"%s\": ", t->key);
			printf("\"%s\"", t->val.val_string);
			if(comma) printf(",");
			printf("\n");
			break;
		case JSON_INT:
			json_print_indent(indent);
			if(t->key)
				printf("\"%s\": ", t->key);
			printf("%i", t->val.val_int);
			if(comma) printf(",");
			printf("\n");
			break;
		case JSON_FLOAT:
			json_print_indent(indent);
			if(t->key)
				printf("\"%s\": ", t->key);
			printf("%f", t->val.val_float);
			if(comma) printf(",");
			printf("\n");
			break;
		case JSON_NULL:
			json_print_indent(indent);
			if(t->key)
				printf("\"%s\": ", t->key);
			printf("null");
			if(comma) printf(",");
			printf("\n");
			break;
		case JSON_TRUE:
			json_print_indent(indent);
			if(t->key)
				printf("\"%s\": ", t->key);
			printf("true");
			if(comma) printf(",");
			printf("\n");
			break;
		case JSON_FALSE:
			json_print_indent(indent);
			if(t->key)
				printf("\"%s\": ", t->key);
			printf("false");
			if(comma) printf(",");
			printf("\n");
			break;
		default:
			printf("ERROR - unknown type: %i\n", t->type);
	}
}

void
json_parser_init2(json_parser* parser, json_parser_dom* dom)
{
	json_parser_dom_init(dom, json_tree_create_structure, json_tree_create_data, json_tree_append);
	json_parser_init(parser, NULL, json_parser_dom_callback, dom);
}

JSONNode*
json_result_tree(json_parser* parser)
{
	return	(JSONNode*)(((json_parser_dom*)(parser->userdata))->stack->val);
}

#endif
