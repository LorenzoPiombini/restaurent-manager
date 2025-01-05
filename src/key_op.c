#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "key_op.h"
#include "common.h"
#include "str_op.h"
#include "debug.h"
#include "record.h"
#include "hash_tbl.h"
#include "date.h"
#include "crud.h"
#include "lock.h"
#include "file.h"



unsigned char assemble_key(char*** key, int n, char c, char* str)
{
	size_t len = number_of_digit(n) + strlen(str) + 2; /* 1 is the char c, and 1 is for '\0' so + 2*/	
	
	*(*key) = calloc(len,sizeof(char));
	if(!*(*key))
	{
		printf("calloc failed. %s:%d",F,L-2);
		return 0;
	}
	
	if(snprintf(*(*key),len,"%c%d%s",c,n,str) < 0)
	{
		printf("key generation failed. %s:%d.\n",F,L-2);
		free(*(*key));
		return 0;
	}
	
	return 1;
}
/*mode is either TIMECARD or TIPS, specify the key that you are extracting the employee key */
unsigned char extract_employee_key(char* key_src, char** key_fnd, int mode)
{
	/*find boundries of employee key*/
	size_t pos = strcspn(key_src,"e");
	size_t len = strlen(key_src);
	
	if(pos == len)
	{
		printf("employee key not found.");
		return 0 ;	
	}

	/*(len - 1)- pos finds the employee key length in the tip keys (the + 1 is for '\0') */
	/*(len - pos) finds the employee key length in the Timecard keys (the + 1 is for '\0') */
	size_t em_k_t = (mode == TIPS) ? ((len - 1) - pos) + 1 : (len - pos) + 1;

	*key_fnd = calloc(em_k_t, sizeof(char));
	if(!*key_fnd)
	{
		printf("calloc failed, %s:%d.\n",F,L-2);
		return 0;
	}

	(*key_fnd)[em_k_t-1] = '\0';
	
	int i = 0, j = 0;
	int end = mode == TIPS ? len - 1 : len;
	for(i = pos, j = 0; i < end; i++)
	{
		(*key_fnd)[j] = key_src[i];
		j++; 
	}
	
	return 1;	
}

unsigned char extract_time(char* key_src, long* time)
{
	
	size_t pos = strcspn(key_src,"e");
	size_t len = strlen(key_src);
	if(pos == len)
	{
		printf("extract time failed. %s:%d.\n",F,L-2);
		return 0;
	}	

	char* end_p = &key_src[pos];	
	*time = strtol(&key_src[2],&end_p,10);
	return 1;
	
}

unsigned char load_files_system(char*** files, int* len)
{
	FILE* fp = fopen("file_sys.txt","r");
	if(!fp)
	{
		printf("failed to open file or it does not exist. %s:%d",F,L-3);
		return 0;
	}

	char buffer[15];
	int i = 0;
	int j = 0;
	while(fgets(buffer,sizeof(buffer),fp)) i++; /* get numbers of lines*/

	*len = i;
	rewind(fp); /* put the file pointer at the beginning */

	*files = calloc(i,sizeof(char*));
	while(fgets(buffer,sizeof(buffer),fp)) {
		buffer[strcspn(buffer,"\n")] = '\0';
		(*files)[j] = strdup(buffer); 
		j++;
	}

	fclose(fp);
	return 1;
}

