#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "business.h"
#include "bst.h"
#include "file.h"
#include "str_op.h"
#include "common.h"
#include "parse.h"
#include "key_op.h"
#include "debug.h"
#include "record.h"
#include "hash_tbl.h"
#include "crud.h"
#include "bst.h"
#include "date.h"
#include "record.h"
#include "index.h"
#include "timecard.h"
#include "restaurant_t.h"
#include "json.h" /* for JsonPair */

unsigned char get_tips_employees_week(int week, int tips_index, int tips_data, struct Record_f ****recs_tr,
					 int **sizes, int *pr_sz)
{
	if(!is_a_db_file(tips_data,"tips",NULL))
	{
		printf("not a db file or header reading error. %s:%d.\n",F,L-2);
		return 0;	
	}
	
	/*week rappresent the index nr in the tips file	*/
	/*- index 1 (today) macro -> TODAY	       	*/
	/*- index 2 (current week) macro -> CUR_WK     	*/
	/*- index 3 (last week) macro-> LST_WK		*/
	HashTable ht= {0,NULL};
	if(!read_indexes("tips",tips_index,week,&ht,NULL))
	{
		printf("read_indexes failed %s:%d.", F,L-2);
		return 0;
	}

	int elements = len(ht), i = 0;
	*pr_sz = elements;
	char** keys_ar = keys(&ht);
	if(!keys_ar)
	{
		printf("index is empty. %s:%d.\n", F,L-2);
		destroy_hasht(&ht);
		return 0;
	}

	destroy_hasht(&ht);

	*recs_tr = calloc(elements,sizeof(struct Record_f**));
	if(!*recs_tr)
	{
		printf("calloc failed, %s:%d.\n", F,L-3);
		free_strs(elements,1,keys_ar);
		return 0;
	}

	*sizes = calloc(elements,sizeof(int));
	if(!*sizes)
	{
		printf("calloc failed, %s:%d.\n", F,L-3);
		free_strs(elements,1,keys_ar);
		free(*recs_tr);
		return 0;
	}

	for(i = 0; i < elements; i++)
	{
		int index = 0, *p_i = &index;
		struct Record_f **recs_t = calloc(1,sizeof(struct Record_f*));
		if(!recs_t)
		{
			printf("calloc failed. %s:%d.\n",F,L-3);
			free_strs(elements,1,keys_ar);
			free_array_of_arrays(elements,recs_tr,*sizes,i);
			free(sizes);
			return 0;
		}
		
							
		if(!get_rec(tips_data,tips_index,p_i,keys_ar[i],&recs_t,"tips",LK_REQ))	
		{
			fprintf(stderr,"read failed. %s:%d\n", F,L-2);
			free_strs(elements,1,keys_ar);
			free_record_array(1,&recs_t);
			free_array_of_arrays(elements,recs_tr,*sizes,i);
			free(sizes);
			return 0;
		}
		
		if(*p_i == 1)
		{	
			(*recs_tr)[i] = recs_t;
			(*sizes)[i] = *p_i; 
		}else
		{
			/*here you have to account for the case where the record is contained*/
			/*in 2 or more position in the array */
			/*this should never happened for this system, becuase the user won't */
			/*have the ability to change the file definiton in the restaurant app */
			printf("code refactor needed, %s:%d.\n",F,L);
			free_strs(elements,1,keys_ar);
			free_record_array(1,&recs_t);
			free_array_of_arrays(elements,recs_tr,*sizes,i);
			free(sizes);
			return 0;
		}
	}

	free_strs(elements,1,keys_ar);
	return 1;
}


