#ifndef JSON_H
#define JSON_H

#include <regex.h>

/*for pBST_param */
#include "bst.h"


#define JS_NULL "null"
#define JS_TRUE "true"
#define JS_FALSE "false"


/* regex(s) used to validate the json string before parsing */
#define JSRGX_STRING "\"[a-zA-Z0-9.!\/*@_]+\""
#define JSRGX_VALUE JSRGX_STRING "|[0-9.]+|true|false|null"
#define JSRGX_ARRAY "\\[\\s*(" JSRGX_STRING "|[0-9.]+|true|false|null)" \
		    "(\\s*,\\s*(" JSRGX_STRING "|[0-9.]+|true|false|null))*\\s*\\]"

#define JSRGX_NESTED_ARRAY "\\[\\s*(\\s*" JSRGX_STRING "|[0-9.]+|true|false|null|" JSRGX_ARRAY ")" \
 			   "(\\s*,\\s*("JSRGX_STRING "|[0-9.]+|true|false|null|" JSRGX_ARRAY "))*\\s*\\]"

#define JSRGX_OBJECT "\\{\\s*" JSRGX_STRING ":\\s*(" JSRGX_STRING "|" JSRGX_VALUE "|" JSRGX_ARRAY "|" \
                     JSRGX_NESTED_ARRAY  ")" \
                     "(,\\s*" JSRGX_STRING ":\\s*(" JSRGX_STRING "|" JSRGX_VALUE "|" JSRGX_ARRAY "|" \
                     JSRGX_NESTED_ARRAY "))*\\s*\\}"

#define JSRGX_NESTED_OBJECT "\\{\\s*" JSRGX_STRING ":\\s*(" JSRGX_STRING "|" JSRGX_VALUE "|" JSRGX_ARRAY "|" \
                     JSRGX_NESTED_ARRAY "|" JSRGX_OBJECT ")(,\\s*" JSRGX_STRING ":\\s*(" JSRGX_STRING "|" \
		     JSRGX_VALUE "|" JSRGX_ARRAY "|" JSRGX_NESTED_ARRAY "|" JSRGX_OBJECT "))*\\s*\\}"

#define JSRGX_NST_OBJ_LAST "\\{\\s*" JSRGX_STRING ":\\s*(" JSRGX_STRING "|" JSRGX_VALUE "|" JSRGX_ARRAY "|" \
                     JSRGX_NESTED_ARRAY "|" JSRGX_OBJECT "|" JSRGX_NESTED_OBJECT")("\
		     ",\\s*" JSRGX_STRING ":\\s*(" JSRGX_STRING "|" JSRGX_NESTED_OBJECT "|" \
                     JSRGX_VALUE "|" JSRGX_ARRAY "|" JSRGX_NESTED_ARRAY "|" JSRGX_OBJECT "))*\\s*\\}"



#define JSRGX_ARRAY_WITH_OBJECT "\\[\\s*(\\s*" JSRGX_STRING "|[0-9.]+|true|false|null|" JSRGX_ARRAY \
				"|" JSRGX_OBJECT "|" JSRGX_NST_OBJ_LAST")(\\s*,\\s*("JSRGX_STRING "|[0-9.]+"\
				"|true|false|null|" JSRGX_ARRAY "|" JSRGX_OBJECT "|"JSRGX_NST_OBJ_LAST \
				"))*\\s*\\]"

#define JSRGX_NESTED_OBJECT_W_ARRAY_W_OBJ "\\{\\s*" JSRGX_STRING ":\\s*(" JSRGX_STRING "|" JSRGX_VALUE \
			"|" JSRGX_ARRAY "|" JSRGX_NESTED_ARRAY "|" JSRGX_OBJECT "|"\
			JSRGX_ARRAY_WITH_OBJECT "|" JSRGX_NESTED_OBJECT ")(,\\s*" JSRGX_STRING \
			":\\s*(" JSRGX_STRING "|" JSRGX_VALUE "|" JSRGX_ARRAY "|" JSRGX_NESTED_ARRAY \
			"|" JSRGX_OBJECT "|" JSRGX_ARRAY_WITH_OBJECT "|" JSRGX_NESTED_OBJECT"))*\\s*\\}"

