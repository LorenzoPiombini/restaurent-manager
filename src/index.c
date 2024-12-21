#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "date.h"
#include "debug.h"
#include "record.h"
#include "file.h"
#include "common.h"
#include "hash_tbl.h"
#include "str_op.h"
#include "key_op.h"

unsigned char put_data_index_nr(int index_pos, HashTable *ht, char* key)
{

	off_t offset = 0;
	if((offset = get(key,&ht[0])) == -1)
	{
		printf("record not found");
		return 0;
	}

	if(!set(key,offset,&ht[index_pos]))
	{
		printf("indexing failed. %s:%d.\n", F,L-2);
		return IND_FLD; 
	}
	
	return 1;
	
}

unsigned char clean_index_nr(int index_n, HashTable* ht)
{
	size_t l = len(ht[index_n]);
	char** keys_ar = keys(&ht[index_n]);
	int i = 0;
	for(i = 0; i < l; i++)
	{
		Node* node =  delete(keys_ar[i],&ht[index_n]);
		if(!node)
		{
			printf("index clean failed. %s:%d.\n",F,L-3);
			return 0;
		}
		
	}
	
	free_strs(l,1,keys_ar);
	return 1;
}

/*your custom code below this line*/

unsigned char tip_indexing(HashTable *ht, char* key)
{
		/*index 1 today tips*/
		/*index 2 this week tips*/
		/*index 3 last week tips*/

		size_t k_n = len(ht[1]);
		char** keys_ar = keys(&ht[1]);
		if(!keys_ar)
		{
			goto indexing; /*line 137*/
		}

		int i = 0;
		char *date = NULL;
		for(i = 0; i < k_n; i++)
		{
			if(!extract_date(keys_ar[i],&date,ht))
			{
				printf("check integrity index failed. %s:%d.\n",F,L-2);
				free_strs(k_n,1,keys_ar);
				return 0;
			}
			
			if(!is_date_this_week(date))
			{
				
				/*put the last week of tips in the  index nr 3 */
				if((len(ht[2])) > 0)
				{
					if(!copy_ht(&ht[2],&ht[3],1))
					{	
						printf("copying indexes failed, %s:%d.\n",F,L-2);
						free_strs(k_n,1,keys_ar);
						free(date);
						return 0;
					}
				}


				/*clean the index */
				if(!clean_index_nr(1,ht) || !clean_index_nr(2,ht))
				{
					printf("index clean failed. %s:%d.\n",F,L-2);
					free_strs(k_n,1,keys_ar);
					free(date);
					return 0;
				}

				free(date);
				date = NULL;
				break;
			}

			long time = 0;
			if((time = convert_str_date_to_seconds(date)) == -1)
			{
				printf("indexing failed. %s:%d.\n",F,L-2);
				free(date);
				return 0;
			}

			if(!is_today(time))
			{
				
				/*clean the index */
				if(!clean_index_nr(1,ht))
				{
					printf("index clean failed. %s:%d.\n",F,L-2);
					free_strs(k_n,1,keys_ar);
					free(date);
					return 0;
				}

				free(date);
				date = NULL;
				break;
			}
			free(date);
			date = NULL;
		}

		indexing:
		if(keys_ar)
			free_strs(k_n,1,keys_ar);

		/* start indexing */
		/* extract date from key */
		unsigned char result = 1;
		unsigned char result_s = 1;
		
		if((result = put_data_index_nr(1,ht,key)) == IND_FLD || 
			(result_s = put_data_index_nr(2,ht,key)) == IND_FLD)
		{
			printf("indexing failed. %s:%d.\n",F,L-2);
			free(date);
			return 0;
		}
		
		if(result == 0 || result_s == 0)
		{
			printf("indexing failed. %s:%d.\n",F,L-2);
			free(date);
			return 0;
		}
	return 1;
}

