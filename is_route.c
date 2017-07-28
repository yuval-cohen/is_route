
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODE_NAME	30
#define READ_BUF_SIZE	256
#define READ_BLOCK_LEN	100
#define MAX_LINE_LEN	((MAX_NODE_NAME*2)+10)


typedef struct _Node 
{
	char name[MAX_NODE_NAME+1];
	_Node *route; /* pointer to the next note in the SAME route */
	_Node *next; /* pointer to the next ROUTE */
	
} Node;

/* program's return codes */
typedef enum
{
	RC_NO_ERROR,
	RC_EOF,
	RC_NO_DELIMITER,
	RC_STR_TOO_LONG,
	RC_NO_MEM,
	RC_NO_EXIST,
	RC_FILE_NOT_FOUND
	
} RETURN_CODE;

/* 
Linked list of all routes, each route is a linked list by itself:

e.g.
  
RoutesList->Melbourne->Jerusalem->Boston->NULL
                |          |         |
              Sydney     Haifa     Miami
                |          |         |
               NULL     Tel-Aviv  New-York
                           |         |
                          NULL   Los-Angeles
                                     |
                                    NULL
*/
static Node *RoutesList = NULL;

/* buffer for read from input file */
static char ReadBuffer[READ_BUF_SIZE] = {0};

static RETURN_CODE build_routes_list (FILE *file);
static RETURN_CODE read_next_line (FILE *file, char *line);
static RETURN_CODE extract_names_from_line (const char *line, char *name_1, char *name_2);
static RETURN_CODE add_pair_to_routes_list (char *name_1, char *name_2);
static void find_node_in_routes_list (const char *name, Node **node, Node **route);
static Node *add_new_route (const char *name);
static RETURN_CODE add_node_to_route (const char *name, Node *route);
static void merge_routes (Node *route_1, Node *route_2);
static RETURN_CODE is_route(const char *name_1, const char *name_2, int *result);
static void free_routes_list (void);


static RETURN_CODE read_next_line (FILE *file, char *line)
{
	char *new_line_ptr;
	size_t read_bytes, cur_length;

	new_line_ptr = strstr(ReadBuffer, "\n");
	if (new_line_ptr == NULL)
	{
		/* read another block from file into ReadBuffer */
		cur_length = strlen(ReadBuffer);
		if ((cur_length +1 + READ_BLOCK_LEN) > READ_BUF_SIZE)
		{
			return RC_NO_DELIMITER;
		}
		read_bytes = fread(ReadBuffer+strlen(ReadBuffer), 1, READ_BLOCK_LEN, file);
		new_line_ptr = strstr(ReadBuffer, "\n");
		if ((read_bytes == 0) || (new_line_ptr == NULL))
		{
			return RC_EOF;
		}
		*(ReadBuffer+cur_length+read_bytes) = '\0';
	}
	/* copy next line into from ReadBuffer into line */
	strncpy(line, ReadBuffer, new_line_ptr-ReadBuffer);
	line[new_line_ptr-ReadBuffer] = '\0';

	/* erase it from ReadBuffer - can be optimized */
	memmove(ReadBuffer, new_line_ptr+1, strlen(new_line_ptr+1)+1);
	
	return RC_NO_ERROR;
}

static RETURN_CODE extract_names_from_line (const char *line, char *name_1, char *name_2)
{
	RETURN_CODE ret_code = RC_NO_ERROR;
	const char *delimiter;
	
	/* 
	ASSUME each line is in the following format: 
	<name>,<name><NEW-LINE>
	(1) no spaces
	(2) New-line is '\n'
	(3) no spaces in names, use hyphen instead (e.g. tel-aviv)
	(4) name are case sensitive
	(5) name length is limited to 30 characters
	*/

	delimiter = strstr(line, ",");
	if (delimiter == NULL)
	{
		ret_code = RC_NO_DELIMITER;
	}
	else if ((delimiter - line > MAX_NODE_NAME) || strlen(delimiter+1) > MAX_NODE_NAME)
	{
		/* node's name too long */
		ret_code = RC_STR_TOO_LONG;
	}
	else
	{
		/* output names to name_1 and name_2 */
		strncpy(name_1, line, delimiter-line);
		name_1[delimiter-line] = '\0';
		strcpy(name_2, delimiter+1);
	}

	return ret_code;
}

static void find_node_in_routes_list (const char *name, Node **node, Node **route)
{
	Node *_route, *_node;

	/* find note by name and output its node pointer and its route pointer (NULL if not found) */
	*node = *route = NULL;
	for (_route = RoutesList; _route != NULL; _route = _route->next)
	{
		for (_node = _route; _node != NULL; _node = _node->route)
		{
			if (strcmp(_node->name, name) == 0)
			{
				*node = _node;
				*route = _route;
			}
		}
	}
}

static Node *add_new_route (const char *name)
{
	Node *new_route;

	/* add a new node (by name) as a new route */
	new_route = (Node*)malloc(sizeof(Node));
	if (new_route == NULL)
	{
		return NULL;
	}
	
	strcpy(new_route->name, name);
	new_route->route = NULL;

	new_route->next = RoutesList;
	RoutesList = new_route;

	return new_route;
}

