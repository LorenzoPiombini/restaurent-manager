#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include "json.h"
#include "user_create.h"
#include "debug.h"
#include "str_op.h"
#include "key_op.h"
#include "file.h"
#include "crud.h"
#include "parse.h"
#include "bst.h"
#include "db_instruction.h"
#include "common.h"

/*local function prototypes */
static unsigned char creates_string_instruction(char* file_name, int fd_data, char** db_data, BST pairs_tree);



static unsigned char creates_string_instruction(char* file_name, int fd_data, char** db_data, BST pairs_tree)
{
	/*loads the schema from the header file and creates the string to write
		to the file */
			 	
	struct Schema sch = {0,NULL,NULL};
	struct Header_d hd = {0,0,sch};

	if(!is_a_db_file(fd_data,file_name,&hd))
	{
		printf("not a db file, or reading header error, %s:%d.\n",F,L-2);
		close_file(1,fd_data);
		return 0;
	}

	/*creates data_to_add from the schema*/
	char* data_to_add = NULL;
	if(!create_data_to_add(&hd.sch_d,&data_to_add))
	{
		printf("create_data_to_add(), failed %s:%d.\n",F,L-2);
		free_schema(&hd.sch_d);
		return 0;
	}

	char *dta_buf = strdup(data_to_add);
	free(data_to_add);

	char** dta_blocks = NULL;
	int blocks = 0;	
	if(!create_blocks_data_to_add(dta_buf, &dta_blocks, &blocks))
	{
		printf("create_blocks_data_to_add() failed, %s:%d.\n",F,L-2);
		free(dta_buf);
		free_schema(&hd.sch_d);
		return 0;
	}
				
	free(dta_buf);

	/*build the final data to add*/
	*db_data = NULL;

	for(int i = 0; i < hd.sch_d.fields_num; i++)
	{
		switch(hd.sch_d.types[i])
		{
			case TYPE_STRING:
			{
				char* node_data = NULL;
				size_t init_s = 0;
				size_t new_part = 0; 
				size_t old_size = 0; 
						
				/* find the data in the tree */ 
				if(!find(t_s,(void*)hd.sch_d.fields_name[i],
					&pairs_tree.root,(void**)&node_data,t_s))
						continue;

				if(!*db_data)
				{
					init_s = strlen(dta_blocks[i]) + strlen(node_data) + 1;
					*db_data = calloc(init_s,sizeof(char));
					if(!*db_data)
					{
					 	__er_calloc(F,L-3);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
				}
				else
				{
					new_part = strlen(dta_blocks[i]) + strlen(node_data) + 1;
					old_size = strlen(*db_data);
					size_t l = old_size + new_part;
					char* nw_db_data = realloc(*db_data,l*sizeof(char));
					if(!nw_db_data)
					{
						__er_realloc(F,L-3);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						free(db_data);
						return 0;
					}
					*db_data = nw_db_data;
					memset((*db_data) + old_size,0,(l-old_size)*sizeof(char));
	
				}
					
				if(i == 0)
				{
					if(snprintf(*db_data,init_s,"%s%s",dta_blocks[i],node_data) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free(*db_data);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
				}
				else
				{
					if(snprintf(&(*db_data)[old_size],new_part,"%s%s",dta_blocks[i],
							node_data) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free(*db_data);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
								
				}
				break;
			} 	
			case TYPE_INT:
			case TYPE_BYTE:
			case TYPE_LONG:
			{
				long* node_data = NULL;
				size_t init_s = 0;
				size_t new_part = 0; 
				size_t old_size = 0; 
				char* str_number = NULL;	
				/* find the data in the tree */ 
				if(!find(t_s,(void*)hd.sch_d.fields_name[i],
					&pairs_tree.root,(void**)&node_data,t_l))
						continue;
	
				if(!*db_data)
				{
					size_t digits =	number_of_digit(*node_data) + 1;
					str_number = calloc(digits,sizeof(char));
					if(!str_number)
					{
						__er_calloc(F,L-3);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
					}
								
					if(snprintf(str_number,digits,"%ld",*node_data) < 0)
					{
						printf("snprintf() failed, %s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
								
					init_s = strlen(dta_blocks[i]) + digits; 
					*db_data = calloc(init_s,sizeof(char));
					if(!*db_data)
					{
						__er_calloc(F,L-3);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
				}
				else
				{
					size_t digits =	number_of_digit(*node_data) + 1;
					str_number = calloc(digits,sizeof(char));
					if(!str_number)
					{
						__er_calloc(F,L-3);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						free(*db_data);
						return 0;
					}
						
					if(snprintf(str_number,digits,
						"%ld",*node_data) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						free(*db_data);
						return 0;
					}

					old_size = strlen(*db_data);
					new_part = strlen(dta_blocks[i]) + digits;

					size_t l = old_size + new_part;
					char* nw_db_data = realloc(*db_data,l*sizeof(char));
					if(!nw_db_data)
					{
						__er_realloc(F,L-3);
						free_schema(&hd.sch_d);
						free(*db_data);
						free_strs(blocks,1,dta_blocks);
						close_file(1,fd_data);
						return 0;
					}
					*db_data = nw_db_data;
					memset((*db_data)+old_size,0,(l-old_size)*sizeof(char));
				}
							
				if(i == 0)
				{
 
					if(snprintf(*db_data,init_s,"%s%s",dta_blocks[i],str_number) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free(*db_data);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
					free(str_number);
					str_number = NULL;
				}
				else
				{
					if(snprintf(&(*db_data)[old_size],new_part,
							"%s%s",dta_blocks[i],str_number) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free(*db_data);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
					free(str_number);
					str_number = NULL;
								
				}
				break;
			}		
			case TYPE_FLOAT:
			case TYPE_DOUBLE:
			{
				double* node_data = NULL;
				size_t init_s = 0;
				size_t new_part = 0; 
				size_t old_size = 0; 
				char* str_num = NULL;	
				/* find the data in the tree */ 
				if(!find(t_s,(void*)hd.sch_d.fields_name[i],
					&pairs_tree.root,(void**)&node_data,t_d))
						continue;

				if(!*db_data)
				{ 
					size_t digits =	digits_with_decimal(*node_data);
					/*+2 for decimal position and +1 for '\0'*/
					digits += (2+1);
					str_num = calloc(digits,sizeof(char));
					if(!str_num)
					{
						__er_calloc(F,L-3);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
								
					if(snprintf(str_num,digits,"%.2f",*node_data) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}

					init_s = strlen(dta_blocks[i])+digits;
					*db_data = calloc(init_s,sizeof(char));
					if(!(*db_data))
					{
						__er_calloc(F,L-3);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
				}
				else
				{
					size_t digits =	digits_with_decimal(*node_data);
					/*+2 for decimal position and +1 for '\0'*/
					digits += (2+1);
					str_num = calloc(digits,sizeof(char));
					if(!str_num)
					{
						__er_calloc(F,L-3);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
							
					if(snprintf(str_num,digits,"%.2f",*node_data) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free_strs(blocks,1,dta_blocks);
						free(*db_data);
						return 0;
					}
				
					old_size = strlen(*db_data); 
					new_part = strlen(dta_blocks[i]) + digits;
					size_t l = old_size + new_part;
					char* nw_db_data = realloc(*db_data,l*sizeof(char));
					if(!nw_db_data)
					{
						__er_realloc(F,L-3);
						free_schema(&hd.sch_d);
						free(*db_data);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
					*db_data = nw_db_data;
					memset((*db_data)+old_size,0,(l-old_size)*sizeof(char));
				}
							
				if(i == 0)
				{
					if(snprintf(*db_data,init_s,"%s%s",dta_blocks[i],str_num) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free(*db_data);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
					free(str_num);
					str_num = NULL;
				}
				else
				{
					if(snprintf(&(*db_data)[old_size],new_part,"%s%s",dta_blocks[i],
								str_num) < 0)
					{
						printf("snprintf() failed,%s:%d.\n",F,L-2);
						free_schema(&hd.sch_d);
						free(*db_data);
						free_strs(blocks,1,dta_blocks);
						return 0;
					}
					free(str_num);
					str_num = NULL;
				}
				break;
			}		
			default:
				printf("unknown type.\n");
				free_schema(&hd.sch_d);
				free(*db_data);
				free_strs(blocks,1,dta_blocks);
				return 0;
			}
		}
			
		free_schema(&hd.sch_d);
		free_strs(blocks,1,dta_blocks);
		return 1;
}
unsigned char convert_pairs_in_db_instruction(BST pairs_tree,Instructions inst)
{
	switch(inst)
	{
		case WRITE_EMP:
			{
				/*find restaurant id (user in the linux system)*/
				long* node_data = NULL;	
				/* find the data in the tree */ 
				if(!find(t_s,(void*)REST_ID,
					&pairs_tree.root,(void**)&node_data,t_l)) {
					fprintf(stderr,"restaurant does not exist.\n");
						return 0;
				}
				
				struct passwd* pwd = getpwuid((int)*node_data);
				if(!pwd) {
					fprintf(stderr,"user does not exist.");
					return 0;
				}

				/*
				 * create the path to open the files 
				 * + 2 is  1 for  '\0' and 1 for '/'
				 * */

				if(chdir(pwd->pw_dir) != 0) {
					fprintf(stderr,"can't change directory.");
					return EUSER;
				}
				/*
				 * loads the schema from the header file 
				 * and creates the string to write to the file 
				 * */
				
				int fd_data = open_file(EMPL,0);
				if(file_error_handler(1,fd_data) > 0) {
					printf("error opening file, %s:%d.\n",F,L-2);
					return 0;
				}
			
				char* db_data = NULL;
				if(!creates_string_instruction("employee", fd_data, &db_data, pairs_tree))
				{
					printf("creates_string_instruction() failed, %s:%d.\n",F,L-2);
					close_file(1,fd_data);
					return 0;
				}
				
				if(!__write_safe(fd_data,db_data,"employee"))
				{
					printf("__write_safe() failed %s:%d.\n",F,L-2);
					free(db_data);
					close_file(1,fd_data);
					return 0;
				}

				free(db_data);
				close_file(1,fd_data);
				break;
			}
		case CK_IN:
			{
				
				break;
			} 
		case CK_OUT:
			{
				
				break;
			}
		case WRITE_TIP:
			{
				
				break;
			}  
		case NW_REST:
                case LG_REST:
		{
			char* username = NULL;
			if(!find(t_s,"username",&pairs_tree.root,(void**)&username,t_s)) {
				fprintf(stderr,"error new user.\n");
				return 0;
			}

			char *passwd = NULL;
			if(!find(t_s,"password",&pairs_tree.root,(void**)&passwd,t_s)) {
				fprintf(stderr,"error new user.\n");
				return 0;
			}

			if(inst == NW_REST) {
				int r = add_user(username,passwd);
				if(r == -1 || r == EALRDY_U) 
					return EUSER; 

				/* here you have to create the file in the new home*/
				struct passwd *pwd = getpwuid(r);
				if(pwd) {
					fprintf(stderr,"fail to get the new user");
					return EUSER;
				}

				if(chdir(pwd->pw_dir) != 0) {
					fprintf(stderr,"can't change directory.");
					return EUSER;
				}
				
				if(!create_system_from_txt_file(FSYS)) {
					fprintf(stderr,
							"failed to create restaurant system");
					return 0;
				}
					
				break;
			} else if(inst == LG_REST) {
				if(login(username,passwd) == -1) {
					fprintf(stderr,"login failed.\n");
					return 0;
				}
				/*YOU WILL HAVE TO KEEP TRACK OF WHO EVER IS ALREADY LOGGED IN*/
				return S_LOG;
			}

		}
		default:
			printf("unknow instruction.\n");
			return 0;	
	}

	printf("instruction succeed.\n");	
	return 1;
}
