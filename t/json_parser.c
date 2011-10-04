#include	"../json_parser.h"

int	main()
{
	const int	chunk	= 1024;
	json_parser_dom json_dom;
	json_parser json_parserr;
	int	ret;
	size_t	read;
	char	buffer[1025]	= {0};

	json_parser_init2(&json_parserr, &json_dom);

	while(read	= fread((void*)buffer, sizeof(char), chunk, stdin))
	{
		ret = json_parser_string(&json_parserr, buffer, read*sizeof(char), NULL);
		if (ret){
			printf("[ERROR] json_parser failed: %d %s\n", ret, buffer);
		}

		if(read < chunk)	break;
	}

	JSONNode*	root	= json_result_tree(&json_parserr);
	json_print_tree(root, 0, false);
	json_free_tree(root);

	return	0;
}