static RETURN_CODE add_node_to_route (const char *name, Node *route)
{
	Node *new_node;
	
	/* add a new node to an existing route */
	new_node = (Node*)malloc(sizeof(Node));
	if (new_node == NULL)
	{
		return RC_NO_MEM;
	}

	strcpy(new_node->name, name);
	new_node->next = NULL;

	new_node->route = route->route;
	route->route = new_node;

	return RC_NO_ERROR;
}

static void merge_routes (Node *route_1, Node *route_2)
{
	Node *_route;
	
	/* merge two routes into one route in the RoutesList */
	for (_route = RoutesList; _route != route_2; _route = _route->next)
		;
	_route->next = route_2->next;
	route_2->next = NULL;

	for (_route = route_2; _route->route != NULL; _route = _route->route)
		;
	_route->route = route_1->route;
	route_1->route = route_2;
}

static RETURN_CODE add_pair_to_routes_list (char *name_1, char *name_2)
{
	Node *node_1_ptr, *route_1_ptr;
	Node *node_2_ptr, *route_2_ptr;

	find_node_in_routes_list(name_1, &node_1_ptr, &route_1_ptr);
	find_node_in_routes_list(name_2, &node_2_ptr, &route_2_ptr);

	if ((node_1_ptr == NULL) && (node_2_ptr == NULL))
	{
		/* both don't exist - add one as a new route and the other to its route */
		route_1_ptr = add_new_route(name_1);
		add_node_to_route(name_2, route_1_ptr);
	}
	else if (node_1_ptr == NULL)
	{
		/* ONLY #1 doesn't exist - add it to the route of #2 */
		add_node_to_route(name_1, route_2_ptr);
	}
	else if (node_2_ptr == NULL)
	{
		/* ONLY #2 doesn't exist - add it to the route of #1 */
		add_node_to_route(name_2, route_1_ptr);
	}
	else
	{
		/* both already exist */
		if (route_1_ptr != route_2_ptr)
		{
			/* they are NOT in the same route - merge the two routes into one route */
			merge_routes(route_1_ptr, route_2_ptr);
		}
	}

	return RC_NO_ERROR;
}

static RETURN_CODE build_routes_list (FILE *file)
{
	char line[MAX_LINE_LEN];
	char name_1[MAX_NODE_NAME+1], name_2[MAX_NODE_NAME+1];
	RETURN_CODE ret_code = RC_NO_ERROR;

	/* build RoutesList from file */
	while ((ret_code = read_next_line(file, line)) == RC_NO_ERROR)
	{
		ret_code = extract_names_from_line(line, name_1, name_2);
		if (ret_code == RC_NO_ERROR)
		{
			ret_code = add_pair_to_routes_list(name_1, name_2);
		}
	}

	return ret_code;
}

static RETURN_CODE is_route (const char *name_1, const char *name_2, int *result)
{
	Node *node_1, *node_2;
	Node *route_1, *route_2;

	/* check (in already built RoutesList) whether there is a route between name_1 and name_2  */
	find_node_in_routes_list(name_1, &node_1, &route_1);
	if (node_1 == NULL)
	{
		/* name_1 does NOT exist */
		return RC_NO_EXIST;
	}
	find_node_in_routes_list(name_2, &node_2, &route_2);
	if (node_2 == NULL)
	{
		/* name_1 does NOT exist */
		return RC_NO_EXIST;
	}

	/* both exist - check if they are on the same route */
	if (route_1 == route_2)
	{
		*result = 1;
	}
	else
	{
		*result = 0;
	}

	return RC_NO_ERROR;
}

static void free_routes_list (void)
{
	Node *route, *node;

	/* free all the memory allocated for RoutesList */
	for (route = RoutesList; route != NULL; route = route->next)
	{
		node = route->route;
		while (node != NULL)
		{
			/* free nodes in the same route */
			route->route = node->route;
			free(node);
			node = route->route;
		}
	}

	route = RoutesList;
	while (route != NULL)
	{
		/* free routes */
		RoutesList = RoutesList->next;
		free(route);
		route = RoutesList;
	}
}

int main (int argc, char* argv[])
{
	RETURN_CODE ret_code;
	FILE *input_file;
	int result;

	input_file = fopen(argv[1]/*"input.txt"*/, "r");
	if (input_file == NULL)
	{
		/* no file */
		ret_code = RC_FILE_NOT_FOUND;
	}
	else
	{
		ret_code = build_routes_list(input_file);
		if (ret_code == RC_EOF)
		{
			ret_code = is_route(argv[2]/*"melbourne"*/, argv[3]/*"jerusalem"*/, &result);
			if (ret_code == RC_NO_ERROR)
			{
				if (result == 1)
				{
					/* there is route between the two */
					printf("ROUTE\n");
				}
				else
				{
					/* there is NO route between the two */
					printf("NO ROUTE\n");
				}
			}
		}
		free_routes_list();

		fclose(input_file);
	}

	if (ret_code != RC_NO_ERROR)
	{
		printf("ERROR: error code no. %d\n", ret_code);
	}

	return 0;
}
