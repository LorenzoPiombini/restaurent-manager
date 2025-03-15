#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include "json.h"
#include "bst.h"
#include "debug.h"
#include "str_op.h"

int nest_arr = 0; /*global counter for nested array*/
int nest_obj = 0; /*global counter for nested object*/
regex_t regex; /*used to compile regexonly one time */

int init_rgx() 
{
        return regcomp(&regex,JSRGX_OBJ,REG_EXTENDED);
}

unsigned char parse_json_object(char* json_str, JsonPair*** pairs, int* pairs_size)
{
	if(!check_json_input(json_str))
	{
		return 0;
	}
	
	Token** tokens = calloc(1,sizeof(Token*));
	if(!tokens) {
		printf("calloc failed, %s:%d.\n",F,L-2);
		return 0;
	}

	int token_counts = 1, *ptcount = &token_counts;
	if(!lex(json_str, &tokens, ptcount)) {
		printf("lexer failed, %s:%d.\n",F,L-2);
		free_tokens_array(&tokens,*ptcount);
		return 0;
	}

	*pairs = calloc(1,sizeof(JsonPair*));
	if(!*pairs)
	{
		printf("calloc failed, %s:%d",F,L-3);
		return 0;		
	}
	int pcount = 1;
	*pairs_size = pcount;
        if(!create_json_pairs(&tokens, *ptcount, pairs, pairs_size))
	{
		printf("create_json_pairs failed, %s:%d.\n",F,L-2);
		free_json_pairs_array(pairs,*pairs_size);
		return 0;
	}

	free_tokens_array(&tokens,*ptcount);
	return 1;

}