unsigned char filter_tip_service(int index,int fd_index, int fd_data, struct Record_f ***recs, struct Record_f ****result,
				struct Record_f ****filtered_r, int service, int *ps,
				int **sizes,int result_size, int **sizes_fts)
{
	if(!(*result))
	{
		if(index < 0)
		{
			printf("index value %d, is invalid.\n",index);
			return 0;
		}

		HashTable ht= {0,NULL};
		if(!read_indexes("tips",fd_index,index,&ht,NULL))
		{
			printf("read_indexes failed %s:%d.", F,L-2);
			return 0;
		}
	
		size_t l = len(ht);
		char** keys_ar = keys(&ht);
		if(!keys_ar)	
		{
			printf("get keys from hash table failed, %s:%d.\n",F,L-3);
			destroy_hasht(&ht);
			return 0;
		}

		destroy_hasht(&ht);

		int i = 0, size = 0, j = 0; 
		for(i = 0; i < l; i++)
		{
			char lc = return_last_char(keys_ar[i]);
			int num = lc - '0';
			if(service == num)
				size++;
		}

		if(size == 0)
		{
			free_strs(l,1,keys_ar);
			printf("there is no entry for this service.\n");
			return 0;	
		}		

		*ps = size;


		int index_i = index, *pi = &index_i;

		*result = calloc(size,sizeof(struct Record_f**));
		if(!*result)
		{
			printf("calloc failed. %s:%d.\n",F,L-2);
			free_strs(l,1,keys_ar);
			return 0;
		}
	
		*sizes = calloc(size,sizeof(int));
		if(!*sizes)
		{
			printf("calloc failed. %s:%d.\n",F,L-3);
			free(*result);
			free_strs(l,1,keys_ar);
			return 0;
		}
		
		/* get the data from the file */
		if(fd_index != -1 && fd_data != -1 )
		{
			for(i = 0, j = 0; i < l; i++)
			{
				char lc = return_last_char(keys_ar[i]);
				int num = lc - '0';
				if(service != num)
					continue;

				*recs = calloc(1,sizeof(struct Record_f*));
				if(!*recs)
				{
					printf("calloc failed. %s:%d.\n",F,L-2);
					free_array_of_arrays(size,result,*sizes,i);
					free(sizes);
					free_strs(l,1,keys_ar);
					return 0;
				}

				if(!get_rec(fd_data,fd_index,pi,keys_ar[i],recs,"tips",LK_REQ))
				{
					printf("get_rec failed. %s:%d.",F,L-2);
					free_array_of_arrays(size,result,*sizes,i);
					free(sizes);
					free_strs(l,1,keys_ar);
					return 0;
				}

				/*put the get_rec output in the reslut array*/
				(*result)[j] = *recs;	

				/*populate the array of all records sizes*/

				(*sizes)[j] = *pi;
		
				/* reset the index value to the parameter 
				to get the right result from get_rec function*/
				*pi = index; 
				j++;
			}
		}else
		{
			printf("check file descriptor, %s:%d.\n",F,L-36);
			free(*result);
			free(*sizes);
			free_strs(l,1,keys_ar);
			return 0;
		}
		
		free_strs(l,1,keys_ar);
	}else{/*you already have data so you do not have to read from a file*/
		int i = 0, size = 0;
		for(i = 0; i < result_size; i++)
		{
			if((*result)[i][0]->fields[3].data.i == service)
				size++;
		}
		
		
		*filtered_r = calloc(size,sizeof(struct Record_f**));
		if(!*filtered_r)
		{
			printf("calloc failed, %s:%d.\n",F,L-3);
			return 0;
		}
		
		*sizes_fts = calloc(size,sizeof(int));
		if(!sizes_fts)
		{
			printf("calloc failed, %s:%d.\n",F,L-3);
			free(*filtered_r);
			return 0;
		}
		
		*ps = size;
		int j = 0;
		for(i = 0, j = 0; i < size; i++)
		{
				
			/* perform a deep copy of the data to comply with memory management rules*/
			struct Record_f *dest_rec = NULL;
			if(!copy_rec((*result)[i][0],&dest_rec))
			{
				printf("deep copy failed, %s:%d.\n",F,L-2);
				free_array_of_arrays(size,filtered_r,*sizes_fts,i);
				free(*sizes_fts);
				return 0;	
			}
			
			struct Record_f **pdest_rec_cpy = calloc(1,sizeof(struct Record_f*));
			if(!pdest_rec_cpy)
			{
				printf("calloc failed, %s:%d.\n",F,L-3);
				free_array_of_arrays(size,filtered_r,*sizes_fts,i);
				free(*sizes_fts);
				return 0;	
			}
			
			*pdest_rec_cpy = dest_rec;		
			(*filtered_r)[j] = pdest_rec_cpy;
			(*sizes_fts)[j] = (*sizes)[i];
			j++;
		}
	}

	return 1;
}