#define JSRGX_OBJ "^\\{\\s*" JSRGX_STRING ":\\s*(" JSRGX_OBJECT "|" JSRGX_STRING "|" JSRGX_VALUE \
		"|" JSRGX_ARRAY "|"  JSRGX_NESTED_ARRAY "|" JSRGX_NESTED_OBJECT "|" \
		 JSRGX_ARRAY_WITH_OBJECT "|" JSRGX_NESTED_OBJECT_W_ARRAY_W_OBJ ")(,\\s*" JSRGX_STRING \
		":\\s*(" JSRGX_OBJECT "|"  JSRGX_STRING "|" JSRGX_ARRAY "|" JSRGX_NESTED_ARRAY "|"JSRGX_VALUE \
		"|"JSRGX_NESTED_OBJECT "|" JSRGX_ARRAY_WITH_OBJECT "|" JSRGX_NESTED_OBJECT_W_ARRAY_W_OBJ \
 		"))*\\s*\\}$"   

/*--------------------------------------------------------------------------------------------------------*/

extern regex_t regex;

#define REG_FREE regfree(&regex);

/*nested array element counter and nested object counter, used for correct memory allocation*/
extern int nest_arr;
extern int nest_obj;


/*define token types*/
typedef enum 
  {
	TOKEN_COMMA,
	TOKEN_COLON,
	TOKEN_JSON_OBJ_OPEN,
	TOKEN_JSON_OBJ_CLOSE,
	TOKEN_JSON_NESTED_OBJ_OPEN,
	TOKEN_JSON_NESTED_OBJ_CLOSE,
	TOKEN_JSON_NESTED_OBJ_OPEN_IN_ARRAY,
	TOKEN_JSON_NESTED_OBJ_CLOSE_IN_ARRAY,
	TOKEN_JSON_ARRAY_CLOSE,
	TOKEN_JSON_ARRAY_OPEN,
	TOKEN_KEY,
	TOKEN_VALUE,
	TOKEN_NULL,
	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_END,
	TOKEN_ARRAY_ELEMENT,
	TOKEN_NESTED_ARRAY_ELEMENT,
	TOKEN_NESTED_OBJ_ARRAY_ELEMENT,
	TOKEN_PROCESSED
  }TokenType;

/*json types*/
typedef enum
{
	JSON_NULL,
	JSON_BOOL,
	JSON_STRING,
	JSON_ARRAY,
	JSON_DOUBLE,
	JSON_LONG,
	JSON_NESTED_ARRAY,
	JSON_NESTED_OBJ_IN_ARRAY,
	JSON_OBJECT,
	JSON_NESTED_OBJECT
}JsonType;

typedef struct JsonValue JsonValue;
typedef struct JsonPair JsonPair;
typedef struct Token Token;
typedef union Data Data;

/*array in a json object*/
typedef struct 	
{
	Data** items;
	JsonType* types;
	size_t length;
}Array;

/*json object itself or contained in a json object*/
typedef struct
{
	JsonPair** pairs;
	size_t length;
}Object;

/*rappresent the type of data in a json object*/
union Data 
{
	unsigned char boolean;
	double d;
	long i;
	Array* nested_array;
	Object* nested_object;
	char* s;
};	

/*json value - part of struct JsonPair*/
struct JsonValue
{
	JsonType type;
	Data data;
	Array array_data;
	Object* jsn_obj;		
};

/* json key value pair*/
struct JsonPair 
{
	char* key;
	JsonValue* value;
};

/*token from the json object string*/
struct Token
{
	TokenType type;
	char* value;
};

unsigned char check_json_input(char* json);
unsigned char parse_json_object(char* json_str, JsonPair*** pairs, int* pairs_size);
unsigned char lex(char* json, Token*** tokens, int* ptcount);
unsigned char create_json_pairs(Token*** tokens, int count, JsonPair*** pairs, int* ppcount);
unsigned char assign_data(JsonValue** value, TokenType type, char* value_s, JsonType* js_type, Object* js_obj);
unsigned char free_array_data(Data** items, JsonType* types, int size);
unsigned char free_json_pair(JsonPair* pair);
void free_json_pairs_array(JsonPair*** pairs,int jpcount);
void free_token(Token* token);
void free_tokens_array(Token*** tokens,int ptcount);
unsigned char create_json_pair_tree(JsonPair*** pairs, int pairs_size, pBST_param);
int init_rgx();

#endif /* json.h */
