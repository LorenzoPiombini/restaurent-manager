#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "timecard.h"
#include "file.h"
#include "lock.h"
#include "debug.h"
#include "str_op.h"
#include "key_op.h"
#include "crud.h"
#include "index.h"
#include "common.h"


unsigned char clock_in(char* employee_id)
{
	time_t now = time(NULL);

	int fd_tcd = open_file("timecard.dat",0);
	if(file_error_handler(1,fd_tcd) > 0)
	{
		printf("can't open timecard files. %s:%d\n", F,L-2);
		return 0;
	}

	size_t digits = number_of_digit((long)now);
	char* clock_in_field = "clock_in:t_l:";
	char* emplo_field = ":employee_id:t_s:";
	size_t len = strlen(clock_in_field) + digits + strlen(emplo_field) + strlen(employee_id) + 1;

	char* data_to_add = calloc(len,sizeof(char));
	if(!data_to_add)
	{
		printf("calloc failed. %s:%d.\n",F,L-3);
		close_file(1,fd_tcd);
		return 0;
	} 	

	if(snprintf(data_to_add,len,"%s%ld%s%s",clock_in_field,now,emplo_field,employee_id)<0)
	{
		printf("data_to_add failed.%s:%d.\n",F,L-2);
		close_file(1,fd_tcd);
		free(data_to_add);
		return 0;
	}
	
		
	if(!__write_safe(fd_tcd,data_to_add,"timecard",NULL))
	{
		printf("__write_safe failed, %s:%d.\n",F,L-2);
		close_file(1,fd_tcd);
		free(data_to_add);
		return 0;
	}

	free(data_to_add);
	close_file(1,fd_tcd);
	return 1;
}