unsigned char lex(char* json, Token*** tokens, int* ptcount)
{
	
	size_t buffer = strlen(json);
	unsigned char in_array = 0, nested_array  = 0, nested_obj = 0, in_obj = 0;
	int i = 0, j = 0, complessive_counter = 0;
	char* original_json = strdup(json);
	for(i = 0; i < buffer; i++) {
		char c = json[i];
		
		if(c == '\"') {
			register int str_b = i + 1; /* skip the quote */
			complessive_counter++;	
			char* temp = &json[str_b];
			int pos = strcspn(&json[str_b],"\"");
			size_t str_l = pos + 1;

			/*allocate memory for the token*/	
			(*tokens)[(*ptcount) - 1]  = calloc(1,sizeof(Token));
			if(!(*tokens)[(*ptcount) - 1])
			{
				printf("calloc failed, %s:%d.\n",F,L-3);
				return 0;
			}

			/*allocate memory for the value (inside token)*/	
			(*tokens)[(*ptcount) - 1 ]->value = calloc(str_l,sizeof(char));
			if(!(*tokens)[(*ptcount) - 1])
			{
				printf("calloc failed, %s:%d.\n",F,L-3);
				return 0;
			}

			(*tokens)[(*ptcount) - 1] ->value[str_l - 1] = '\0';
  
			for(j = 0; j < pos; j++)
				(*tokens)[(*ptcount) - 1] ->value[j] = (&json[str_b])[j], complessive_counter++;

			if(json[str_b - 2] == ':')
			{
				(*tokens)[(*ptcount) - 1]->type = TOKEN_VALUE;
			}
			else if(!in_array)
			{
				(*tokens)[(*ptcount) - 1]->type = TOKEN_KEY;
			}
			else if(in_array && !nested_array && nested_obj == 0)
			{
				(*tokens)[(*ptcount) - 1]->type = TOKEN_ARRAY_ELEMENT;
			}
			else if(in_array && nested_array && nested_obj == 0)	
			{
				(*tokens)[(*ptcount) - 1]->type = TOKEN_NESTED_ARRAY_ELEMENT;
			}
			else if(in_array && !nested_array && nested_obj > 0 )
			{	
				if(json[str_b - 2] == ':')
                        	        (*tokens)[(*ptcount) - 1]->type = TOKEN_VALUE;
        	                else
	                        	(*tokens)[(*ptcount) - 1]->type = TOKEN_KEY;
			}
			else if(in_array && !nested_array && nested_obj > 0 )	
			{
				if(json[str_b - 2] == ':')
                                (*tokens)[(*ptcount) - 1]->type = TOKEN_VALUE;
        	                else if(!in_array)
	                        (*tokens)[(*ptcount) - 1]->type = TOKEN_KEY;
			}

			/*resizing buffer and json string*/
			i = pos;
			json = temp;
			buffer = strlen(json);

			/* we incremente the total count because here we set the pointer 
				to point at the end of the string (key or value) so now the value of
				json[i] is '"' and once we hit the for loop statment it will not be 
				'"' anymore, but it will be the next char */
			complessive_counter++;	

			/*expand the tokens array by one to contain the next data*/
			Token** tokens_n = realloc(*tokens, ++(*ptcount) * sizeof(Token*));
			if(!tokens_n) {
				printf("realloc failed, %s:%d.",F,L-3);
				return 0;	
			}
			*tokens = tokens_n;
			continue;
		}

		if(c == ' ') { 
			complessive_counter++;	
			continue;
		}
		
		if(in_array && (c != ':' && c != '[' && c != ',' && c != ']' && c != '"' && c != '{' && c != '}')) {
			int p = 0, value_size = 0;
			char* temp = &json[i];

			(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
			if(!(*tokens)[(*ptcount) - 1]) {
				printf("calloc failed,%s:%d.\n",F,L-3);
				return 0;
			}
			
			while(temp[p] != ',' && temp[p] != '[' && temp[p] != ']') {	
				value_size++;
				complessive_counter++;	
				p++;
			}
			
			(*tokens)[(*ptcount) - 1]->value = calloc(++value_size,sizeof(char));
			if(!(*tokens)[(*ptcount) - 1]->value) {
				printf("calloc falied, %s:%d.\n",F,L-3);
				return 0;
			}

			(*tokens)[(*ptcount) - 1]->value[value_size - 1] = '\0';
			
			p = 0;	
			while(temp[p] != ',' && temp[p] != '[' && temp[p] != ']') {
				(*tokens)[(*ptcount) - 1]->value[p] = temp[p];
				p++;
			}
				
			(*tokens)[(*ptcount) - 1]->type = nested_array == 1 ? TOKEN_NESTED_ARRAY_ELEMENT : TOKEN_ARRAY_ELEMENT;
			/*check if the last char is the closing "]" or "[" */
			if(temp[p] == ']' || temp[p] == '[')
				i = --p;
			else
				i = p;

			/*resizing buffer and json string*/
			json = temp;
			buffer = strlen(json);

			/*expand the tokens array by one to contain the next data*/
			Token** tokens_n = realloc(*tokens, ++(*ptcount) * sizeof(Token*));
			if(!tokens_n) {
				printf("realloc failed, %s:%d.\n",F,L-3);
				return 0;
			}
			*tokens = tokens_n;
			continue;
		}
			
			
			
			
		switch(c)
		{
			case 'n':
			{
				(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
				if(!(*tokens)[(*ptcount) - 1]) {
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}

				size_t l = strlen(JS_NULL) + 1;	
				complessive_counter+= l - 1;

				(*tokens)[(*ptcount) - 1]->value = calloc(l,sizeof(char));
			       	if(!(*tokens)[(*ptcount) - 1]->value) {
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}
				
				if(snprintf((*tokens)[(*ptcount) - 1]->value,l,"%s",JS_NULL) < 0) {
					printf("snprintf failed %s:%d.\n",F,L-2);
					return 0;
				}	

				(*tokens)[(*ptcount - 1)]->type = TOKEN_NULL;
				
				Token** tokens_n = realloc(*tokens,++(*ptcount) * sizeof(Token*));
				if(!tokens_n) {
					printf("realloc failed, %s:%d.\n",F,L-3);
					return 0;
				}

				*tokens = tokens_n;
				/* skip the all null token in the next iterations*/
				i += l - 2; /* l is the size of JS_NULL + the null char '\0'*/
				break;
			}
			case 't':
			{
				(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
				if(!(*tokens)[(*ptcount) - 1]) {
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}

				size_t l = strlen(JS_TRUE) + 1;	
				complessive_counter+= l - 1;

				(*tokens)[(*ptcount) - 1]->value = calloc(l,sizeof(char));
			       	if(!(*tokens)[(*ptcount) - 1]->value) {
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}
				
				if(snprintf((*tokens)[(*ptcount) - 1]->value,l,"%s",JS_TRUE) < 0) {
					printf("sprintf failed %s:%d.\n",F,L-2);
					return 0;
				}	

				(*tokens)[(*ptcount - 1)]->type = TOKEN_TRUE;
				
				Token** tokens_n = realloc(*tokens,++(*ptcount) * sizeof(Token*));
				if(!tokens_n) {
					printf("realloc failed, %s:%d.\n",F,L-3);
					return 0;
				}

				*tokens = tokens_n;

				/* skip the entire null token in the next iterations*/
				i += l - 2; /* l is the size of JS_NULL + the null char '\0'*/
				break;
			}
			case 'f':
			{
				(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
				if(!(*tokens)[(*ptcount) - 1]) {
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}

				size_t l = strlen(JS_FALSE) + 1;	
				complessive_counter+= l - 1;

				(*tokens)[(*ptcount) - 1]->value = calloc(l,sizeof(char));
			       	if(!(*tokens)[(*ptcount) - 1]->value) {
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}
				
				if(snprintf((*tokens)[(*ptcount) - 1]->value,l,"%s",JS_FALSE) < 0 ) {
					printf("sprintf failed %s:%d.\n",F,L-2);
					return 0;
				}	

				(*tokens)[(*ptcount - 1)]->type = TOKEN_FALSE;
				
				Token** tokens_n = realloc(*tokens,++(*ptcount) * sizeof(Token*));
				if(!tokens_n) {
					printf("realloc failed, %s:%d.\n",F,L-3);
					return 0;
				}
				*tokens = tokens_n;

				/* skip the all null token in the next iterations*/
				i += l - 2; /* l is the size of JS_NULL + the null char '\0'*/
				break;
			
			}	
			case '{':
			{
				if(in_obj) {
					if(nested_obj)
						nested_obj++;
					else
						nested_obj = 1;
				}else {
					in_obj = 1;
				}

				(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
				if(!(*tokens)[(*ptcount - 1)])
				{
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}	
				
				if(nested_obj && !in_array) {
					(*tokens)[(*ptcount - 1)]->type = TOKEN_JSON_NESTED_OBJ_OPEN;

				} else if(nested_obj && in_array) {
					(*tokens)[(*ptcount - 1)]->type = TOKEN_JSON_NESTED_OBJ_OPEN_IN_ARRAY;

				}else if(!nested_obj) {
					(*tokens)[(*ptcount - 1)]->type = TOKEN_JSON_OBJ_OPEN;
				}

				Token** tokens_n = realloc(*tokens,++(*ptcount) * sizeof(Token*));
				if(!tokens_n) {
					printf("realloc failed, %s:%d.\n",F,L-3);
					return 0;
				}

				*tokens = tokens_n;
				complessive_counter++;
				break;
			}
			case '}':
			{	
				(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
				if(!(*tokens)[(*ptcount - 1)])
				{
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}	
				
				if(nested_obj && !in_array) {
					(*tokens)[(*ptcount - 1)]->type = TOKEN_JSON_NESTED_OBJ_CLOSE;
					complessive_counter++;
				}else if(nested_obj && in_array) {
					(*tokens)[(*ptcount - 1)]->type = TOKEN_JSON_NESTED_OBJ_CLOSE_IN_ARRAY;
					complessive_counter++;
				} else if(!nested_obj) {
					(*tokens)[(*ptcount - 1)]->type = TOKEN_JSON_OBJ_CLOSE;
					complessive_counter++;
				}
				/*check if we are at the end of the json object*/
				/*--- if we are we do not have to allocate more memory */
				/*--- for the tokens array*/
				if(!nested_obj)
				{
					size_t original_buffer = strlen(original_json);
					if(original_buffer == complessive_counter) 
					{
						/*setting in_obj to false is irrelevant but for consistency */
						in_obj = 0;
						/*create the end token*/
						Token** tokens_n = realloc(*tokens,++(*ptcount) * sizeof(Token*));
						if(!tokens_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
						*tokens = tokens_n;

						(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
						if(!(*tokens)[(*ptcount) - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*tokens)[(*ptcount) - 1]->value = calloc(2,sizeof(char));
						if(!(*tokens)[(*ptcount - 1)])
						{
							printf("calloc failed %s:%d.\n",F,L-3);
							return 0;
						}

						(*tokens)[(*ptcount) - 1]->value[0] = '0';
						(*tokens)[(*ptcount) - 1]->value[1] = '\0';
						(*tokens)[(*ptcount) - 1]->type = TOKEN_END;
						free(original_json); 
						return 1;
					}
				}

				if(nested_obj)
					nested_obj--;

				Token** tokens_n = realloc(*tokens,++(*ptcount) * sizeof(Token*));
				if(!tokens_n)
				{
					printf("realloc failed, %s:%d.\n",F,L-3);
					return 0;
				}
				*tokens = tokens_n;
				break;
			}
			case ',':	
			case ':':
				complessive_counter++;
				break;
			case '[':	
			{	
				if(in_array > nested_obj)
				{
					nested_array = 1;
				} else if(in_array == nested_obj)
				{
					in_array++;	
				}

				(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
				if(!(*tokens)[(*ptcount - 1)])
				{
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}	
				

				(*tokens)[(*ptcount - 1)]->type = TOKEN_JSON_ARRAY_OPEN;
				
				Token** tokens_n = realloc(*tokens,++(*ptcount) * sizeof(Token*));
				if(!tokens_n)
				{
					printf("realloc failed, %s:%d.\n",F,L-3);
					return 0;
				}
				*tokens = tokens_n;
				break;
			}
			case ']':	
			{
				if(nested_array && in_array)
					nested_array = 0;
				else
					in_array--;
				
				(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
				if(!(*tokens)[(*ptcount - 1)])
				{
					printf("calloc failed, %s:%d.\n",F,L-3);
					return 0;
				}	
				
				(*tokens)[(*ptcount - 1)]->type = TOKEN_JSON_ARRAY_CLOSE;
				
				Token** tokens_n = realloc(*tokens,++(*ptcount) * sizeof(Token*));
				if(!tokens_n)
				{
					printf("realloc failed, %s:%d.\n",F,L-3);
					return 0;
				}
				*tokens = tokens_n;
				break;
			}
			default:
			{
				if(!isdigit(c) )
				{
					printf("unknown token");
					return 0;
				}
				
				int p = 0, value_size = 0;
				char* temp = &json[i];

				(*tokens)[(*ptcount) - 1] = calloc(1,sizeof(Token));
				if(!(*tokens)[(*ptcount) - 1])
				{
					printf("calloc failed,%s:%d.\n",F,L-3);
					return 0;
				}
			
				while(temp[p] != ',' && temp[p] != '[' && temp[p] != ']'
					&& temp[p] != '{' && temp[p] != '}')
				{	
					value_size++;
					complessive_counter++;
					p++;
				}
			
				(*tokens)[(*ptcount) - 1]->value = calloc(++value_size,sizeof(char));
				if(!(*tokens)[(*ptcount) - 1]->value)
				{
					printf("calloc falied, %s:%d.\n",F,L-3);
					return 0;
				}

				(*tokens)[(*ptcount) - 1]->value[value_size - 1] = '\0';
			
				p = 0;	
				while(temp[p] != ',' && temp[p] != '[' && temp[p] != ']'
					&& temp[p] != '{' && temp[p] != '}')
				{
					(*tokens)[(*ptcount) - 1]->value[p] = temp[p];
					p++;
				}
					
				(*tokens)[(*ptcount - 1)]->type = TOKEN_VALUE;
				/*check if the last char is the one of "]}[{"  */
				if(temp[p] == ']' || temp[p] == '['|| temp[p] != '{' || temp[p] != '}')
					i = --p;
				else
					i = p;

					/*resizing buffer and json string*/
				json = temp;
				buffer = strlen(json);

				/*expand the tokens array by one to contain the next data*/
				Token** tokens_n = realloc(*tokens, ++(*ptcount) * sizeof(Token*));
				if(!tokens_n)
				{
					printf("realloc failed, %s:%d.\n",F,L-3);
					return 0;
				}
				*tokens = tokens_n;
				continue;
			}
		}
	}
	free(original_json); 
	return 1;
}

/*in case of failure we do not free the compiled regex to increase performance */
unsigned char check_json_input(char* json)
{
	if(regexec(&regex,json,0,NULL,0) > 0) {
		printf("invalid json string, %s:%d.\n",F,L-2);
		return 0;
	}
	
	return 1;
}

/*
 * create_json_pairs() parses the json tokens into a structure JsonPairs,
 * @@@--- create_json_pairs uses conditional recursion ---@@@
 *
 *	function: create_json_pairs
 *	params:	- tokens, a pointer to an array of pointers to Token (Token*).
 *		- count, the size of the (*tokens) array.
 *		- pairs, a pointer to an array of pointers  to JsonPair (JsonPair*);
 *		- ppcount, a pointer to an integer that holds the size of the pairs array.
 *
 *	return: unsigned char, 0 the function failed, 1 the functions succeed.
 *	
 *	on retrun (succesfull) pairs will contain all the pairs at the caller level. 
 */
unsigned char create_json_pairs(Token*** tokens, int count, JsonPair*** pairs, int* ppcount)
{
	int i = 0;
	for(i = 0; i < count; i++)
	{
		switch((*tokens)[i]->type)
		{
			case TOKEN_COMMA:
			case TOKEN_COLON:
			case TOKEN_PROCESSED:
				break;
			case TOKEN_JSON_OBJ_OPEN:
			case TOKEN_JSON_OBJ_CLOSE:
				(*tokens)[i]->type = TOKEN_PROCESSED;
				break;
			case TOKEN_JSON_ARRAY_CLOSE:
				{
					if(nest_arr > 0)
						nest_arr = 0;

					(*tokens)[i]->type = TOKEN_PROCESSED;
					continue;
				}
			case TOKEN_JSON_ARRAY_OPEN:
			case TOKEN_END:
				(*tokens)[i]->type = TOKEN_PROCESSED;
				continue;
			case TOKEN_KEY:
				{
					/*check if we need to allocate new memory*/
					if((*pairs)[(*ppcount) - 1])
					{
						if((*pairs)[(*ppcount) - 1]->key)
						{
							JsonPair** pairs_n = realloc(*pairs,
									++(*ppcount)*sizeof(JsonPair*));
							if(!pairs_n) {
								printf("realloc failed,\
								    %s:%d.\n",F,L-3);
								return 0;
							}
							*pairs = pairs_n;	
						}
					}	
					(*pairs)[(*ppcount) - 1] = calloc(1,sizeof(JsonPair));
					if(!(*pairs)[(*ppcount) - 1])
					{
						printf("calloc failed, %s:%d.\n",F,L-3);
						return 0;
					}
					
					(*pairs)[(*ppcount) - 1]->key = strdup((*tokens)[i]->value);
					if(!(*pairs)[(*ppcount) - 1]->key)
					{
						printf("strdup failed, %s:%d.\n",F,L-3);
						return 0;
					}
					(*tokens)[i]->type = TOKEN_PROCESSED;
					
					break;
				}
			case TOKEN_VALUE:
			case TOKEN_ARRAY_ELEMENT:
			case TOKEN_JSON_NESTED_OBJ_OPEN_IN_ARRAY:
			case TOKEN_NESTED_ARRAY_ELEMENT:
				{
					if(!(*pairs)[(*ppcount) - 1]->value)
					{
						(*pairs)[(*ppcount) - 1]->value = calloc(1,sizeof(JsonValue));
						if(!(*pairs)[(*ppcount) - 1]->value)
						{
							printf("calloc failed, %s:%d.\n",F,L-2);
							return 0;
						} 
					}
					
					if((*tokens)[i]->type == TOKEN_JSON_NESTED_OBJ_OPEN_IN_ARRAY)
					{
						JsonPair** nest_pairs = calloc(1,sizeof(JsonPair*));
						if(!nest_pairs)
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
						int left_count = count - i - 1;
						int npcount = 1, *pnpc = &npcount;

						Token** nested_tokens = &(*tokens)[i+1];


						if(!create_json_pairs(&nested_tokens,left_count,
								&nest_pairs,pnpc))
						{
							printf("create_json_pairs failed, %s:%d.\n",F,L-2);
							return 0;
						}	 

						Object* json_ob = calloc(1,sizeof(Object));
						if(!json_ob)
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						json_ob->pairs = nest_pairs;
						json_ob->length = *pnpc;

						JsonType js_type = 0, *pjt = &js_type;
						if(!assign_data(&(*pairs)[(*ppcount) - 1]->value,
							(*tokens)[i]->type,NULL, pjt, json_ob))
						{
							printf("assign_data failed, %s:%d.\n",F,L-2);
							return 0;
						}
						
						(*tokens)[i]->type = TOKEN_PROCESSED;
						(*pairs)[(*ppcount) - 1]->value->type = *pjt;
						break;
					}

					JsonType js_type = 0, *pjt = &js_type;
					if(!assign_data(&(*pairs)[(*ppcount) - 1]->value,
							(*tokens)[i]->type,(*tokens)[i]->value, pjt, NULL))
					{
						printf("assign_data failed, %s:%d.\n",F,L-2);
						return 0;
					}
						
					(*tokens)[i]->type = TOKEN_PROCESSED;

					(*pairs)[(*ppcount) - 1]->value->type = *pjt;
					
					break;
				}
			case TOKEN_NULL:
				{
					(*pairs)[(*ppcount) - 1]->value = calloc(1,sizeof(JsonValue));
					if(!(*pairs)[(*ppcount) - 1]->value)
					{
						printf("calloc failed, %s:%d.\n",F,L-3);
						return 0;
					}
					
					(*pairs)[(*ppcount) - 1]->value->data.s = strdup((*tokens)[i]->value);
					if(!(*pairs)[(*ppcount) - 1]->value->data.s)
					{
						printf("strdup failed, %s:%d.\n",F,L-3);
						return 0;
					}

					(*tokens)[i]->type = TOKEN_PROCESSED;

					(*pairs)[(*ppcount) - 1]->value->type = JSON_NULL;

					break;
				}
			case TOKEN_FALSE:	
			case TOKEN_TRUE:
				{	
					(*pairs)[(*ppcount) - 1]->value = calloc(1,sizeof(JsonValue));
					if(!(*pairs)[(*ppcount) - 1]->value)
					{
						printf("calloc failed, %s:%d.\n",F,L-3);
						return 0;
					}
					
					(*pairs)[(*ppcount) - 1]->
						value->data.boolean = (*tokens)[i]->type == TOKEN_TRUE ? 1:0;
					
					(*tokens)[i]->type = TOKEN_PROCESSED;

					(*pairs)[(*ppcount) - 1]->value->type = JSON_BOOL;
					break;
				}
			case TOKEN_JSON_NESTED_OBJ_OPEN:
				{
					JsonPair** nest_pairs = calloc(1,sizeof(JsonPair*));
					if(!nest_pairs)
					{
						printf("calloc failed, %s:%d.\n",F,L-3);
						return 0;
					}
					int left_count = count - i - 1;
					int npcount = 1, *pnpc = &npcount;

					Token** nested_tokens = &(*tokens)[i+1];
					(*tokens)[i]->type = TOKEN_PROCESSED;


					if(!create_json_pairs(&nested_tokens,left_count,&nest_pairs,pnpc))
					{
						printf("create_json_pairs failed, %s:%d.\n",F,L-2);
						return 0;
					} 
					Object* json_ob = calloc(1,sizeof(Object));
					if(!json_ob)
					{
						printf("calloc failed, %s:%d.\n",F,L-3);
						return 0;
					}

					json_ob->pairs = nest_pairs;
					json_ob->length = *pnpc;

					(*pairs)[(*ppcount) - 1]->value = calloc(1,sizeof(JsonValue));
					if(!(*pairs)[(*ppcount) - 1]->value)
					{
						printf("calloc failed, %s:%d.\n",F,L-3);
						return 0;
					}
					
					(*pairs)[(*ppcount) - 1]->value->jsn_obj = json_ob;
					(*pairs)[(*ppcount) - 1]->value->type = JSON_NESTED_OBJECT;
					break;
				}
			case TOKEN_JSON_NESTED_OBJ_CLOSE:
			case TOKEN_JSON_NESTED_OBJ_CLOSE_IN_ARRAY:
				{	
					(*tokens)[i]->type = TOKEN_PROCESSED;
					return 1;
				}
			default:
				printf("unknown token type.\n");
				return 0;
		}
	}
	nest_arr = 0; /*reset global counter for nested array element*/
	return 1;
}

/*
	assign_data() populate the union Data.
		Data @@ defined in json.h @@
		{
			unsigned char boolean;
			double d;
			long i;
			Array* nested_array;
			Object* nested_object;
			char* s;
		};	
	
	params: - value, a double pointer to JsonValue, that is the data that we have to assign.
		- type, the token  that we are assigning, @@ see TokenType enum in json.h @@
		- value_s, char pointer to the actual data
		- js_type, pointer to the type of value that we are assiging @@ see JsonType enum in json.h
		- js_obj, pointer to struct Object, this can be NULL if the token that we are passing,
			to assign_data() is not an Object.  
 	return: unsigned char, 0 the function failed, 1 the functions succeed.

*/
unsigned char assign_data(JsonValue** value, TokenType type, char* value_s, JsonType* js_type, Object* js_obj)
{
	switch(type)
	{
		case TOKEN_VALUE:
			{
				if(is_floaintg_point(value_s))
				{
					double n = strtod(value_s,NULL);
					if(n == 0)
					{
						printf("strtod failed, %s:%d.\n",F,L-3);
						return 0;
					}
					(*value)->data.d = n;
					*js_type = JSON_DOUBLE;
					break;
				}	
				
				if(is_integer(value_s))	
				{
					long n = strtol(value_s,NULL,10);
					unsigned char value_eq_0 = strncmp(value_s,"0",strlen(value_s)+1) == 0 ;
					if(n == 0 && !value_eq_0)
					{
						printf("strtod failed, %s:%d.\n",F,L-3);
						return 0;
					}
					(*value)->data.i = n;
					*js_type = JSON_LONG;
					break;
				}
				(*value)->data.s = strdup(value_s);
				*js_type = JSON_STRING;
				break;
			}
		case TOKEN_ARRAY_ELEMENT:
		case TOKEN_JSON_NESTED_OBJ_OPEN_IN_ARRAY:
		case TOKEN_NESTED_ARRAY_ELEMENT:
			{
				*js_type = JSON_ARRAY;
				if(type == TOKEN_ARRAY_ELEMENT)
				{
					if(!(*value)->array_data.items)
					{
						(*value)->array_data.items = calloc(1,sizeof(Data*));
						if(!(*value)->array_data.items)
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
						(*value)->array_data.length = 1;
						
						(*value)->array_data.types = calloc(1,sizeof(JsonType));
						if(!(*value)->array_data.types)
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
					
					}else
					{
						Data** data_n = realloc((*value)->array_data.items,
							(++((*value)->array_data.length)) * sizeof(Data*));
						if(!data_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-2);
							return 0;
						}

						(*value)->array_data.items = data_n;
						
						JsonType* types_n = realloc((*value)->array_data.types,
								(*value)->array_data.length * sizeof(JsonType));
						if(!types_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data.types = types_n;
					}

					if(strncmp(value_s,JS_NULL,strlen(JS_NULL)) == 0)
					{
						(*value)->array_data
							.items[(*value)->array_data.length - 1] =
							calloc(1,sizeof(Data));

						if(!(*value)->array_data
								.items[(*value)->array_data.length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
		
						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							s = strdup(value_s);

						(*value)->array_data
							.types[(*value)->array_data.length - 1]= JSON_NULL;
						break;
					}

					if(strncmp(value_s,JS_TRUE,strlen(JS_TRUE)) == 0)
					{
						(*value)->array_data
							.items[(*value)->array_data.length - 1] =
							calloc(1,sizeof(Data));

						if(!(*value)->array_data.items[(*value)->array_data.length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							boolean = 1;

						(*value)->array_data
							.types[(*value)->array_data.length - 1] = JSON_BOOL;
						break;
					}

					if(strncmp(value_s,JS_FALSE,strlen(JS_FALSE)) == 0)
					{
						(*value)->array_data
							.items[(*value)->array_data.length - 1] =
							calloc(1,sizeof(Data));

						if(!(*value)->array_data
							.items[(*value)->array_data.length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
	
						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							boolean = 0;

						(*value)->array_data
							.types[(*value)->array_data.length - 1] = JSON_BOOL;
						break;
					}

					if(is_floaintg_point(value_s))
					{
						double n = strtod(value_s,NULL);
						if(n == 0)
						{	
							printf("strtod failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1] = 
							calloc(1,sizeof(Data));

						if(!(*value)->array_data
							.items[(*value)->array_data.length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->d = n;


						(*value)->array_data
							.types[(*value)->array_data.length - 1] = JSON_DOUBLE;
						break;
					}	
				
					if(is_integer(value_s))	
					{
						long n = strtol(value_s,NULL,10);
						if(n == 0)
						{
							printf("strtol failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1] =
							calloc(1,sizeof(Data));

						if(!(*value)->array_data
							.items[(*value)->array_data.length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->i = n;


						(*value)->array_data
							.types[(*value)->array_data.length - 1] = JSON_LONG;
						break;
					}

					(*value)->array_data
						.items[(*value)->array_data.length - 1] = 
						calloc(1,sizeof(Data));

					if(!(*value)->array_data
						.items[(*value)->array_data.length - 1])
					{
						printf("calloc failed, %s:%d.\n",F,L-3);
						return 0;
					}

					(*value)->array_data
						.items[(*value)->array_data.length - 1]->s = strdup(value_s);


					(*value)->array_data
							.types[(*value)->array_data.length - 1] = JSON_STRING;
					break;
				}else if(type == TOKEN_NESTED_ARRAY_ELEMENT)
				{
							
					if(nest_arr == 0) /* global counter for nested array elements*/
					{
						Data** data_n = realloc((*value)->array_data.items,
								++(*value)->array_data.length * sizeof(Data*));
						if(!data_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-3);
							return 0;
						} 
						
						(*value)->array_data.items = data_n;
						
					
						JsonType* types_n = realloc((*value)->array_data.types,
								(*value)->array_data.length * sizeof(JsonType));
						if(!types_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-3);
							return 0;
						} 
						
						(*value)->array_data.types = types_n;
						(*value)->array_data
							.types[(*value)->array_data.length - 1] 
							= JSON_NESTED_ARRAY;

						(*value)->array_data	
							.items[(*value)->array_data.length-1] =
							calloc(1, sizeof(Data));
						
						if(!(*value)->array_data
							.items[(*value)->array_data.length-1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length-1]->
							nested_array = calloc(1,sizeof(Array));

						if(!(*value)->array_data
							.items[(*value)->array_data.length-1]->
							nested_array)
						{
							printf("calloc failed, %s:%d.\n",F,L-2);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length-1]->
							nested_array->items = calloc(1,sizeof(Data*));
						
						if(!(*value)->array_data
							.items[(*value)->array_data.length-1]->
							nested_array->items) 
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data.items[(*value)->array_data.length-1]->
										nested_array->length = 1;
						
						(*value)->array_data.items[(*value)->array_data.length-1]->
										nested_array->types =
									calloc(1,sizeof(JsonType));
						
						if(!(*value)->array_data.items[(*value)->array_data.length-1]->
										nested_array->types)
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						nest_arr++;/*incremente global counter*/
					}else
					{
						Data** data_n = realloc(
							(*value)->array_data
								.items[(*value)->array_data.length-1]->
								nested_array->items,
							++(*value)->array_data
								.items[(*value)->array_data.length-1]->
								nested_array->length * sizeof(Data*));
						if(!data_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-2);
							return 0;
						}

						(*value)->array_data.items[(*value)->array_data.length-1]->
									nested_array->items = data_n;

						JsonType* types_n = realloc(
							(*value)->array_data
								.items[(*value)->array_data.length-1]->
								nested_array->types,
							(*value)->array_data
								.items[(*value)->array_data.length-1]->
								nested_array->length * sizeof(JsonType));
						if(!types_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-2);
							return 0;
						}

						(*value)->array_data.items[(*value)->array_data.length-1]->
									nested_array->types = types_n;

						nest_arr++;/*increment global counter*/
					}


					if(strncmp(value_s,JS_NULL,strlen(JS_NULL)) == 0)
					{
							(*value)->array_data
								.items[(*value)->array_data.length - 1]->
								nested_array->items[(*value)->array_data
								.items[(*value)->array_data.length - 1]->
								nested_array->length - 1] = 
								calloc(1,sizeof(Data));
						if(!(*value)->array_data
								.items[(*value)->array_data.length - 1]->
								nested_array->items[(*value)->array_data
								.items[(*value)->array_data.length - 1]->
								nested_array->length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
		
						(*value)->array_data.items[(*value)->array_data.length - 1]->
								nested_array->items[(*value)->array_data
								.items[(*value)->array_data.length - 1]->
								nested_array->length - 1]->
								s = strdup(value_s);

						(*value)->array_data.items[(*value)->array_data.length - 1]->
								nested_array->types[(*value)->array_data.
								items[(*value)->array_data.length - 1]->
								nested_array->length-1] 
								= JSON_NULL;
						break;
					}

					if(strncmp(value_s,JS_TRUE,strlen(JS_TRUE)) == 0)
					{
						 (*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1] =
							calloc(1,sizeof(Data));
						if(!(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1]->boolean = 1;

						(*value)->array_data.items[(*value)->array_data.length - 1]->
								nested_array->types[(*value)->array_data.
								items[(*value)->array_data.length - 1]->
								nested_array->length-1] 
								= JSON_BOOL;
						break;
					}

					if(strncmp(value_s,JS_FALSE,strlen(JS_FALSE)) == 0)
					{
						(*value)->array_data.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1] = 
							calloc(1,sizeof(Data));

						if(!(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
	
						(*value)->array_data.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1]->boolean = 0;

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->types[(*value)->array_data.
							items[(*value)->array_data.length - 1]->
							nested_array->length-1] 
							= JSON_BOOL;
						break;
					}

					if(is_floaintg_point(value_s))
					{
						double n = strtod(value_s,NULL);
						if(n == 0)
						{	
							printf("strtod failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1] = 
							calloc(1,sizeof(Data));

						if(!(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1]->d = n;

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->types[(*value)->array_data.
							items[(*value)->array_data.length - 1]->
							nested_array->length-1] 
							= JSON_DOUBLE;
						break;
					}	
				
					if(is_integer(value_s))	
					{
						long n = strtol(value_s,NULL,10);
						if(n == 0)
						{
							printf("strtol failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1] =
							calloc(1,sizeof(Data));
						if(!(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1])
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->items[(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->length - 1]->i = n;

						(*value)->array_data
							.items[(*value)->array_data.length - 1]->
							nested_array->types[(*value)->array_data.
							items[(*value)->array_data.length - 1]->
							nested_array->length - 1] 
							= JSON_LONG;
						break;
					}

					(*value)->array_data
						.items[(*value)->array_data.length - 1]->
						nested_array->items[(*value)->array_data
						.items[(*value)->array_data.length - 1]->
						nested_array->length - 1]  = calloc(1,sizeof(Data));
					if(!(*value)->array_data
						.items[(*value)->array_data.length - 1]->
						nested_array->items[(*value)->array_data
						.items[(*value)->array_data.length - 1]->
						nested_array->length - 1])
					{
						printf("calloc failed, %s:%d.\n",F,L-3);
						return 0;
					}

					(*value)->array_data
						.items[(*value)->array_data.length - 1]->
						nested_array->items[(*value)->array_data
						.items[(*value)->array_data.length - 1]->
						nested_array->length - 1]->s = strdup(value_s);

					(*value)->array_data
						.items[(*value)->array_data.length - 1]->
						nested_array->types[(*value)->array_data.
						items[(*value)->array_data.length - 1]->
						nested_array->length-1] 
						= JSON_STRING;
					break;
				}else
				{
					if(!(*value)->array_data.items)
					{
						(*value)->array_data.items = calloc(1,sizeof(Data*));
						if(!(*value)->array_data.items)
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
						(*value)->array_data.length = 1;
						
						(*value)->array_data.types = calloc(1,sizeof(JsonType));
						if(!(*value)->array_data.types)
						{
							printf("calloc failed, %s:%d.\n",F,L-3);
							return 0;
						}
					
					}else
					{
						Data** data_n = realloc((*value)->array_data.items,
							++(*value)->array_data.length * sizeof(Data*));
						if(!data_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-2);
							return 0;
						}

						(*value)->array_data.items = data_n;
					
						JsonType* types_n = realloc((*value)->array_data.types,
						(*value)->array_data.length * sizeof(JsonType));
						if(!types_n)
						{
							printf("realloc failed, %s:%d.\n",F,L-3);
							return 0;
						}

						(*value)->array_data.types = types_n;
			
					}		
					
					(*value)->array_data
						.items[(*value)->array_data.length - 1] = calloc(1,sizeof(Data));
					
					if(!(*value)->array_data
                                                .items[(*value)->array_data.length - 1])
					{
						printf("calloc failed, %s:%d.\n",F,L-5);
						return 0;
					}

					(*value)->array_data.items[(*value)->array_data.length -1]
						->nested_object = js_obj;
					
					(*value)->array_data.types[(*value)->array_data.length - 1] =
						JSON_NESTED_OBJ_IN_ARRAY;
					break;
					
				}
				}
		default:
			printf("unknown type");
			return 0;
	}

	return 1;

}
unsigned char free_array_data(Data** items, JsonType* types, int size)
{
	int i = 0;
	for(i = 0; i < size; i++)
	{
		switch(types[i])
		{
			case JSON_BOOL:
			case JSON_NULL:
			case JSON_STRING:
				free(items[i]->s);
				break;
			case JSON_NESTED_ARRAY:
				free_array_data(items[i]->nested_array->items, 
					items[i]->nested_array->types,
					items[i]->nested_array->length);
				free(items[i]->nested_array);	
				break;
			case JSON_DOUBLE:
			case JSON_LONG:
				break;
			case JSON_OBJECT:
				break;
			case JSON_NESTED_OBJ_IN_ARRAY:
				{
					int size = items[i]->nested_object->length;
					int j = 0;
					for(j = 0; j < size; j++)
						free_json_pair(items[i]->nested_object->pairs[j]);

					free(items[i]->nested_object->pairs);
					free(items[i]->nested_object);
					break;
				}
			default:
				printf("unkown type");
				return 0;
		}
		free(items[i]);
		
	}
	free(items);
	free(types);
	return 1;
}
unsigned char free_json_pair(JsonPair* pair)
{
	if(pair)
	{
		if(pair->key)
		{
			free(pair->key);
			if(pair->value)
			{	switch(pair->value->type)
				{
					case JSON_DOUBLE:
					case JSON_LONG:
					case JSON_BOOL:
						free(pair->value);
						break;
					case JSON_NULL:
					case JSON_STRING:
							free(pair->value->data.s);
							free(pair->value);
							break;
					case JSON_ARRAY:
						if(!free_array_data(pair->value->array_data.items,
							pair->value->array_data.types,
							pair->value->array_data.length))
						{
							printf("free_array_data \
								 failed %s:%d.\n",F,L-6);
							return 0;	
						}
						free(pair->value);
						break;
					case JSON_NESTED_OBJECT:
						{
							int size = pair->value->jsn_obj->length;
							int j = 0;
							for(j = 0; j < size; j++)
								free_json_pair(pair->value->jsn_obj->pairs[j]);

							free(pair->value->jsn_obj->pairs);
							free(pair->value->jsn_obj);
							free(pair->value);
							break;
						}
					default:
						printf("unknown type. %s:%d.\n",F,L);
						return 0;
				}
			}
		}
		free(pair);
	}
	return 1;
}	
void free_token(Token* token)
{
	if(token)
	{
		if(token->value)
		{
			free(token->value);
		}
		free(token);
	}
}
void free_tokens_array(Token*** tokens,int ptcount)
{
	int i = 0;
	for(i = 0; i<ptcount; i++)
	{
		if(*tokens)
		{
			if((*tokens)[i])
			{
				free_token((*tokens)[i]);
			}
		}
	}
	free(*tokens);
}

void free_json_pairs_array(JsonPair*** pairs,int jpcount)
{
	for(int i = 0; i < jpcount; i++)
	{
		if(*pairs)
		{
			if((*pairs)[i])
				free_json_pair((*pairs)[i]);
		}
	}
	
	free(*pairs);
}


unsigned char create_json_pair_tree(JsonPair*** pairs, int pairs_size, pBST_param)
{
	for(int i = 0; i < pairs_size; i++)
	{
		switch((*pairs)[i]->value->type) {
		case JSON_STRING:
			if(!(*BST_tree).insert(t_s,(void*)(*pairs)[i]->key,&(*BST_tree).root,
				(void**)&(*pairs)[i]->value->data.s,t_s))
			{
				printf("binary tree insert failed, %s:%d.\n",F,L-3);
				return 0;
			}
			break;
		case JSON_LONG:
			long* pl = &(*pairs)[i]->value->data.i;
			if(!(*BST_tree).insert(t_s,(void*)(*pairs)[i]->key,
						&(*BST_tree).root,(void**)&pl,t_l)) {
					printf("binary tree insert failed, %s:%d.\n",F,L-3);
					return 0;
			}
			break;
		default:
			printf("unknown json type.\n");
			return 0;	
		}
	}	
	
	return 1;
}