unsigned char total_credit_card_tips_each_employee(struct Record_f ****result, int size, int *sizes,
					 float **totals, char ***emp_k,	int *pemp_t, int *ptot_t)
{
	if(size <= 1) return 0;
	
	/*creates array to  contain each employee key if the key is in the array we have a subtotal */
	
	size_t emp_t = 1;
	*emp_k = calloc(emp_t,sizeof(char*));
	if(!*emp_k)
	{
		printf("calloc failed, %s:%d.\n",F,L-3);
		return 0;	
	}
	
	/* array of totals */
	size_t tot_t = 1;
	*totals = calloc(tot_t,sizeof(float));
	if(!*totals)
	{
		printf("calloc failed.\n");
		free_strs(emp_t-1,1,*emp_k);
		return 0;
	}
	
	char** emp_k_n = NULL;
	float* tot_n = NULL;
	
	(*emp_k)[0] = "null";/*just to give a value to avoid seg fault*/
	int i = 0, j = 0, k = 0, fnd = 0;
	for(i = 0; i < size; i++)
	{
		for(k = 0; k < sizes[i]; k++)
		{
			for(j = 0; j < emp_t; j++)
			{
				if(strcmp((*result)[i][k]->fields[4].data.s,(*emp_k)[j]) == 0)
				{
					fnd++;
					k = 0;
					break; 
				}
				
			}

			if(fnd == 0)
			{
				if(i != 0)
				{
					emp_t++;
					*pemp_t = emp_t;
					emp_k_n = realloc(*emp_k, emp_t * sizeof(char*));

					if(!emp_k_n)
					{
						printf("realloc failed. %s:%d.\n", F,L-3);
						free_strs(emp_t-1,1,*emp_k);
						free(totals);
						return 0;
					}

					*emp_k = emp_k_n;
					(*emp_k)[emp_t-1] = strdup((*result)[i][k]->fields[4].data.s);
					
					tot_t++;
					*ptot_t = tot_t;
					tot_n = realloc(*totals, tot_t * sizeof(float));
					if(!tot_n)
					{
						printf("realloc failed. %s:%d.\n", F,L-3);
						free_strs(emp_t-1,1,*emp_k);
						free(totals);
						return 0;
					}

					*totals = tot_n;
					(*totals)[tot_t-1] = (*result)[i][k]->fields[0].data.f; 
					
		
				}else
				{
					(*emp_k)[emp_t-1] = strdup((*result)[i][k]->fields[4].data.s);
					(*totals)[tot_t-1] = (*result)[i][k]->fields[0].data.f; 
				}
			}else
			{
				/*if the program reaches this else it means we already have a subtotals*/
				/* so we need to add to it, at this point the value of j is the position*/
				/* of the subtotal that we need to increment*/
				(*totals)[j] += (*result)[i][k]->fields[0].data.f;
				j = 0;
	
			
			}

			fnd = 0;
		}
	}
	
	return 1; //succeed
}