unsigned char clock_out(char* employee_id)
{
	/* open the index file */
	
	int fd_tci = open_file("timecard.inx",0);
	if(file_error_handler(1,fd_tci) > 0)
	{
		printf("can't open timecard files. %s:%d\n", F,L-2);
		return 0;
	}	
	
	HashTable *ht = NULL;
	int i = 0 , *pi = &i;
	if(!read_indexes("timecard", fd_tci, -1, ht,pi))
	{
		printf("read indexes failed, %s:%d.\n",F,L-2);
		close_file(1,fd_tci);
		return 0;	
	}

	size_t n = len(ht[1]);
	if(n == 0)
	{
		printf("no clock in yet.\n");
		free_ht_array(ht,*pi);	
		close_file(1,fd_tci);
		return 0;
	}

	char** keys_ar = keys(&ht[1]);
	if(!keys_ar)
	{
		printf("no clock in yet.\n");
		free_ht_array(ht,*pi);	
		close_file(1,fd_tci);
		return 0;
	}

	int j = 0, fnd = 0, pos[n];
	for(j = 0; j < n; j++)
	{
		if(strstr(keys_ar[j],employee_id) == NULL)
		{
			pos[j] = -1;
			continue;
		}else
		{	
			fnd++;
			pos[j] = j;
		}
	}

	if(fnd == 0)
	{
		printf("no clock in for this employee.\n");
		free_ht_array(ht,*pi);	
		free_strs(n,1,keys_ar);
		close_file(1,fd_tci);
		return 0;
	}

	/* open the data file to perform the clockout */
	
	int fd_tcd = open_file("timecard.dat",0);
	if(file_error_handler(1,fd_tcd) > 0)
	{
		printf("can't open timecard files, %s:%d.\n",F,L-3);
		free_ht_array(ht,*pi);	
		free_strs(n,1,keys_ar);
		close_file(1,fd_tci);
		return 0;
	}
	

	for(j = 0; j < n; j++)
	{
		if(pos[j] == - 1)
			continue;
		

		if(return_first_char(keys_ar[pos[j]]) == 'o')
		{
			fnd--;
			continue;
		} else
		{
			break;
		}

	}
	
	if(fnd == 0)
	{
		printf("employee already clocked out.\n");	
		free_ht_array(ht,*pi);	
		close_file(2,fd_tcd,fd_tci);
		free_strs(n,1,keys_ar);
		return 0;
	}

 	char* clock_o_field = "clock_out:t_l:";
	time_t now = time(NULL);
	size_t size = number_of_digit(now) + strlen(clock_o_field) + 1;
	char* data_to_add = calloc(size,sizeof(char));
	if(!data_to_add)
	{
		printf("calloc failed. %s:%d.\n",F,L-3);
		close_file(2,fd_tcd,fd_tci);
		free_ht_array(ht,*pi);	
		free_strs(n,1,keys_ar);
		return 0;
	} 

	if(snprintf(data_to_add,size,"%s%ld",clock_o_field,now) < 0)
	{
		printf("create data to update failed, %s:%d.\n",F,L-2);
		close_file(2,fd_tcd,fd_tci);
		free_ht_array(ht,*pi);	
		free(data_to_add);
		free_strs(n,1,keys_ar);
		return 0;
	}

	if(!__update("timecard",fd_tci,fd_tcd,data_to_add,keys_ar[pos[j]]))
	{
		printf("update record failed. %s:%d.\n", F,L-2);
		free_ht_array(ht,*pi);	
		close_file(2,fd_tcd,fd_tci);
		free(data_to_add);
		free_strs(n,1,keys_ar);
		return 0;
	}
	
	free(data_to_add);
	close_file(1,fd_tcd);

	char* key_n = strdup(keys_ar[pos[j]]);
	if(!key_n)
	{
		printf("strdup failed. %s:%d.\n",F,L-3);
		free_ht_array(ht,*pi);	
		free_strs(n,1,keys_ar);
		return 0;	
	}
	
	/* delete the key from all the indexes */	
	/* repopulate the indexes with the new key */
	off_t off_v = 0;
	key_n[0] = 'o';	
	int j_old = j;
	for(j = 0; j < *pi; j++)
	{
		Node* node = NULL;
		if((node = delete(keys_ar[pos[j_old]],&ht[j])) == NULL)
			continue;	
		
		if(off_v == 0)
			off_v = node->value;

		free(node->key);
		free(node);

		if(!set(key_n,off_v,&ht[j]))
		{
			printf("set key failed, %s:%d.\n",F,L-2);
			free_ht_array(ht,*pi);
			free(key_n);	
			free_strs(n,1,keys_ar);
			close_file(1,fd_tci);
			return 0;
		}
	}

	free(key_n);	
	
	int lock_pos_i = 0, *plp_i = &lock_pos_i;
	int lock_pos_arr_i, *plpa_i = &lock_pos_arr_i;
 
	if(shared_locks)
	{
		int result_i = 0;
		do
		{		
			off_t fd_i_s = get_file_size(fd_tci,NULL);
			if(fd_i_s == - 1)
			{
				printf("can't get file size, %s:%d.\n",F,L-3);
				close_file(1,fd_tci);
				free_ht_array(ht,*pi);	
				return 0;	
			}

			if((result_i = acquire_lock_smo(&shared_locks,plp_i,plpa_i,
				"timecard.inx",0,fd_i_s,WR_IND,fd_tci)) == 0)
			{
				__er_acquire_lock_smo(F,L-3);
				close_file(1,fd_tci);
				free_ht_array(ht,*pi);	
				return 0;
			}
						
		}while(result_i == WTLK || result_i == MAX_WTLK);	
	}

	close_file(1,fd_tci);
	fd_tci = open_file("timecard.inx",1); // open with O_TRUNC
	if(file_error_handler(1,fd_tci) > 0)
	{
		printf("can't open timecard files, %s:%d.\n",F,L-3);
		free_ht_array(ht,*pi);	
		free_strs(n,1,keys_ar);
		if(shared_locks)
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

	if(!write_all_indexes(ht,fd_tci,*pi))
	{
		free_ht_array(ht,*pi);
		free_strs(n,1,keys_ar);
		close_file(1,fd_tci);
		if(shared_locks)
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

	free_ht_array(ht,*pi);	
	free_strs(n,1,keys_ar);
	close_file(1,fd_tci);

	if(shared_locks)
	{/*release the lock normaly*/
		int result_i = 0;
		do
		{
			if((result_i = release_lock_smo(&shared_locks,plp_i,plpa_i)) == 0 )
			{
				__er_release_lock_smo(F,L-3);
				return 0;
			}
		}while(result_i == WTLK);
		return 1;	
	}


	return 1;
}

unsigned char list_clocked_in_employee(char*** employee_list, int* size)
{
	/*read time card index 1 to see the employee that are working */
	int fd_tci = open_file("timecard.inx",0);
	if(file_error_handler(1,fd_tci) > 0)
	{
		printf("error opening timecard file %s:%d.\n",F,L-3);
		return 0;
	}

	HashTable ht = {0,NULL};
	if(!read_indexes("timecard",fd_tci,1,&ht,NULL))
	{
		printf("read from file failed, %s:%d.\n", F,L-2);
		return 0;
	}

	close_file(1,fd_tci);

	size_t ht_size = len(ht);
	char** keys_ar = keys(&ht);
	destroy_hasht(&ht);

	int i = 0, sz = 0;

	/*keeping only the employee clocked in*/
	for(i = 0; i < ht_size; i++)
	{
		register char c = return_first_char(keys_ar[i]);
		if( c != 'm' )
		{	
			free(keys_ar[i]);
			keys_ar[i] = NULL;
			continue;
		}
		sz++;
	}
	
	if(sz == 0)
	{
		printf("no employee to tip.\n");
		free_strs(ht_size,1,keys_ar);
		return NO_EMPLOYEE_CLOCKED_IN;
	}

	*size = sz;	
	*employee_list = calloc(sz,sizeof(char*));
	if(!employee_list)
	{
		printf("calloc faield, %s:%d.\n",F,L-3);
		free_strs(ht_size,1,keys_ar);
		return 0;
	}
	int j = 0;
	for(i = 0; i < ht_size; i ++)
	{
		if(!keys_ar[i])
			continue;

		char* key_fnd = NULL;
		if(!extract_employee_key(keys_ar[i],&key_fnd,TIMECARD))
		{
			printf("extract_employee_key failed, %s:%d.\n",F,L-2);
			free_strs(ht_size,1,keys_ar);
			return 0;
		}
			

		(*employee_list)[j] = strdup(key_fnd);
		free(key_fnd);
		j++;
	}
	
	free_strs(ht_size,1,keys_ar);
	return 1;
}
