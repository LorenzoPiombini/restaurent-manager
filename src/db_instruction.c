#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#include "build.h"
#include "db_instruction.h"
#include "common.h"
#include "restaurant_t.h"


/* global login data to send back to the user loggin in */
struct login_u user_login = {NULL,-1};

/*local function prototypes */
static unsigned char creates_string_instruction(char* file_name, int fd_data, char** db_data, BST pairs_tree);
static int login_employee(char *username, char * passwd);


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

static int login_employee(char *username, char * passwd)
{
	

}
unsigned char convert_pairs_in_db_instruction(BST pairs_tree,Instructions inst)
{
	switch(inst)
	{
		case WRITE_EMP:
			{
				/* find the data in the tree */
			       char *node_data = NULL;	       
				if(!find(t_s,(void*)REST_HM,
					&pairs_tree.root,(void**)&node_data,t_s)) {
					fprintf(stderr,"restaurant does not exist.\n");
						return 0;
				}
				
				/*
				 * get the current directory
				 * */

				char cur_dir[1024];
				memset(cur_dir,0,1024);
				if(getcwd(cur_dir,1024) == NULL) {
					fprintf(stderr,"can't get the current directory.\n");
					free(node_data);
					return 0;
				}

				if(chdir(node_data) != 0) {
					fprintf(stderr,"can't change directory.");
					free(node_data);
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
				if(!creates_string_instruction("employee", 
							fd_data, &db_data, 
							pairs_tree)) {
					printf("creates_string_instruction() failed,\
						       	%s:%d.\n",F,L-2);
					close_file(1,fd_data);
					return 0;
				}
			
				char *export_key = NULL;	
				if(!__write_safe(fd_data,db_data,"employee",&export_key)) {
					printf("__write_safe() failed %s:%d.\n",F,L-2);
					free(db_data);
					close_file(1,fd_data);
					return 0;
				}
				
				close_file(1,fd_data);
				free(db_data);
				

				/*
				 * save the employee
				 * into teh users master file 
				 * for login porpuses;
				 * */
				char *first_name = NULL;	       
				if(!find(t_s,(void*)"name",
					&pairs_tree.root,(void**)&first_name,t_s)) {
					fprintf(stderr,"first name not found.\n");
						return 0;
				}

				char *last_name = NULL;	       
				if(!find(t_s,(void*)"last name",
					&pairs_tree.root,(void**)&last_name,t_s)) {
					fprintf(stderr,"last name does not exist.\n");
						return 0;
				}
				
				long *rest_id = NULL;
				if(!find(t_s,(void*)REST_ID,
					&pairs_tree.root,(void**)&rest_id,t_l)) {
					fprintf(stderr,"resaurant id does not exist.\n");
						return 0;
				}

				long *role = NULL;
				if(!find(t_s,(void*)"role",
					&pairs_tree.root,(void**)&role,t_l)) {
					fprintf(stderr,"role does not exist.\n");
						return 0;
				}
				
				/*create the users master file entry*/
				int permission = (*role) == SERVER ? 1 : 0;	
				char *user_name = "user_name:t_s:";
				char *pass = ":password:t_s:";
				char *perm = ":permission:t_b:";
				char *employee_id = ":employee_id:t_s:";
				char *r_id = ":restaurant_id:t_i:";

				char *hash = NULL;
				size_t l = number_of_digit((int)*rest_id) + 1;
				char paswd[l];
				memset(paswd,0,l);

				if(snprintf(paswd,l,"%ld",*rest_id) < 0) {
					fprintf(stderr
							,"snprintf() failed %s:%d\n"
							,F,L-3);
					return 0;
				}

				if(crypt_pswd(paswd,&hash) == -1) {
					fprintf(stderr
							,"crypt_paswd() failed %s:%d\n"
							,F,L-3);
					return 0;
				}

				size_t len = strlen(first_name) 
						+ strlen(last_name)
						+ strlen(user_name)
						+ strlen(pass)
						+ strlen(hash)
						+ strlen(perm)
						+ strlen(employee_id)
						+ strlen(export_key)
						+ strlen(r_id)
						+ number_of_digit(permission)
						+ number_of_digit((int)*rest_id) + 1;

				char data[len];
				memset(data,0,len);
				
				
				if(snprintf(data,len,"%s%s%s%s%s%s%d%s%s%s%ld",
							user_name,first_name,
							last_name,pass,hash,
							perm,permission,
							employee_id,export_key,
							r_id,*rest_id) < 0) {
					fprintf(stderr,"snprintf() failed. %s:%d.\n"
							,F,L - 7);
					free(hash);
					free(export_key);
					return 0;
				}
					
				free(export_key);
				free(hash);

				/*change directory to u*/
				if( chdir("/u") != 0 ) {	
					fprintf(stderr,
							"failed to create restaurant system");
					return 0;
				}

				/*write to the file users*/
				int fd_users = open_file("users.dat",0);
				if(file_error_handler(1,fd_users) > 0 ) {
					fprintf(stderr,"can't open users.\n");
					return 0;
				}
				

				if(!__write_safe(fd_users,data,"users", NULL)) {
					fprintf(stderr,"can't write to users.\n");
					close_file(1,fd_users);
					return 0;
				}

				close_file(1,fd_users);

				/*change back to the original directory*/
				if( chdir(cur_dir) != 0 ) {	
					fprintf(stderr,
							"failed to create restaurant system");
					return 0;
				}
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
				char *home = NULL;
				int uid = 0;	
				if(get_user_info(username,&home,&uid) == -1) {
					fprintf(stderr,"can't get user info.\n");
					return 0;
				}

				/*
				 * get the current directory
				 * */

				char cur_dir[1024];
				memset(cur_dir,0,1024);
				if(getcwd(cur_dir,1024) == NULL) {
					fprintf(stderr,"can't get the current directory.\n");
					free(home);
					return 0;
				}

				if(chdir(home) != 0) {
					fprintf(stderr,"can't change directory.");
					free(home);
					return EUSER;
				}

				free(home);
				if(!create_system_from_txt_file(FSYS)) {
					fprintf(stderr,
							"failed to create restaurant system");
					return 0;
				}
				
				/*change back to the original directory*/
				if(chdir(cur_dir) != 0 ) {	
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
				
				if(get_user_info(username,
							&user_login.home_pth,
							&user_login.uid) == -1) {
					fprintf(stderr,
						"can't get user info upon login\n");
					return 0;
				}
				/*YOU WILL HAVE TO KEEP TRACK OF WHO EVER IS ALREADY LOGGED IN*/
				return S_LOG;
			}

		}
		case LG_EMP:
		{
			
		}
		default:
			printf("unknow instruction.\n");
			return 0;	
	}

	printf("instruction succeed.\n");	
	return 1;
}