unsigned char compute_tips(float credit_card_tips, float cash_tips, int service, char **employee_list, int size)
{

	int fd_ei = open_file("employee.inx",0);
	int fd_ed = open_file("employee.dat",0);
	int fd_pi = open_file("percentage.inx",0);
	int fd_pd = open_file("percentage.dat",0);
	if(file_error_handler(4,fd_ei,fd_ed,fd_pi,fd_pd) > 0)
	{
		printf("error opening timecard file %s:%d.\n",F,L-3);
		return 0;
	}

	/*create a AVL binary search tree, to store the data and increase performance*/	
	BST binary_st = {NULL,insert_bst};

	/*this pointer is important, rappresent the data
		that each node in the BST will carry*/
	int* prole = NULL, role = 0;

	int index = 0, *p_index = &index;
	int servers = 0; /*counter to divide cash tips*/
	struct Record_f **recs = NULL;
	float point_role[size];
	int roles[size];
	char percentage_k[4];/*this is for the percentage key*/
	float points = 0;
	
	for(int i = 0; i < size; i++)
	{
		/*get the employee record and get the role*/
		recs = calloc(1,sizeof(struct Record_f*));
		if(!recs)
		{
			printf("calloc failed, %s:%d.\n",F,L-3);
			close_file(4,fd_ei,fd_pi,fd_ed,fd_pd); 
			if(binary_st.root)
				free_BST(&binary_st);	
			return 0;
		}

		if(!get_rec(fd_ed,fd_ei,p_index,employee_list[i],&recs,"employee",LK_REQ))
		{
			printf("get_rec failed %s:%d.\n",F,L-2);
			free_record_array(*p_index,&recs);
			close_file(4,fd_ei,fd_pi,fd_ed,fd_pd); 
			if(binary_st.root)
				free_BST(&binary_st);	
			return 0;
		}

		if(*p_index == 1)
		{
			/*get the role that would be the key in the percentage file*/
			if(snprintf(percentage_k,3,"%d",recs[0]->fields[3].data.i) < 0)
			{
				free_record_array(*p_index,&recs);
				close_file(4,fd_ei,fd_pi,fd_ed,fd_pd);
				if(binary_st.root)
					free_BST(&binary_st);	
				return 0;
			}
			
			
			if(recs[0]->fields[3].data.i == SERVER)
				servers++;

			roles[i] = recs[0]->fields[3].data.i;
			
			role = recs[0]->fields[3].data.i;
			prole = &role; 
			if(!binary_st.insert(t_s,(void*)employee_list[i],&binary_st.root,(void**)&prole,t_i))
			{
				printf("BST insertion failed, %s:%d.\n",F,L-2);
				free_record_array(*p_index,&recs);
				close_file(4,fd_ei,fd_pi,fd_ed,fd_pd);
				if(binary_st.root)
					free_BST(&binary_st);	
				return 0;
			}
				
			
		}else
		{
			/*here you have to account for the case where the record is contained*/
			/*in 2 or more position in the array */
			/*this should never happened for this system, becuase the user won't */
			/*have the ability to change the file definitons in the restaurant app */
			/*this is an helper function to find where is the fields that we are looking for*/
			/*	int field_pos = 0, *pfield_pos = &field_pos, rec_i = 0, *prec_i = &rec_i;
			 	if(!get_index_rec_filed("role", recs, *p_index, pfield_pos, prec_i))
			 	{
			 		handle the error
			 	}
			*/
			printf("code refactor needed, %s:%d.\n",F,L);
			free_record_array(*p_index,&recs);
			close_file(4,fd_ei,fd_pi,fd_ed,fd_pd);
			if(binary_st.root)
				free_BST(&binary_st);	
			return 0;
		}

		free_record_array(*p_index,&recs);
		recs = NULL;
		*p_index = 0;

		recs = calloc(1,sizeof(struct Record_f*));	
		if(!recs)
		{
			printf("calloc failed, %s:%d.\n",F,L-3);
			close_file(4,fd_ei,fd_pi,fd_ed,fd_pd); 
			if(binary_st.root)
				free_BST(&binary_st);	
			return 0;
		}

		/*get the points from the percentage file and sum it up*/
		if(!get_rec(fd_pd,fd_pi,p_index,percentage_k,&recs,"percentage",LK_REQ))
		{
			printf("get_rec failed %s:%d.\n",F,L-2);
			free_record_array(*p_index,&recs);
			close_file(4,fd_ei,fd_pi,fd_ed,fd_pd); 
			if(binary_st.root)
				free_BST(&binary_st);	
			return 0;
		}

		if(*p_index == 1)
		{
			points += recs[0]->fields[0].data.f;
			/*store the point in the array */
			point_role[i] = recs[0]->fields[0].data.f;
		}else
		{
			/*here you have to account for the case where the record is contained*/
			/*in 2 or more position in the array */
			/*this should never happened for this system, becuase the user won't */
			/*have the ability to change the file definiton in the restaurant app */
			printf("code refactor needed, %s:%d.\n",F,L);
			free_record_array(*p_index,&recs);
			close_file(4,fd_ei,fd_pi,fd_ed,fd_pd);
			if(binary_st.root)
				free_BST(&binary_st);	
			return 0;
		}
		
		free_record_array(*p_index,&recs);
		recs = NULL;
		*p_index = 0;
	}

	close_file(4,fd_ei,fd_pi,fd_ed,fd_pd);

	/*transform floating point numbers in integer to achive precision */
	unsigned char all_cash_tips = 0;

	if(credit_card_tips == 0)
		credit_card_tips = cash_tips, cash_tips = 0.0, all_cash_tips = 1;

	int cc_tips = (int)floorf((credit_card_tips * 100));

	int csh_tips = 0; 
	if(cash_tips > 0)
		csh_tips = (int)floorf(cash_tips * 100);


	/*building blocks for the tips record*/
	char* amt = "credit_card:t_f:", *dt = ":date:t_s:", *emply = ":employee_id:t_s:";
	char* srvc = ":service:t_i:", *cash_tp = ":cash:t_f:";
	size_t pre_size = 0; 

	if(cash_tips > 0)
	{	
		pre_size = strlen(amt) + strlen(emply) + strlen(dt) + strlen(srvc) + strlen(cash_tp);
	}else if(cash_tips == 0 && all_cash_tips)
	{
		cash_tp = "cash:t_f:";
		pre_size = strlen(emply) + strlen(dt) + strlen(srvc) + strlen(cash_tp);
	}else
	{
		pre_size = strlen(amt) + strlen(emply) + strlen(dt) + strlen(srvc);
	}

	/* with shr you can compute the tips */
	int shr = (cc_tips + csh_tips) / points;

	int fd_td = open_file("tips.dat",0);
	if(file_error_handler(1,fd_td) > 0)
	{
		printf("error opening timecard file %s:%d.\n",F,L-3);
		free_BST(&binary_st);	
		return 0;
	}

	int srv_ct = 0, cents_ct = 0;
	unsigned char cents_ct_given = 0;
	if(cash_tips > 0)
	{
		if(servers > 1)
		{	
			srv_ct = csh_tips /servers;
			cents_ct = csh_tips - (srv_ct*servers);
		}else 
		{
			srv_ct = csh_tips;
		}
	}
	
	int tip_amt = 0, csh_amt_srv = 0, tip_cc_srv = 0;
	unsigned char split_cash_among_others = 0; /* flag to split cash tips */ 
	for(int j = 0; j < size; j++)
	{
		int tip_part = point_role[j] * shr; 
		tip_amt += tip_part; 
		if(roles[j] == SERVER)
		{	
			if(!all_cash_tips)	
			{
				tip_cc_srv = point_role[j] * shr;
				if(servers == 1 && srv_ct > tip_cc_srv)
				{
					csh_amt_srv = (int)(floorf(tip_cc_srv / 100)*100);
					int temp = csh_tips - csh_amt_srv;
					tip_cc_srv -= csh_amt_srv;
					split_cash_among_others = 1;
					srv_ct = temp / (size-servers);
					cents_ct = csh_tips - ((srv_ct*(size-servers)) + csh_amt_srv) ;
				}
			}

		}
	}

	int cents = (cc_tips + csh_tips) - tip_amt;
	
	/*generate random number between 0 and size-1*/
	srand(time(NULL));
	int random_n = rand() % size;	

	/* loop  the employees that have been working today and split the tips*/
	for(int i = 0; i < size; i++)
	{
		register int tip_cc = 0;
		register int  csh_amt = 0;
		register size_t digit = 0;
		int* pdata_from_the_node = NULL; 

		if(!find(t_s,(void*)employee_list[i],&binary_st.root,(void**)&pdata_from_the_node,t_i))
		{
			printf("no node contains \"%s\" in the tree.\n",employee_list[i]);
			free_BST(&binary_st);
		}

		switch(*pdata_from_the_node)
		{
			case BUSSER:
			{
				int tip_amt_b = point_role[i] * shr;
				if((digit = digits_with_decimal(tip_amt_b/100)) == - 1)
				{
					printf("compute tips failed %s:%d.\n",F,L-2);
					free_BST(&binary_st);
					close_file(1,fd_td);
					return 0;
				}
				tip_cc = tip_amt_b;
				break;
			}
			case SERVER:
			{	
				if(!split_cash_among_others)
				{
					int tip_amt_s = point_role[i] * shr;
					if((digit = digits_with_decimal(tip_amt_s/100)) == - 1)
					{
						printf("compute tips failed %s:%d.\n",F,L-2);
						close_file(1,fd_td);
						free_BST(&binary_st);
						return 0;
					}
					tip_cc = tip_amt_s;
				}
				else
				{
					tip_cc = tip_cc_srv;
					csh_amt = csh_amt_srv;
				}
				break;
			}
			case FOOD_RUNNER:
				{
					int tip_amt_fr = point_role[i] * shr;
					if((digit = digits_with_decimal(tip_amt_fr/100)) == - 1)
					{
						printf("compute tips failed %s:%d.\n",F,L-2);
						free_BST(&binary_st);
						close_file(1,fd_td);
						return 0;
					}
					tip_cc = tip_amt_fr;
					break;
				}
			default:
				break;	
		}	

		
		if(*pdata_from_the_node == SERVER)
		{
			if(servers > 1 && !cents_ct_given && !all_cash_tips)
			{
				cents_ct_given = 1;
						
				csh_amt = srv_ct;
				csh_amt += cents_ct;
				tip_cc -= csh_amt;
			}
			else if(servers == 1 && !cents_ct_given && !all_cash_tips)
			{
				cents_ct_given = 1;
				csh_amt+= cents_ct;
			}
			else if(!all_cash_tips)
			{
				csh_amt = srv_ct;
				tip_cc -= srv_ct;
			}
		}

		if(split_cash_among_others && (*pdata_from_the_node != SERVER))
		{
			csh_amt = srv_ct;
			tip_cc -= csh_amt;
		}

		int csh_digit = 0;
		
		if(csh_amt > 0 || (csh_amt == 0 && all_cash_tips))
		{
			csh_amt = tip_cc;
			if((csh_digit = digits_with_decimal(csh_amt/100)) == -1)
			{
				printf("compute tips failed %s:%d.\n",F,L-2);
				free_BST(&binary_st);
				close_file(1,fd_td);
				return 0;
			}
		}
		
		/* digit and csh_digit are  the sizes of the floating point number including the '.' 
		 	meaning if the float is 20.898080980 the digit value will be 3         
		 	the program will be using always only 2 decimal after the '.'    
		 	(digit + 2) account for the 2 decimal number after the '.' */	   
	
		if(csh_amt > 0 && !all_cash_tips)
		{
			pre_size += ((digit + 2) + (csh_digit + 2) + strlen(employee_list[i])); 
		}
		else if(all_cash_tips)
		{
			pre_size += ( (csh_digit + 2) + strlen(employee_list[i])); 
		}
		else
		{
			pre_size += ((digit + 2) + strlen(employee_list[i])); 
		}	

		char *date_str = NULL;
		if(!create_string_date(now_seconds(),&date_str))
		{
			printf("crate string date failed, %s:%d.\n",F,L-2);
			free_BST(&binary_st);
			close_file(1,fd_td);
			return 0;
		}		
		
		pre_size += strlen(date_str) + number_of_digit(service) + 1; /* 1 is for '\0'*/

		char data_to_add[pre_size];
		memset(data_to_add,0,pre_size);

		float final_cc = 0.0;
		if(random_n == i && cents > 1)
			final_cc = (((float)tip_cc/100) +((float)cents/100));
		else
			final_cc = (float)tip_cc/100;
	
		if(all_cash_tips)
			csh_amt = final_cc*100, final_cc = 0.0, tip_cc = 0;

		if(csh_amt > 0 && !all_cash_tips)
		{
			if(snprintf(data_to_add,pre_size,"%s%.2f%s%.2f%s%s%s%s%s%d",amt,
						final_cc,cash_tp,(float)csh_amt/100,
						dt,date_str,emply,employee_list[i],
						srvc,service) < 0)
			{
				printf("snprintf failed, %s:%d.\n",F,L-3);
				free(date_str);
				free_BST(&binary_st);
				close_file(1,fd_td);
				return 0;
			}
		}
		else if(all_cash_tips)
		{
			if(snprintf(data_to_add,pre_size,"%s%.2f%s%s%s%s%s%d",cash_tp,(double)csh_amt/100,
						dt,date_str,emply,employee_list[i],
						srvc,service) < 0)
			{
				printf("snprintf failed, %s:%d.\n",F,L-3);
				free(date_str);
				close_file(1,fd_td);
				free_BST(&binary_st);
				return 0;
			}

		}
		else
		{
			if(snprintf(data_to_add,pre_size,"%s%.2f%s%s%s%s%s%d",amt,
						final_cc,dt,date_str,emply,
						employee_list[i],srvc,service)< 0)
			{
				printf("snprintf failed, %s:%d.\n",F,L-3);
				free(date_str);
				close_file(1,fd_td);
				free_BST(&binary_st);
				return 0;
			}
		}


		if(!__write_safe(fd_td,data_to_add,"tips",NULL))
		{
			printf("__write_safe() failed %s:%d.\n",F,L-2);
			free(date_str);
			close_file(1,fd_td);
			free_BST(&binary_st);
			return 0;
		}
		free(date_str);
	}

	close_file(1,fd_td);
	free_BST(&binary_st);
	return 1;
}