unsigned char key_generator(struct Record_f *rec, char** key, int fd_data, int fd_index, int lock)
{	
 	char** files = NULL;
	int l = 0, *p_l = &l;
	if(!load_files_system(&files, p_l))
	{
		printf("load files name failed %s:%d.\n",F,L-2);
		return 0;
	}	
	
	if(!files)
	{
		printf("load files name failed %s:%d.\n",F,L-2);
		return 0;
	}
	
	/*generate a key based on the file name in the rec*/
	int i = 0;
	for(i = 0; i < *p_l; i++)
	{
		if(strcmp(files[i],rec->file_name) == 0)
		{
			break;	
		}
	}	

	/* this variables are needed for the lock system */
	int lock_pos_i = 0, *plp_i = &lock_pos_i;
	int lock_pos_arr_i = 0, *plpa_i = &lock_pos_arr_i;

	
	/*acquire WR_IND lock*/
	if(shared_locks && lock == LK_REQ)
	{
		char** file_lck = two_file_path(rec->file_name);
		if(!file_lck)
		{
			printf("%s() failed, %s:%d.\n",__func__,F,L-3);
			free_strs(*p_l,1,files);
			return 0;
		}
	
		int result_i = 0;
		do
		{
			off_t fd_i_s = get_file_size(fd_index,NULL);
 
			if((result_i = acquire_lock_smo(&shared_locks,plp_i,plpa_i,files[0],0,
					fd_i_s,WR_IND,fd_index)) == 0)
			{
				__er_acquire_lock_smo(F,L-5);
				free_strs(*p_l,1,files);
				free_strs(2,1,file_lck);
				if(munmap(shared_locks,
					sizeof(lock_info)*MAX_NR_FILE_LOCKABLE) == -1)
				{
					__er_munmap(F,L-3);
					return 0;
				}
				return 0;
			}
		}while(result_i == MAX_WTLK || result_i == WTLK);

		free_strs(2,1,file_lck);
		file_lck = NULL;
	}


	
	HashTable *ht = NULL;	
	int ht_i = 0, *pht_i = &ht_i;	
	if(!read_all_index_file(fd_index,&ht,pht_i))
	{
		printf("read index failed. %s:%d.\n",F,L-2);
		free_strs(*p_l,1,files);
		if(shared_locks && lock == LK_REQ)
		{/*release the locks before exit*/
			int result_i = 0;
			do
			{
				if((result_i = release_lock_smo(&shared_locks,plp_i,plpa_i)) == 0 )
				{
					__er_release_lock_smo(F,L-3);
					return 0;
				}
			}while(result_i == WTLK);
						
			return 0;
		}
		return 0;
	}

	
	off_t pos = 0;
	switch(i)
	{
		case 0:/*employee*/
			{ 
				char f = return_first_char(rec->file_name); 
				int n = len(ht[0]);

				/*check if the employee already exists*/
				char** keys_a = keys(&ht[0]);
				int j = 0;
				for(j = 0; j < n; j ++)
				{
					if(strstr(keys_a[j],rec->fields[1].data.s) != NULL)
					{
	
						if((pos = get(keys_a[j],ht)) == -1)
						{
							free_strs(n,1,keys_a);
							free_strs(*p_l,1,files);
							free_ht_array(ht,*pht_i);
							printf("check on key failed. record not found. %s:%d.\n",F,L-4);
							return 0;
						}

						if(find_record_position(fd_data,pos) == -1)	
						{
							__er_file_pointer(F,L-2);
							free_strs(n,1,keys_a);
							free_strs(*p_l,1,files);
							free_ht_array(ht,*pht_i);
							return 0;
						}
					
						struct Record_f **rec_e = calloc(1,sizeof(struct Record_f*));
						if(!rec_e)
						{
							printf("calloc failed. %s:%d.\n",F,L-3);
							free_strs(n,1,keys_a);
							free_strs(*p_l,1,files);
							free_ht_array(ht,*pht_i);
							return 0;
						}
						int index = 0, *p_i = &index;	
						if(!get_rec(fd_data,fd_index, p_i, 
							keys_a[j], &rec_e, rec->file_name,lock))
						{
							free_strs(n,1,keys_a);
							free_strs(*p_l,1,files);
							free_ht_array(ht,*pht_i);
							printf("check on key failed. record not found. %s:%d.\n",F,L-4);
							return 0;
						}
				
						if(strcmp(rec_e[0]->fields[0].data.s,rec->fields[0].data.s) == 0)
						{
							free_record_array(*p_i,&rec_e);
							free_strs(n,1,keys_a);
							free_strs(*p_l,1,files);
							free_ht_array(ht,*pht_i);
							printf("Employee already exist.\n");
							return 0;
						}

						free_record_array(*p_i,&rec_e);

						/*this force the loop to go trought all the employee 
							in the case multiple employees share the same last
							name, we are sure that the employee file is accurate*/

						if((n-j) > 1)
							continue;

						/*if you reach this point the employee is new,
						 they share the last name*/
						break;
					}
				}
			
				/* free the keys array and the hash table*/
				free_strs(n,1,keys_a);
				free_ht_array(ht,*pht_i);

				/* this is a new record, generate a new unique key*/

				if(!assemble_key(&key,n,f,rec->fields[1].data.s))
				{
					printf("error in key gen. %s:%d.\n",F,L-2);
					return 0;
				}

				free_strs(*p_l,1,files);
				break;
			}		
		case 1: /* schedule */
			{
				char f = return_first_char(rec->file_name); 
				int n = len(*ht);

				if(!assemble_key(&key,n,f,rec->fields[0].data.s))
				{
					printf("error in key gen. %s:%d.\n",F,L-2);
					free_strs(*p_l,1,files);	
					free_ht_array(ht,*pht_i);
					return 0;
				}
				free_strs(*p_l,1,files);
				free_ht_array(ht,*pht_i);
				break;
			}		
		case 2:/* tips */ 
			{
				char f = return_first_char(rec->file_name); 
				int n = len(ht[0]);
			
					
				size_t lt = strlen(rec->fields[2].data.s) + strlen(rec->fields[4].data.s) + 1;
				lt += number_of_digit(rec->fields[3].data.i);

				char* tip_key_b = NULL;	
				tip_key_b = calloc(lt,sizeof(char));
				if(!tip_key_b)
				{
					printf("calloc failed. %s:%d.\n",F,L-2);
					free_strs(*p_l,1,files);
					free_ht_array(ht,*pht_i);
					return 0;
				}	
			
				if(snprintf(tip_key_b,lt, "%s%s%d",rec->fields[2].data.s, rec->fields[4].data.s,
							rec->fields[3].data.i) < 0)
				{
					printf("tips key builder failed.%s:%d.\n",F,L-2);
					free(tip_key_b);
					free_strs(*p_l,1,files);
					free_ht_array(ht,*pht_i);
					return 0;
				}	

				/* check if the same tip entry exist*/
				char** keys_ar = keys(&ht[1]);
				size_t size = len(ht[1]);
				int j = 0;
				for(j = 0; j < size; j++)
				{
					if(strstr(keys_ar[j],tip_key_b) != NULL)
					{
						char* date = NULL;
						if(!extract_date(keys_ar[j], &date, &ht[0]))
						{
							printf("extract date failed. %s:%d.\n", F,L-2);
							free_strs(size,1,keys_ar);
							free(tip_key_b);
							free_strs(*p_l,1,files);
							free_ht_array(ht,*pht_i);
							return 0;
						}
					 	
						if(strcmp(date,rec->fields[2].data.s) == 0)
						{
							char c = return_last_char(keys_ar[j]);
							int numb = c - '0';
							if(numb == rec->fields[3].data.i)
							{
								printf("tip already saved.\n");
								free_strs(size,1,keys_ar);
								free(tip_key_b);
								free_strs(*p_l,1,files);
								free(date);
								free_ht_array(ht,*pht_i);
								return 0;
							}
						}

						free(date);
					}
				}
			
				free_ht_array(ht,*pht_i);
				free_strs(size,1,keys_ar);
				if(!assemble_key(&key,n,f,tip_key_b))
				{
					printf("error in key gen. %s:%d.\n",F,L-2);
					free(tip_key_b);
					free_strs(*p_l,1,files);
					return 0;
				}
			
				free(tip_key_b);
				free_strs(*p_l,1,files);
				break;		
			}
		case 3: /* percentage */
			{
				size_t buff = number_of_digit(rec->fields[1].data.i) + 1;
				*key = calloc(buff,sizeof(char));
				if(!(*key))
				{
					printf("calloc failed. %s:%d",F,L-3);
					free_strs(*p_l,1,files);
					free_ht_array(ht,*pht_i);
					return 0;
				}
	
				if(snprintf(*key,buff,"%d",rec->fields[1].data.i) > 0)
				{
					printf("error in key gen (snprintf). %s:%d.\n",F,L-2);
					free_strs(*p_l,1,files);
					free_ht_array(ht,*pht_i);
					free(*key);
					return 0;
				}

				free_strs(*p_l,1,files);
				free_ht_array(ht,*pht_i);
				break;
			}		
		case 4:/*shift*/
			{
				char f = return_first_char(rec->file_name); 
				int n = len(*ht);

				if(!assemble_key(&key,n,f,rec->fields[1].data.s))
				{
					printf("error in key gen. %s:%d.\n",F,L-2);
					free_strs(*p_l,1,files);
					free_ht_array(ht,*pht_i);
					return 0;
				}
				free_strs(*p_l,1,files);
				free_ht_array(ht,*pht_i);
				break;
			}	
		case 5:/*user*/
			{
			 	char f = return_first_char(rec->file_name); 
				int n = len(*ht);
				
				size_t buff_l = strlen(rec->fields[0].data.s)
						+ number_of_digit(rec->fields[4].data.l)
						+ 1;
				
				char buff[buff_l];
				memset(buff,0,buff_l);
				if(snprintf(buff,buff_l,"%s%s",rec->fields[0].data.s,
							rec->fields[4].data.l) < 0) {
					fprintf(stderr,
							"snprintf() failed %s:%d.\n",
							F,L-3);
					free_strs(*p_l,1,files);
					return 0;
				}

				if(!assemble_key(&key,n,f,buff)) {
					printf("error in key gen. %s:%d.\n",F,L-2);
					free_strs(*p_l,1,files);
					return 0;
				}

				free_strs(*p_l,1,files);
				break;
			}
		case 6:/*timecard*/
			{
				char f = 'm';
				int n = len(ht[1]);
				if(n == 0)
				{
					goto build_tc_key; /* line 1452 */	
				}

				char** keys_ar = keys(&ht[1]);
				if(!keys_ar)
				{
					printf("fetch keys faield.%s:%d.\n",F,L-2);
					free_strs(*p_l,1,files);
					free_ht_array(ht,*pht_i);
					return 0;
				}

				int j = 0;
				for(j = 0; j < n; j++)
				{
					if(strstr(keys_ar[j], rec->fields[2].data.s) == NULL)
						continue;

					long time = 0;	
					if(!extract_time(keys_ar[j],&time))
					{
						printf("can`t extract time. %s:%d.\n",F,L-2);
						free_strs(*p_l,1,files);
						free_strs(n,1,keys_ar);
						free_ht_array(ht,*pht_i);
						return 0;
					}
					
					/* now you have the seconds, you should create a tm struct */
					/* you can compare it against the value in the record */
					/* if the day is the same and clock out is 0 then */
					/* no clock in should be allowed*/

					if(!is_today(time))
						continue;					
					
						
					/* check if the employee clocked out if not exit clock in*/
					
					if((return_first_char(keys_ar[j])) == 'm')
					{
						printf("employee already clocked in\n");
						free_strs(*p_l,1,files);
						free_strs(n,1,keys_ar);
						free_ht_array(ht,*pht_i);
						return 0;	
					}
					
				
				}	
				
				free_strs(n,1,keys_ar);
				build_tc_key:
				
				free_ht_array(ht,*pht_i);

				size_t sz = number_of_digit(rec->fields[0].data.l) + strlen(rec->fields[2].data.s) + 1;
				char* time_card_b = NULL; 
				time_card_b = calloc(sz, sizeof(char));
				if(!time_card_b)
				{
					printf("calloc failed. %s:%d.\n", F,L-2);
					free_strs(*p_l,1,files);
					return 0;
				}
	
				if(snprintf(time_card_b,sz, "%ld%s", rec->fields[0].data.l, rec->fields[2].data.s) < 0)
				{
					printf("time_card key building failed %s:%d.\n",F,L-2);
					free(time_card_b);
					free_strs(*p_l,1,files);
					return 0;
				}
				
				/*get_service() returns either  0,1 or 2 (breakfast,lunch or dinner)*/	
				if(!assemble_key(&key,get_service(),f,time_card_b))
				{
					printf("error in key gen. %s:%d.\n",F,L-2);
					free_strs(*p_l,1,files);
					return 0;
				}
			free(time_card_b);	
			free_strs(*p_l,1,files);
			break;	
			}
		default:
			printf("no cases for key gen. %s:%d.\n",F,L);
			free_strs(*p_l,1,files);
			free_ht_array(ht,*pht_i);
			return 0;
	}

	if(shared_locks && lock == LK_REQ)
	{/*release the lock normally*/
		int result_i = 0;
		do
		{
			if((result_i = release_lock_smo(&shared_locks,plp_i,plpa_i)) == 0 )
			{
				__er_release_lock_smo(F,L-3);
				return 0;
			}
		}while(result_i == WTLK);
						
	}

	return 1;
}