unsigned char timecard_indexing(HashTable *ht, char* key)
{
		/*perform indexing for timecard file */
		/*put today's punch card into index 1 */
		/*put the current week into index 2 */
		/*put last week in index 3 */
		size_t k_n = len(ht[1]);
		char** keys_ar = keys(&ht[1]);
		if(!keys_ar)
		{
			unsigned char result = 1;
			unsigned char result_s = 1;
			if((result = put_data_index_nr(1,ht,key)) == IND_FLD ||
				((result_s = put_data_index_nr(2,ht,key)) == IND_FLD))
			{
				printf("indexing failed. %s:%d.\n",F,L-2);
				return 0;
			}
	
			if(result == 0 || result_s == 0)
			{
				printf("record not found for indexing. %s:%d.\n",F,L-8);
				return 0;
			}
			goto indexing_timecard_end; /*line 302*/
		}

		/*check if index 1 contains yesterday, if so clean it*/
		/*we extract the time stamp from the  keys in the first index */
		long time = 0;
		int i = 0;
		for(i =  0; i < k_n; i++)
		{
			if(!extract_time(keys_ar[i],&time))
			{
				printf("extract_time_failed, %s:%d.\n",F,L-2);
				free_strs(k_n, 1, keys_ar);
				return 0;
			}

			if(is_today(time))
				continue;
			
			/*clean index 1 */
			char* date_str = NULL;
			if(!create_string_date(time, &date_str))
			{
				printf("create date string failed, %s:%d.\n",F,L-2);
				free_strs(k_n, 1, keys_ar);
				return 0;
			}

			/* if it is true we are still in the same week*/
			if(is_date_this_week(date_str))
			{
				if(!clean_index_nr(1,ht))
				{
					printf("index clean failed. %s:%d.\n",F,L-2);
					free_strs(k_n, 1, keys_ar);
					free(date_str);
					return 0;
						
				}

				unsigned char result = 1;
				unsigned char result_s = 1;
				if((result = put_data_index_nr(1,ht,key)) == IND_FLD ||
					(result_s = put_data_index_nr(2,ht,key)) == IND_FLD)
				{
					printf("indexing failed. %s:%d.\n",F,L-2);
					free_strs(k_n, 1, keys_ar);
					free(date_str);
					return 0;
				}
	
				if(result == 0 || result_s == 0)
				{
					printf("record not found for indexing. %s:%d.\n",F,L-8);
					free_strs(k_n, 1, keys_ar);
					free(date_str);
					return 0;
				}
				
				
				free(date_str);
			}else 
			{
				if(!copy_ht(&ht[2],&ht[3],1))
				{	
					printf("copying indexes failed, %s:%d.\n",F,L-2);
					free_strs(k_n, 1, keys_ar);
					return 0;
				}

				if(!clean_index_nr(1,ht) || !clean_index_nr(2,ht))
				{
					printf("index clean failed. %s:%d.\n",F,L-2);
					free_strs(k_n, 1, keys_ar);
					return 0;
						
				}

				unsigned char result = 1;
				unsigned char result_s = 1;
				if((result = put_data_index_nr(1,ht,key)) == IND_FLD ||
					(result_s = put_data_index_nr(2,ht,key)) == IND_FLD)
				{
					printf("indexing failed. %s:%d.\n",F,L-2);
					free_strs(k_n, 1, keys_ar);
					return 0;
				}
	
				if(result == 0 || result_s == 0)
				{
					printf("record not found for indexing. %s:%d.\n",F,L-8);
					free_strs(k_n, 1, keys_ar);
					return 0;
				}
				
			}
			goto indexing_timecard_end;/* line 302*/		

		}

		unsigned char result = 1;
		unsigned char result_s = 1;
		if((result = put_data_index_nr(1,ht,key)) == IND_FLD ||
			(result_s = put_data_index_nr(2,ht,key)) == IND_FLD)
		{
			printf("indexing failed. %s:%d.\n",F,L-2);
			free_strs(k_n, 1, keys_ar);
			return 0;
		}

		if(result == 0 || result_s == 0)
		{
			printf("record not found for indexing. %s:%d.\n",F,L-8);
			free_strs(k_n, 1, keys_ar);
			return 0;
		}
		
		indexing_timecard_end:
		free_strs(k_n, 1, keys_ar);

		return 1;
}