unsigned char submit_cash_tips(float cash, char *employee_key, int service)
{
	int fd_ti = open_file("tips.inx",0);
	if(file_error_handler(1,fd_ti) > 0)
	{
		printf("open_file failed, %s:%d.\n",F,L-4);
		return 0;
	}
	
	/*Load index 1*/
	HashTable ht={0, NULL};
	if(!read_indexes("tips",fd_ti,1,&ht,NULL))
	{
		printf("read_indexes failed %s:%d.", F,L-2);
		return 0;
	}
	
	char **keys_a = keys(&ht);
	if(!keys_a)
	{
		printf("no tip to submit to.\n");
		close_file(1,fd_ti);
		destroy_hasht(&ht);
		return 0;	
	}

	size_t k_n = len(ht);	 

	destroy_hasht(&ht);

	int j = 0, fnd = 0;
	for(j = 0; j < k_n; j++)
	{
		if(strstr(keys_a[j],employee_key) != NULL)
		{
			char c = return_last_char(keys_a[j]);
			int num = c - '0';
			if( num == service)
			{
				fnd++;
				break;
			}
		}
	}
	
	if(fnd == 0)
	{
		printf("no entry found.\n");
		free_strs(k_n,1,keys_a);
		return 0;
	}


	char *key_tip = strdup(keys_a[j]);
	free_strs(k_n,1,keys_a);
	
	/* from line 699 to line 742 I perform a check to make sure we are writing the data */
	/* correctly to maintain data integrity, if the indexing tips fails, this make sure */
	/* the program does not write the wrong data to the wrong record in the file.       */
	char *date_str = NULL;
	if(!create_string_date(now_seconds(),&date_str))
	{
		printf("crate string date failed, %s:%d.\n",F,L-2);
		close_file(1,fd_ti);
		free(key_tip);
		return 0;
	}
	
	/*load index 0*/
	if(!read_indexes("tips",fd_ti,0,&ht,NULL))
	{
		printf("read_indexes failed %s:%d.", F,L-2);
		close_file(1,fd_ti);
		free(date_str);
		free(key_tip);
		return 0;	
	}

	char *date = NULL;
	if(!extract_date(key_tip, &date, &ht))
	{
		printf("extract date failed, %s:%d.\n",F,L-2);
		close_file(1,fd_ti);
		destroy_hasht(&ht);
		free(date_str);
		free(key_tip);
		return 0;	
	}
	
	destroy_hasht(&ht);

	if(strcmp(date,date_str) != 0)
	{
		printf("date are not equal.\n");
		close_file(1,fd_ti);
		free(key_tip);
		free(date_str);
		free(date);
		return 0;	
	}

	free(date_str);
	free(date);

	/* get the tip record*/
	int fd_td = open_file("tips.dat",0);
	if(file_error_handler(1,fd_td) > 0)
	{
		printf("open_file failed, %s:%d.\n",F,L-2);
		close_file(1,fd_ti);
		free(key_tip);
		return 0;	
	}	

	struct Record_f **recs = calloc(1,sizeof(struct Record_f*));
	if(!recs)
	{
		printf("calloc failed, %s:%d.\n",F,L-3);
		close_file(2,fd_ti,fd_td);
		free(key_tip);
		return 0;
	}

	int i = 0, *pi = &i;
	if(!get_rec(fd_td,fd_ti,pi,key_tip,&recs,"tips",LK_REQ))
	{
		printf("get_rec failed, %s:%d.\n",F,L-2);
		free_record_array(*pi,&recs);
		free(key_tip);
		close_file(2,fd_ti,fd_td);
		return 0;
	}

	if(*pi > 1)
	{
		printf("code refactor needed.%s:%d.",F,L);
		free_record_array(*pi,&recs);
		free(key_tip);
		close_file(2,fd_ti,fd_td);
		return 0;
	}

	if(cash > recs[0]->fields[0].data.f)
	{
		printf("cash bigger than credit card.\n");
		free_record_array(*pi,&recs);
		free(key_tip);
		close_file(2,fd_ti,fd_td);
		return 0;
	}
	
	char* cash_field = ":cash:t_f:";
	char* credit_field = "credit_card:t_f:";
	size_t digit = 0;
	float credit_c = recs[0]->fields[0].data.f - cash;
	float temp = 0;
	
	free_record_array(*pi,&recs);
	/*count how many char in the string the floating point numbers will need*/
	if((digit = digits_with_decimal(cash)) == -1 || (temp = digits_with_decimal(credit_c)) == -1)
	{
		printf("can't count float digits %s:%d.\n",F,L-2);
		close_file(2,fd_ti,fd_td);
		free(key_tip);
		return 0;
	}
	
	digit += (temp + 4); /*+ 4 is to account 2 position after the '.' for two float */
	size_t size = strlen(cash_field) + strlen(credit_field) + digit + 1;/*+ 1 for '\0'*/
	
	/* create the string data (data_to_add)*/
	char* data_to_add = calloc(size, sizeof(char));
	if(!data_to_add)
	{
		printf("calloc failed, %s:%d.\n", F,L-2);
		free(key_tip);
		close_file(2,fd_ti,fd_td);
		return 0;
	}

	if(snprintf(data_to_add,size,"%s%.2f%s%.2f",credit_field,credit_c,cash_field,cash) < 0)
	{
		printf("snprintf failed, %s:%d.",F,L-2);
		free(data_to_add);
		free(key_tip);
		close_file(2,fd_ti,fd_td);
		return 0;
	}
	
	/*use __update() to update the record in the tips file*/
	if(!__update("tips",fd_ti,fd_td, data_to_add,key_tip))
	{
		printf("update record failed. %s:%d.\n", F,L-2);
		close_file(2,fd_ti,fd_td);
		free(key_tip);
		free(data_to_add);
		return 0;
	}

	free(key_tip);
	close_file(2,fd_ti,fd_td);
	free(data_to_add);
	return 1;
}

unsigned char submit_tips(float c_card, float cash, int service)
{
	char** employee_list = NULL;
	int size = 0, *psz = &size;
	unsigned char test = list_clocked_in_employee(&employee_list,psz);
	if( test == NO_EMPLOYEE_CLOCKED_IN || !test) 
	{
		printf("list_clocked employee failed.\n");
		free_strs(size,1,employee_list);
		return 0;
	}
	
	if(!compute_tips(c_card, cash,service,employee_list,*psz))
	{	
		printf("compute tips failed.\n");
		free_strs(size,1,employee_list);
		return 0;
	}

	free_strs(size,1,employee_list);
	return 1;
}
