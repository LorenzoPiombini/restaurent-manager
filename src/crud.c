#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "str_op.h"
#include "hash_tbl.h"
#include "record.h"
#include "debug.h"
#include "crud.h"
#include "key_op.h"
#include "file.h"
#include "parse.h"
#include "common.h"
#include "index.h"
#include "lock.h"
#include "date.h"
#include "restaurant_t.h"

unsigned char read_indexes(char* file_name,int fd_index,int index_nr,HashTable* ht, int* ht_array_size)
{
	char** files = two_file_path(file_name);
	if(!files)
	{
		fprintf(stderr,"%s() failed, %s:%d.\n",__func__,F,L-3);
		return 0;
	}
	
	int lock_pos_i = 0, *plp_i = &lock_pos_i;
	int lock_pos_arr_i = 0, *plpa_i = &lock_pos_arr_i;
	/*acquire RD_IND locks*/
	if(shared_locks)
	{
		int result_i = 0;
		do {	
			off_t fd_i_s = get_file_size(fd_index,NULL);
 
			if(((result_i = acquire_lock_smo(&shared_locks,plp_i,plpa_i,files[0],0,
					fd_i_s,RD_IND,fd_index)) == 0)) {
				__er_acquire_lock_smo(F,L-5);
				free_strs(2,1,files);
				return 0;
			}
		}while(	result_i == MAX_WTLK || result_i == WTLK);
	}

	free_strs(2,1,files);
	if(index_nr == -1)
	{
		if(!ht_array_size){
			fprintf(stderr,"pointer to int (array ht size) can't be NULL.\n");
			goto exit_error;
		}

		/*read all indexes*/
		if(!read_all_index_file(fd_index,&ht,ht_array_size)) {
			fprintf(stderr,"read_all_index() failed. %s:%d.\n",F,L-2);
			goto exit_error;
		}
		
	} else {
		/*read index_nr*/
		if(!read_index_nr(index_nr,fd_index,&ht)){
			fprintf(stderr,"read_index_nr() failed. %s:%d.\n",F,L-2);
			goto exit_error;
		}
	}
	
	/*release the lock normally*/
	if(shared_locks) {
		int result_i = 0;
		do {
			if((result_i = release_lock_smo(&shared_locks,
				plp_i,plpa_i)) == 0 ) {
				__er_release_lock_smo(F,L-3);
				return 0;
			}
		}while(result_i == WTLK);
						
		return 1;
	}
	
	return 1;

exit_error:
	if(shared_locks) {
		/*release the locks before exit*/
		int result_i = 0;
		do{
			if((result_i = release_lock_smo(&shared_locks,
				plp_i,plpa_i)) == 0 ) {
					__er_release_lock_smo(F,L-3);
					return 0;
			}
		}while(result_i == WTLK);
						
		return 0;
	}
	return 0;
}

unsigned char is_a_db_file(int fd_data, char* file_name,struct Header_d *hd_caller)
{
	struct Schema sch = {0,NULL,NULL};
	struct Header_d hd = {0,0,sch};

	/* this variables are needed for the lock system */
	int lock_pos = 0, *plp = &lock_pos;
	int lock_pos_arr = 0, *plpa = &lock_pos_arr;

	char** files = two_file_path(file_name);
	if(!files) {
		fprintf(stderr,"%s() failed, %s:%d.\n",__func__,F,L-3);
		return 0;
	}

	/* acquire RD_REC lock */
	/* shared_locks is declare in lock.h and lock.c making the variable global*/
	if(shared_locks) {
		int result_d = 0;
		do {
			off_t fd_d_s = get_file_size(fd_data,NULL);
 
			if((result_d = acquire_lock_smo(&shared_locks,plp,plpa,files[1],0,
					fd_d_s,RD_REC,fd_data)) == 0) {
				__er_acquire_lock_smo(F,L-5);
				free_strs(2,1,files);
				return 0;
			}
		}while(result_d == MAX_WTLK || result_d == WTLK);
	}

	free_strs(2,1,files);

	/*make sure the file pointer is at the start*/
	if(begin_in_file(fd_data) == -1) {
		__er_file_pointer(F,L-2);
		goto exit_error;
	}

	/* this is to ensure the file is a db file */
	if(!read_header(fd_data,&hd)) {
		printf("read header failed. %s:%d.\n",F,L-2);
		goto exit_error;
	}

	if(hd_caller) {	
		(*hd_caller).id_n = hd.id_n;
		(*hd_caller).version = hd.version;
		(*hd_caller).sch_d.fields_num = hd.sch_d.fields_num;	
		(*hd_caller).sch_d.fields_name = hd.sch_d.fields_name;	
		(*hd_caller).sch_d.types = hd.sch_d.types;
	}else {
		free_schema(&hd.sch_d);
	}
	
	/* if the file is a db file reset the file pointer at the beginning */
	if(begin_in_file(fd_data) == -1)
	{
		__er_file_pointer(F,L-2);
		goto exit_error;
	}
	
	/*release the lock normally*/
	
	if(shared_locks)
	{
		int result_d = 0;
		do
		{
			if((result_d = release_lock_smo(&shared_locks,
					plp,plpa)) == 0 )
			{
				__er_release_lock_smo(F,L-3);
				return 0;
			}
		}while(result_d == WTLK);
					
		return 1;
	}

	return 1;

exit_error:
	if(sch.fields_name && sch.types)
		free_schema(&sch);

	if(shared_locks){
		/*release the locks before exit*/
		int result_d = 0;
		do{
			if((result_d = release_lock_smo(&shared_locks,
				plp,plpa)) == 0 ) {
				__er_release_lock_smo(F,L-3);
				return 0;
			}
		}while(result_d == WTLK);
						
		return 0;
	}
	return 0;
}

unsigned char get_rec(int fd_dat,int fd_inx, int* index, void* key, int key_type, struct Record_f ***recs, char* file_name, int lock)
{
	if(lock == LK_REQ)
	{
		if(!is_a_db_file(fd_dat,file_name,NULL))
		{
			printf("not a db file, or reading header error, %s:%d.\n",F,L-2);
			return 0;
		}
	}

	/* this variables are needed for the lock system */
	int lock_pos = 0, *plp = &lock_pos;
	int lock_pos_arr = 0, *plpa = &lock_pos_arr;
	int lock_pos_i = 0, *plp_i = &lock_pos_i;
	int lock_pos_arr_i = 0, *plpa_i = &lock_pos_arr_i;
	

	/* acquire RD_IND and RD_REC locks */
	/* shared_locks is declare in lock.h and lock.c making the variable global*/
	if(shared_locks && lock == LK_REQ)
	{
		char** files = two_file_path(file_name);
		if(!files)
		{
			printf(stderr,"two_file_path() failed, %s:%d.\n",F,L-3);
			return 0;
		}

		int result_i = 0, result_d = 0;
		do {
			off_t fd_i_s = get_file_size(fd_inx,NULL);
			off_t fd_d_s = get_file_size(fd_dat,NULL);
 
			if(((result_i = acquire_lock_smo(&shared_locks,plp_i,plpa_i,files[0],0,
					fd_i_s,RD_IND,fd_inx)) == 0) ||
			     ((result_d = acquire_lock_smo(&shared_locks,plp,plpa,files[1],0,
					fd_d_s,RD_REC,fd_dat)) == 0))
			{
				__er_acquire_lock_smo(F,L-5);
				free_strs(2,1,files);
				return 1;
			}
		}while(result_d == MAX_WTLK || result_d == WTLK 
			|| result_i == MAX_WTLK || result_i == WTLK);

		free_strs(2,1,files);
	}


	/*we can read the file*/	
	HashTable ht = {0,NULL}, *pht = &ht;
	off_t pos = 0;
	int rec_arr_s = 1;

	if(!read_index_nr(*index,fd_inx,&pht)) {
		printf("read index %d failed. %s:%d.\n", *index,F,L-2);
		goto exit_error;
	}
		
	if((pos = get(key,pht,key_type)) == -1) {
		printf("record not found.\n");
		destroy_hasht(pht);
		goto exit_error;
	}	
	
	destroy_hasht(pht);

	if(find_record_position(fd_dat,pos) == -1) {
		__er_file_pointer(F,L-2);
		goto exit_error;
	}

	if(((*recs)[rec_arr_s - 1] = read_file(fd_dat, file_name)) == NULL) {
		printf("read record failed. %s:%d.\n", F,L-2);
		free(*recs);
		goto exit_error;
	}

	if((pos = get_update_offset(fd_dat)) == -1) {
		printf("read update offset failed. %s:%d.\n", F,L-2);
		free_record_array(rec_arr_s,recs);
		goto exit_error;
	}	

	if(pos == 0) {
		*index = rec_arr_s;
		/*release locks*/
		if(shared_locks && lock == LK_REQ) {
			int result_i = 0, result_d = 0;
			do {
				if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
				   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0) {
					__er_release_lock_smo(F,L-5);
					return 0;
				}

			}while(result_d == WTLK || result_i == WTLK);

			return 1;	
		}

		return 1;
	}else {
		do {
			/*(++rec_arr_s) increments the rec_arr_s counter (the size of the array) */
			struct Record_f **rec_n = realloc(*recs,(++rec_arr_s) * sizeof(struct Record_f*));
			if(!rec_n) {
				printf("realloc failed. %s:%d.\n",F,L-3);
				free_record_array(rec_arr_s,recs);
				goto exit_error;
			}

			*recs = rec_n;
			(*recs)[rec_arr_s-1] = NULL;/*assigning the new array element to NULL*/

			if(find_record_position(fd_dat,pos) == -1) {
				__er_file_pointer(F,L-2);
				free_record_array(rec_arr_s,recs);
				goto exit_error;
			}
		
		
			if(((*recs)[rec_arr_s-1] = read_file(fd_dat, file_name)) == NULL) {
				printf("read record failed. %s:%d.\n", F,L-2);
				free_record_array(rec_arr_s,recs);
				goto exit_error;
			}

			if((pos = get_update_offset(fd_dat)) == -1) {
				printf("read update offset failed. %s:%d.\n", F,L-2);
				free_record_array(rec_arr_s,recs);
				goto exit_error;
			}

		}while(pos > 0);
	}

	*index = rec_arr_s;
	/*release locks*/
	if(shared_locks && lock == LK_REQ)
	{
		int result_i = 0, result_d = 0;
		do
		{
			if((result_i = release_lock_smo(&shared_locks,
					plp_i,plpa_i)) == 0 ||
			   (result_d = release_lock_smo(&shared_locks,
					plp,plpa)) == 0)
			{
				__er_release_lock_smo(F,L-5);
				return 0;
			}

		}while(result_d == WTLK || result_i == WTLK);

		return 1;	
	}

	return 1;

exit_error:
	if(shared_locks && lock == LK_REQ) {
		int result_i = 0, result_d = 0;
		do {
			if((result_i = release_lock_smo(&shared_locks,
					plp_i,plpa_i)) == 0 ||
			   (result_d = release_lock_smo(&shared_locks,
					plp,plpa)) == 0) {
				__er_release_lock_smo(F,L-5);
				return 0;
			}

		}while(result_d == WTLK || result_i == WTLK);
		return 0;	
	}
	return 0;
}

unsigned char schema_control(int fd_data, unsigned char* check_s, char* file_name,char* data_to_add,
				 struct Record_f **rec_c, struct Schema **sch_c, unsigned char update,
				 int *pcount, int crud)
{

	int lock_rel = 0;
	struct Schema sch = {0,NULL,NULL};
	struct Header_d hd = {0,0,sch};

	if(crud == LK_REQ)
	{
		if(!is_a_db_file(fd_data,file_name,&hd)) {
			printf("not a db file, or reading header error, %s:%d.\n",F,L-2);
			return 0;
		}
	}else
	{
		/*make sure the file pointer is at the start*/
		if(begin_in_file(fd_data) == -1) {
			__er_file_pointer(F,L-2);
			return 0;
		}


		/* this is to ensure the file is a db file */
		if(!read_header(fd_data,&hd)) {
			printf("read header failed. %s:%d.\n",F,L-2);
	                return 0;
		}
	
		/* if the file is a db file reset the file pointer at the beginning */
		if(begin_in_file(fd_data) == -1) {
			__er_file_pointer(F,L-2);
			free_schema(&hd.sch_d);
			return 0;	
		}


	}


	int fields_count = count_fields(data_to_add, TYPE_) + count_fields(data_to_add,T_);
	if(fields_count > MAX_FIELD_NR) {
       		printf("Too many fields, max %d each file definition.",MAX_FIELD_NR);
		free_schema(&hd.sch_d);
	   	return 0;
        }

	char* buffer = strdup(data_to_add);
        char* buf_t = strdup(data_to_add);
        char* buf_v = strdup(data_to_add);
	
	struct Record_f *rec = NULL;
	unsigned char check = perform_checks_on_schema(buffer, buf_t,buf_v,fields_count,
							   fd_data,file_name,&rec, &hd);

	if(check == SCHEMA_ERR || check == 0) {
        	free_schema(&hd.sch_d);
        	free(buffer), free(buf_t), free(buf_v);
		return 0;
	}
			
	if(!rec) {
          	printf("error creating record. %s:%d\n",F,L-1);
		free(buffer), free(buf_t), free(buf_v);
	        free_schema(&hd.sch_d);
                return 0;
         }

	*rec_c = rec;

	/*if the schema is new we update the header*/
	if(check == SCHEMA_NW) {
		// check the header size
		if(compute_size_header((void*)&hd) >= MAX_HD_SIZE) {
			printf("File definition is bigger than the limit.\n");
			free(buffer);
			free(buf_t);
			free(buf_v);
               		free_schema(&hd.sch_d);
			free_record(rec,rec->fields_num);
                	return 0;
		}
		
		if(crud == LK_REQ) {	
			/* this variables are needed for the lock system */
			int lock_pos = 0, *plp = &lock_pos;
			int lock_pos_arr = 0, *plpa = &lock_pos_arr;

			char** files = two_file_path(file_name);
			if(!files) {
				printf("%s() failed, %s:%d.\n",__func__,F,L-3);
				goto exit_error;
			}

			/*acquire WR_REC lock*/
			if(shared_locks) {
				int result_d = 0;
				do {	
 
					if(((result_d = acquire_lock_smo(&shared_locks,plp,plpa,files[1],0,
						MAX_HD_SIZE,WR_REC,fd_data)) == 0)) {
						__er_acquire_lock_smo(F,L-5);
						free_strs(2,1,files);
						goto exit_error;
						return 0;
					}
				}while(result_d == MAX_WTLK || result_d == WTLK);
			}

			lock_rel = 1;/*this variable is for the exit on error cleanup*/
			free_strs(2,1,files);
			files = NULL;
			/*set the file pointer at the start*/
			if(begin_in_file(fd_data) == -1){
				__er_file_pointer(F,L-1);
				goto exit_error;				
			}

			if(!write_header(fd_data,&hd)) {
				printf("write to file failed, %s:%d.\n",F,L-1);
				goto exit_error;				
			}
			
			/*release the lock normally*/
			if(shared_locks) {
				int result_d = 0;
				do {
					if((result_d = release_lock_smo(&shared_locks,
							plp,plpa)) == 0 ) {
						__er_release_lock_smo(F,L-3);
						return 0;
					}
				}while(result_d == WTLK);
			}
			
			lock_rel  = 0;
		}else {
			/*set the file pointer at the start*/
			if(begin_in_file(fd_data) == -1) {
				__er_file_pointer(F,L-1);
				goto error_exit;
			}

			if(!write_header(fd_data,&hd)) {
				printf("write to file failed, %s:%d.\n",F,L-1);
				goto error_exit;
			}

		}
	}

	/*extracting values that we need at the caller level*/
	if(update)
	{
		*pcount = fields_count;			
		*check_s = check;
		(*sch_c)->fields_num =  hd.sch_d.fields_num;
		(*sch_c)->fields_name = calloc((*sch_c)->fields_num, sizeof(char*));

		if(!(*sch_c)->fields_name)
		{
			printf("calloc failed. %s:%d.\n", F,L-3);
			goto error_exit;
		}
	
		(*sch_c)->types = calloc((*sch_c)->fields_num, sizeof(enum ValueType));
		if(!(*sch_c)->types)
		{
			printf("calloc failed. %s:%d.\n", F,L-3);
			free((*sch_c)->fields_name);
			goto error_exit;
		}

		int i = 0;
		for(i = 0; i < (*sch_c)->fields_num; i++)
		{
			(*sch_c)->fields_name[i] = strdup(hd.sch_d.fields_name[i]);
			(*sch_c)->types[i] = hd.sch_d.types[i];
		}
	}
	
        free_schema(&hd.sch_d);
	free(buffer), free(buf_t), free(buf_v);
	return 1;

error_exit:
        free_schema(&hd.sch_d);
	free_record(rec,rec->fields_num);
	free(buffer);
	free(buf_t);
	free(buf_v);
	if(shared_locks && lock_rel){
		/*release the locks before exit*/
		int result_d = 0;
		do
		{
			if((result_d = release_lock_smo(&shared_locks,
					plp,plpa)) == 0 )
			{
				__er_release_lock_smo(F,L-3);
				return 0;
			}
		}while(result_d == WTLK);
				
		return 0;
	}
        return 0;		
}

/*read all indexes before using this fucntion*/
unsigned char write_rec(int fd_data, int fd_index, HashTable* ht,struct Record_f *rec, void* key, int key_type)
{
	/* set the file pointer at the end*/	
	off_t eof = go_to_EOF(fd_data);
	if(eof == -1) {
		__er_file_pointer(F,L-1);
		return 0;
	}

	/*save the key with the off_t in the hash_table (index file)*/
	if(!set(key,key_type,eof,&ht[0])) {
		printf("set failed. %s:%d.\n",F,L-2);
		free(key);
        	return 0;
        }

	if(!write_file(fd_data,rec,0,0)) {
		printf("write to file failed, %s:%d.\n",F,L-2);
		return 0;	
	}
	
	return 1;  
}

/*read all indexes before using this fucntion*/
unsigned char delete_rec(int fd_inx, void* key,int key_type, HashTable *ht, int* p_i)
{

	/*this will delete all the occurences of the key among all indexes*/
	int i = 0;
	for(i = 0; i < *p_i; i++)
	{
		Node* del_r = delete(key, key_type, &ht[i]);
		if(del_r) {
			switch(key_type) {
			case STR:
				free(del_r->key.s);
				break;
			default:
				break;
			}
			free(del_r);
		}
	}
	
	return 1;
}

/*you have to read the index 0 before using this fucntion*/
unsigned char update_rec(int fd_data, HashTable* ht, struct Schema* sch, char* file_path,
			 struct Record_f *rec, char* buffer, unsigned char check, unsigned char update,
			 int fields_count, void* key, int key_type)
{		
	/*look for the key in the ht */
	off_t offset = get(key,ht,key_type);
	if(offset == - 1) {
		printf("record not found.\n");
		return 0;
	}
		
	destroy_hasht(ht);
	ht = NULL;

	if(find_record_position(fd_data, offset) == -1) {
		__er_file_pointer(F,L-1);
		free_schema(sch);
               	return 0;
	}

	struct Record_f *rec_old = read_file(fd_data, file_path);
	if (!rec_old) {
		printf("reading record failed %s:%d.\n",F,L-2);
		free_schema(sch);
		return 0;
	}
			
	off_t updated_rec_pos = get_update_offset(fd_data);
	if(updated_rec_pos == -1 ) {
		__er_file_pointer(F,L-1);
		free_schema(sch);
		return 0;
	}
			
	struct Record_f **recs_old = NULL;
	off_t* pos_u = NULL;
	if(updated_rec_pos > 0) {
		int error = 0;
		int index = 2;
		int pos_i = 2;
		recs_old = calloc(index, sizeof(struct Record_f*));
		
		if(!recs_old) {
			printf("calloc failed. %s%d.\n",F,L-3);
			free_record(rec_old,rec_old->fields_num);
			free_schema(sch);
			return 0;
		}
				
		pos_u = calloc(pos_i, sizeof(off_t));
		if(!pos_u) {
			printf("calloc failed, %s:%d.\n",F,L-2);
			free_record(rec_old,rec_old->fields_num);
			free_schema(sch);
			free_record_array(index,&recs_old);
			return 0;
		}	

		pos_u[0] = offset;/*first record position*/
		pos_u[1] = updated_rec_pos; /*first updated record position*/
				 
		if(find_record_position(fd_data, updated_rec_pos) == -1){
			__er_file_pointer(F,L-1);
			free(pos_u);
			free_schema(sch);
			free_record_array(index,&recs_old);
			return 0;
		}
					
		struct Record_f *rec_old_s =  read_file(fd_data, file_path);
		if(!rec_old_s)
		{
			printf("error reading file, %s:%d.\n",F,L-2);
			free(pos_u);
			free_schema(sch);
			free_record_array(index,&recs_old);
			return 0;
		}
				
		recs_old[0] = rec_old;
		recs_old[1] = rec_old_s;

		/*read all the record's parts if any */	
		while((updated_rec_pos = get_update_offset(fd_data)) > 0)
		{
			index++, pos_i++;
			struct Record_f **recs_old_n = realloc(recs_old, index * sizeof(struct Record_f*));
			if(!recs_old_n) {
				printf("realloc failed, %s:%d.\n",F,L-3);
				free_schema(sch);
				free(pos_u);
				free_record_array(index,&recs_old);
				return 0;
			}

			recs_old = recs_old_n;
					
			off_t* pos_u_n = realloc(pos_u, pos_i * sizeof(off_t));
			if(!pos_u_n) {	
				printf("realloc failed, %s:%d.\n",F,L-3);
				free_schema(sch);
				free(pos_u);
				free_record_array(index,&recs_old);
				return 0;
			}

			pos_u = pos_u_n;
			pos_u[pos_i - 1 ] = updated_rec_pos;

			struct Record_f *rec_old_new = read_file(fd_data, file_path);							
			if(!rec_old_new) {
				printf("error reading file, %s:%d.\n",F,L-1);
				free_schema(sch);
				free(pos_u);
				free_record_array(index,&recs_old);
				return 0;
			}

			recs_old[index - 1] = rec_old_new; 
		}/* end of while loop*/
	
		/* check which record we have to update*/

		char* positions = calloc(index,sizeof(char));
		if(!positions) {
			printf("calloc failed, main.c l %d.\n", __LINE__ - 1);
			free_schema(sch);
			free(pos_u);
			free_record_array(index,&recs_old);
			return 0;
		}
		/* this function check all records from the file 
		   against the new record setting the values that we have to update
		   and populates the char array positions. If an element contain 'y'
		   you have to update the record at that index position of 'y'. */
		 		
		find_fields_to_update(recs_old, positions,rec, index);
				
				
		if(positions[0] != 'n' && positions[0] != 'y'){
			printf("check on fields failed, %s:%d.\n",F,L-1);
			error = 1;
			goto exit;
		}
				
		/* write the update records to file */
		int i = 0, j = 0;
		unsigned short updates = 0; /* bool value if 0 no updates*/
		for(i = 0; i < index; i++)
		{
			if(positions[i] == 'n')
				continue;	
					
			++updates;
			if(find_record_position(fd_data, pos_u[i]) == -1) {
				__er_file_pointer(F,L-1);
				error = 1;
				goto exit;
			}
					
			off_t right_update_pos = 0;
			if( (index - i) > 1 )
				 right_update_pos = pos_u[i+1];
				
			if( index - i == 1  && check == SCHEMA_NW)
			{
				right_update_pos = go_to_EOF(fd_data);
				if(find_record_position(fd_data, pos_u[i]) == -1 || 
							right_update_pos == -1) {
					__er_file_pointer(F,L-3);
					error = 1;
					goto exit;
				}
						
			}

			if(!write_file(fd_data, recs_old[i], right_update_pos,update)) {
				printf("error write file, %s:%d.\n",F,L-1);
				error = 1;
				goto exit;
			}					
		}

		if(check == SCHEMA_NW && updates > 0){
			struct Record_f *new_rec = NULL;
			if(!create_new_fields_from_schema(recs_old,rec, sch,index,&new_rec,file_path)) {
				printf("create new fileds failed, %s:%d.\n",F,L-2);
				error = 1;
				goto exit;
			}
					
			off_t eof = go_to_EOF(fd_data);/* file pointer to the end*/
			if(eof == -1) {
				__er_file_pointer(F,L-1);
				free_record(new_rec,new_rec->fields_num);
				error = 1;
				goto exit;
			}

			/*writing the new part of the schema to the file*/
			if(!write_file(fd_data,new_rec,0,0)) {
				printf("write to file failed, %s:%d.\n",F,L - 1);
				free_record(new_rec,new_rec->fields_num);
				error = 1;
				goto exit;
			}
					
			free_record(new_rec,new_rec->fields_num);
			/*the position of new_rec in the old part of the record
				 was already set at line 846*/

		}

		if(check == SCHEMA_NW && updates == 0 ){
			/* store the EOF value*/
		        off_t eof = 0;
			if((eof = go_to_EOF(fd_data)) == -1) {
				__er_file_pointer(F,L-1);
				error = 1;
				goto exit;
			}

			/*find the position of the last piece of the record*/
			off_t initial_pos = 0;
		
			if((initial_pos = find_record_position(fd_data,pos_u[index - 1])) == -1) {
				__er_file_pointer(F,L-1);
				error = 1;
				goto exit;
			}
		
					
			/*re-write the record*/
			if(write_file(fd_data,recs_old[index - 1], eof, update) == - 1){
				printf("write file failed, %s:%d.\n",F,L-1);
				error = 1;
				goto exit;
			}

 		     	/*move to EOF*/	
			if((go_to_EOF(fd_data)) == -1) {
				__er_file_pointer(F,L-1);
				error = 1;
				goto exit;
			}

			/*create the new record*/
			struct Record_f *new_rec = NULL;
			if(!create_new_fields_from_schema(recs_old,rec, sch,index,&new_rec,file_path)) {
				printf("create new fields failed, %s:%d.\n",F,L-2);
				error = 1;
				goto exit;
			}

			/*write the actual new data*/
			if(!write_file(fd_data,new_rec,0,0)) {
				printf("write to file failed, %s:%d.\n",F,L-1);
				free_record(new_rec,new_rec->fields_num);
				error = 1;
				goto exit;
			}

			free_record(new_rec,new_rec->fields_num);
		}
			
			exit:	
			free_schema(sch);
			free(pos_u);
			free(positions);
			free_record_array(index,&recs_old);
			if(error){
				return 0;
			}else{
				return 1;//success
			}
	}

			free_schema(sch);
			/*updated_rec_pos is 0, THE RECORD IS ALL IN ONE PLACE */
			struct Record_f *new_rec = NULL;
			unsigned char comp_rr = compare_old_rec_update_rec(&rec_old, rec, &new_rec
									,file_path,check,buffer,fields_count);
			if (comp_rr == 0){
				printf("compare records failed, %s:%d.\n",F,L-4);
				free_record(rec_old,rec_old->fields_num);
				if(new_rec){
					free_record(new_rec, new_rec->fields_num);
				}
				return 0;
			}

			if(updated_rec_pos == 0 && comp_rr == UPDATE_OLD){
				//set the position back to the record
				if(find_record_position(fd_data, offset) == -1){
					__er_file_pointer(F,L-1);
					free_record(rec_old,rec_old->fields_num);
                        		return 0;
				}

				//write the updated record to the file
				if(!write_file(fd_data,rec_old,0,update)){
					printf("write to file failed, main.c l %d.\n", __LINE__ - 1);
					free_record(rec_old,rec_old->fields_num);
					return 0;
				}

				free_record(rec_old,rec_old->fields_num);
				return 1;					
			}
			
			/*updating the record but we need to write some data in another place in the file*/
			if(updated_rec_pos == 0 && comp_rr == UPDATE_OLDN){
				
				off_t eof = 0;
				if((eof = go_to_EOF(fd_data)) == -1)
				{
					__er_file_pointer(F,L-1);
					free_record(rec_old,rec_old->fields_num);
					free_record(new_rec, new_rec->fields_num);
					return 0;	
				}

				// put the position back to the record
				if(find_record_position(fd_data, offset) == -1){
					__er_file_pointer(F,L-1);
					free_record(rec_old,rec_old->fields_num);
					free_record(new_rec, new_rec->fields_num);
					return 0;	
				}

				
				//update the old record :
				if(!write_file(fd_data,rec_old,eof,update)){
					printf("can't write record, %s:%d.\n",__FILE__, __LINE__ - 1);
					free_record(rec_old,rec_old->fields_num);
					free_record(new_rec, new_rec->fields_num);
					return 0;
				}
				

				
				eof = go_to_EOF(fd_data);
				if(eof == -1){
					__er_file_pointer(F,L-1);
					free_record(rec_old,rec_old->fields_num);
					free_record(new_rec, new_rec->fields_num);
					return 0;
				}
 				/*passing update as 0 becuase is a "new_rec", (right most paramaters) */
				if(!write_file(fd_data, new_rec,0,0)){
					printf("can't write record, main.c l %d.\n", __LINE__ - 1);
					free_record(rec_old,rec_old->fields_num);
					free_record(new_rec, new_rec->fields_num);
					return 0;	
				}

					free_record(rec_old,rec_old->fields_num);
					free_record(new_rec, new_rec->fields_num);
					return 1;
			}
			
			return 1;
}	



unsigned char write_all_indexes(HashTable* ht,int fd, int index)
{
	if(!write_index_file_head(fd,index))
	{
		printf("write to file failed, %s:%d",F,L-2);
		return 0;
	}
	
	int i = 0;
	for(i = 0; i < index; i++)
	{
		if(write_index_body(fd,i,&ht[i]) == -1)
		{
			printf("write to file failed. %s:%d.\n",F,L-2);
			return 0;
		}
				
	}
	return 1;	
}

/*i decied to make bigger functions to make writing this software easier*/
/* even thought i do not like hiding functionality */
unsigned char __write(char* file_name, int* fd_index, int fd_data, char* data_to_add,
			struct Record_f *rec, char* key, indexing ixgn, int lock)
{
	if((data_to_add && rec) || (!data_to_add && !rec))
	{
		printf("specify either rec or data_to_add.\n");
		return 0;
	}

	/* this variables are needed for the lock system */
	int lock_pos = 0, *plp = &lock_pos;
	int lock_pos_arr = 0, *plpa = &lock_pos_arr;
	int lock_pos_i = 0, *plp_i = &lock_pos_i;
	int lock_pos_arr_i = 0, *plpa_i = &lock_pos_arr_i;

	char** files = two_file_path(file_name);
	if(!files)
	{
		printf("%s() failed, %s:%d.\n",__func__,F,L-3);
		return 0;
	}

	/*acquire WR_REC and WR_IND locks*/
	if(shared_locks && lock == LK_REQ)
	{
		int result_i = 0, result_d = 0;
		do
		{	
			off_t fd_i_s = get_file_size(*fd_index,NULL);
			off_t fd_d_s = get_file_size(fd_data,NULL);
 
			if(((result_i = acquire_lock_smo(&shared_locks,plp_i,plpa_i,files[0],0,
					fd_i_s,WR_IND,*fd_index)) == 0) ||
			     ((result_d = acquire_lock_smo(&shared_locks,plp,plpa,files[1],0,
					fd_d_s,WR_REC,fd_data)) == 0))
			{
				__er_acquire_lock_smo(F,L-5);
				free_strs(2,1,files);
				return 0;
			}
		}while(result_d == MAX_WTLK || result_d == WTLK 
			|| result_i == MAX_WTLK || result_i == WTLK);
	}

	free_strs(2,1,files);

	if(data_to_add)/*if the caller passed data_to_add we check the schema*/
	{
		unsigned char update = 0;
		if(!schema_control(fd_data, NULL, file_name,data_to_add,&rec, NULL, update,NULL,NO_OP))
		{
			printf("illegal schema. %s:%d.\n",F,L-2);
			if(shared_locks && lock == LK_REQ)
			{
				int result_i = 0, result_d = 0;
				do
				{
					if((result_i = release_lock_smo(&shared_locks,
							plp_i,plpa_i)) == 0 ||
					   (result_d = release_lock_smo(&shared_locks,
							plp,plpa)) == 0)
					{
						__er_release_lock_smo(F,L-5);
						return 0;
					}

				}while(result_d == WTLK || result_i == WTLK);
	
				return 0;	
			}

			return 0;
		}
	}

	HashTable *ht = NULL;	
	int ht_i = 0, *pht_i = &ht_i;	
	if(!read_all_index_file(*fd_index,&ht,pht_i))
	{
		printf("read index failed. %s:%d.\n",F,L-2);
		if(data_to_add)
			free_record(rec,rec->fields_num);
		if(shared_locks && lock == LK_REQ)
		{
			int result_i = 0, result_d = 0;
			do
			{
				if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
				   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0)
				{
					__er_release_lock_smo(F,L-5);
					return 0;
					}

			}while(result_d == WTLK || result_i == WTLK);

			return 0;	
		}

		return 0;
	}
	

	if(!write_rec(fd_data,*fd_index,ht,rec,key))
	{
		printf("write record failed. %s:%d.\n",F,L-2);
		free_ht_array(ht,*pht_i);
		if(data_to_add)
			free_record(rec,rec->fields_num);
		if(shared_locks && lock == LK_REQ)
		{
			int result_i = 0, result_d = 0;
			do
			{
				if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
				   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0)
				{
					__er_release_lock_smo(F,L-5);
					return 0;
				}

			}while(result_d == WTLK || result_i == WTLK);

			return 0;	
		}

		return 0;
	}	

	/*INDEXING __@@--USING FUNCTION POINTERS--@@__ */
	
	if(ixgn)/*this is a pointer to an indexing fucntion */
	{
		if(!ixgn(ht,key))
		{
			free_ht_array(ht,*pht_i);
			if(data_to_add)
				free_record(rec,rec->fields_num);
			if(shared_locks && lock == LK_REQ)
			{
				int result_i = 0, result_d = 0;
				do
				{
					if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
					   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0)
					{
						__er_release_lock_smo(F,L-5);
						return 0;
					}

				}while(result_d == WTLK || result_i == WTLK);

				return 0;	
			}
		
			return 0;
		}
	}		

	
	
	
	char* suffix = ".inx";
	size_t len = strlen(suffix) + strlen(file_name) + 1;
	char* file = calloc(len,sizeof(char));
	if(!file)
	{
		printf("calloc failed. %s:%d.\n",F,L-3);
		free_ht_array(ht,*pht_i);
		if(data_to_add)
			free_record(rec,rec->fields_num);
		if(shared_locks && lock == LK_REQ)
		{
			int result_i = 0, result_d = 0;
			do
			{
				if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
				   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0)
				{
					__er_release_lock_smo(F,L-5);
					return 0;
				}

			}while(result_d == WTLK || result_i == WTLK);

			return 0;	
		}

		return 0;	
	}

	if(snprintf(file,len,"%s%s", file_name, suffix) < 0)
	{
		printf("create file path failed.%s:%d.\n", F,L-2);
		free_ht_array(ht,*pht_i);
		if(data_to_add)
			free_record(rec,rec->fields_num);
		if(shared_locks && lock == LK_REQ)
		{
			int result_i = 0, result_d = 0;
			do
			{
				if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
				   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0)
				{
					__er_release_lock_smo(F,L-5);
					return 0;
				}

			}while(result_d == WTLK || result_i == WTLK);

			return 0;	
		}

		return 0;
	}

	close_file(1,*fd_index);
	*fd_index = open_file(file,1); //O_TRUNC to overwrite the content
	if(file_error_handler(1,*fd_index) > 0)
	{
		printf("can't open file in O_TRUNC mode %s:%d.\n",F,L-3);
		free(file);
		free_ht_array(ht,*pht_i);
		if(data_to_add)
			free_record(rec,rec->fields_num);
		if(shared_locks && lock == LK_REQ)
		{
			int result_i = 0, result_d = 0;
			do
			{
				if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
				   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0)
				{
					__er_release_lock_smo(F,L-5);
					return 0;
				}

			}while(result_d == WTLK || result_i == WTLK);

			return 0;	
		}

		return 0;
	}	
	
	if(!write_all_indexes(ht,*fd_index,*pht_i))
	{
		printf("can`t write indexes. %s:%d",F,L-2);
		free_ht_array(ht,*pht_i);
		free(file);
		if(data_to_add)
			free_record(rec,rec->fields_num);
		if(shared_locks && lock == LK_REQ)
		{
			int result_i = 0, result_d = 0;
			do
			{
				if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
				   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0)
				{
					__er_release_lock_smo(F,L-5);
					return 0;
				}

			}while(result_d == WTLK || result_i == WTLK);

			return 0;	
		}

		return 0;
	}
	
	close_file(1,*fd_index);
	*fd_index = open_file(file,0); // reopen in O_RDWR mode
	if(file_error_handler(1,*fd_index) > 0)
	{
		printf("can't open file in O_RDWR mode %s:%d.\n",F,L-3);
		free_ht_array(ht,*pht_i);
		if(data_to_add)
			free_record(rec,rec->fields_num);
		free(file);
		if(shared_locks && lock == LK_REQ)
		{
			int result_i = 0, result_d = 0;
			do
			{
				if((result_i = release_lock_smo(&shared_locks,
						plp_i,plpa_i)) == 0 ||
				   (result_d = release_lock_smo(&shared_locks,
						plp,plpa)) == 0)
				{
					__er_release_lock_smo(F,L-5);
					return 0;
				}

			}while(result_d == WTLK || result_i == WTLK);

			return 0;	
		}

		return 0;
	}	

	free(file);
	free_ht_array(ht,*pht_i);
	if(data_to_add)
		free_record(rec,rec->fields_num);
	
	if(shared_locks && lock == LK_REQ)
	{
		int result_i = 0, result_d = 0;
		do
		{
			if((result_i = release_lock_smo(&shared_locks,
					plp_i,plpa_i)) == 0 ||
			   (result_d = release_lock_smo(&shared_locks,
					plp,plpa)) == 0)
			{
				__er_release_lock_smo(F,L-5);
				return 0;
			}

		}while(result_d == WTLK || result_i == WTLK);

		return 1;	
	}

	return 1;
}

unsigned char __update(char *file_name, int fd_index, int fd_data, char *data_to_add,void *key, int key_type)
{	
	/* this variables are needed for the lock system */
	int lock_pos = 0, *plp = &lock_pos;
	int lock_pos_arr = 0, *plpa = &lock_pos_arr;
	int lock_pos_i = 0, *plp_i = &lock_pos_i;
	int lock_pos_arr_i = 0, *plpa_i = &lock_pos_arr_i;
	
	char** files = two_file_path(file_name);
	if(!files) {
		printf("%s() failed, %s:%d.\n",__func__,F,L-3);
		return 0;
	}

	/*acquire WR_REC and WR_IND locks*/
	if(shared_locks) {
		int result_i = 0, result_d = 0;
		do {
			off_t fd_i_s = get_file_size(fd_index,NULL);
			off_t fd_d_s = get_file_size(fd_data,NULL);

			if(((result_i = acquire_lock_smo(&shared_locks,plp_i,plpa_i,files[0],0,
					fd_i_s,WR_IND,fd_index)) == 0) ||
			     ((result_d = acquire_lock_smo(&shared_locks,plp,plpa,files[1],0,
					fd_d_s,WR_REC,fd_data)) == 0)) {
				__er_acquire_lock_smo(F,L-5);
				free_strs(2,1,files);
				return 0;
			}
		}while(result_d == MAX_WTLK || result_d == WTLK 
			|| result_i == MAX_WTLK || result_i == WTLK);
	}

	free_strs(2,1,files);
	files = NULL;
	struct Schema sch = {0,NULL,NULL}, *psch = &sch;
	unsigned char check = 0, *pcheck = &check;
	unsigned char update = 1;
	int fc = 0, *pcount = &fc;	     		
	struct Record_f *rec = NULL;
	
	if(!schema_control(fd_data, pcheck, file_name,data_to_add,&rec, &psch,update,pcount,UPDATE)) {
		printf("invalid schema or an error occured. %s:%d.\n", F,L-2);
		free_schema(psch);
		goto exit_error;
	}

	HashTable ht = {0,NULL}, *pht = &ht;	
	if(!read_index_nr(0,fd_index, &pht)) {
		printf("reading index 0 failed. %s:%d.\n",F,L-2);
		free_record(rec,rec->fields_num);
		free_schema(psch);
		goto exit_error;
	}	

	/*this functions free the memory for the Schema and the Hash Table */
	if(!update_rec(fd_data, pht, psch, file_name, rec, data_to_add, *pcheck, update,*pcount, key)) {
		printf("update failed. %s:%d",F,L-2);
		free_record(rec,rec->fields_num);
		goto exit_error;
	}


	free_record(rec,rec->fields_num);
	if(shared_locks) {
		int result_i = 0, result_d = 0;
		do {
			if((result_i = release_lock_smo(&shared_locks,plp_i,plpa_i)) == 0 ||
			   (result_d = release_lock_smo(&shared_locks,plp,plpa)) == 0) {
				__er_release_lock_smo(F,L-5);
				return 0;
			}

		}while(result_d == WTLK || result_i == WTLK);
		return 1;	
	}

	return 1;
exit_error:
	if(shared_locks) {
		int result_i = 0, result_d = 0;
		do {
			if((result_i = release_lock_smo(&shared_locks,
					plp_i,plpa_i)) == 0 ||
			   (result_d = release_lock_smo(&shared_locks,
					plp,plpa)) == 0) {
				__er_release_lock_smo(F,L-5);
				return 0;
			}

		}while(result_d == WTLK || result_i == WTLK);

		return 0;	
	}
	return 0;	

}

unsigned char __delete(char* file_name, int* fd_index,char* key)
{
	/* this variables are needed for the lock system */
	int lock_pos_i = 0, *plp_i = &lock_pos_i;
	int lock_pos_arr_i = 0, *plpa_i = &lock_pos_arr_i;
	
	/*this is for create the file name to open the file in O_TRUNC */	
	char* suffix = ".inx";
	size_t l = strlen(suffix) + strlen(file_name) + 1;
	char file[l];

	char** files = two_file_path(file_name);
	if(!files) {
		printf("two_file_path() failed, %s:%d.\n",F,L-3);
		return 0;
	}

	/*acquire  WR_IND lock*/
	if(shared_locks) {
		int result_i = 0;
		do {
			off_t fd_i_s = get_file_size(*fd_index,NULL);

			if((result_i = acquire_lock_smo(&shared_locks,plp_i,plpa_i,files[0],0,
					fd_i_s,WR_IND,*fd_index)) == 0) {
				__er_acquire_lock_smo(F,L-5);
				free_strs(2,1,files);
				return 0;
			}
		}while(result_i == MAX_WTLK || result_i == WTLK);
	}

	free_strs(2,1,files);
	files = NULL;

	HashTable *ht = NULL;	
	int ht_i = 0, *pht_i = &ht_i;	
	if(!read_all_index_file(*fd_index,&ht,pht_i)) {
		printf("read all indexes failed. %s:%d.\n",F,L-2);
		goto exit_error;
	}

	if(!delete_rec(*fd_index, key, ht, pht_i)) {
		printf("delete_rec failed. %s:%d.\n",F,L-2);
		free_ht_array(ht,*pht_i);
		goto exit_error;
	}

	close_file(1, *fd_index);
	
	if(snprintf(file,l,"%s%s",file_name,suffix) < 0) {
		printf("file name build failed. %s:%d.\n",F,L-2);
		free_ht_array(ht,*pht_i);
		free(file);
		goto exit_error;
	}

	*fd_index = open_file(file,1);//open with O_TRUNC to overwrite content
        if(file_error_handler(1,*fd_index) > 0) {
		printf("can't open file in O_TRUNC mode %s:%d.\n",F,L-3);
		free_ht_array(ht,*pht_i);
		free(file);
		goto exit_error;
	}	

	if(!write_all_indexes(ht,*fd_index,*pht_i)) {
		printf("write all indexes failed. %s:%d.\n",F,L-2);
		free_ht_array(ht,*pht_i);
		goto exit_error;
	}

	free_ht_array(ht,*pht_i);
	close_file(1,*fd_index);
	*fd_index = open_file(file,0); // reopen in O_RDWR mode
	free(file);
	if(file_error_handler(1,*fd_index) > 0) {
		printf("can't open file in O_RDWR mode %s:%d.\n",F,L-3);
		goto exit_error;
	}	

	if(shared_locks) {
		int result_i = 0;
		do
		{
			if((result_i = release_lock_smo(&shared_locks,
				plp_i,plpa_i)) == 0)
			{
				__er_release_lock_smo(F,L-5);
				return 0;
			}

		}while(result_i == WTLK);
		return 1;	
	}
	return 1;

exit_error:
	if(shared_locks) {
		int result_i = 0;
		do {
			if((result_i = release_lock_smo(&shared_locks,
					plp_i,plpa_i)) == 0) {
				__er_release_lock_smo(F,L-5);
				return 0;
			}

		}while(result_i == WTLK);

		return 0;	
	}
	return 0;
}

unsigned char __write_safe(int fd_data,char* db_data, char* file_name, char **export_key)
{

	/* this variables are needed for the lock system */
	int lock_pos = 0, *plp = &lock_pos;
	int lock_pos_arr = 0, *plpa = &lock_pos_arr;
	int lock_pos_i = 0, *plp_i = &lock_pos_i;
	int lock_pos_arr_i = 0, *plpa_i = &lock_pos_arr_i;
	
	char** files = two_file_path(file_name);
	if(!files) {
		printf("%s() failed, %s:%d.\n",__func__,F,L-3);
		return 0;
	}

	/*acquire WR_IND WR_REC here*/
	if(shared_locks) {
		int result_i = 0, result_d = 0;
		do {
			off_t fd_i_s = get_file_size(-1,files[0]);
			off_t fd_d_s = get_file_size(fd_data,NULL);

			if(((result_i = acquire_lock_smo(&shared_locks,plp_i,plpa_i,files[0],0,
					fd_i_s,WR_IND,0)) == 0) ||
			     ((result_d = acquire_lock_smo(&shared_locks,plp,plpa,files[1],0,
					fd_d_s,WR_REC,fd_data)) == 0)) {
				__er_acquire_lock_smo(F,L-5);
				free_strs(2,1,files);
				return 0;
			}
		}while(result_d == MAX_WTLK || result_d == WTLK 
			|| result_i == MAX_WTLK || result_i == WTLK);
	}

	free_strs(2,1,files);
	files = NULL;
	
	struct Record_f *rec = NULL; 
	if(!schema_control(fd_data,NULL,file_name,db_data,&rec,NULL,0,NULL,NO_OP)) {
		printf("illegal schema, %s:%d.\n",F,L-2);
		goto exit_error;
	}
				
	size_t l = strlen(file_name) + strlen(".inx") + 1 ;
	char file_pt[l];
	memset(file_pt,0,l);

	if(snprintf(file_pt,l,"%s%s",file_name,".inx") < 0) {
		fprintf(stderr,"snprintf() failed.\n");
		goto exit_error;
	}

	int fd_index = open_file(file_pt,0);
	if(file_error_handler(1,fd_index) > 0) {
		printf("error opening file, %s:%d.\n",F,L-2);
		goto exit_error;
	}

	char* key = NULL;
	if(!key_generator(rec,&key,fd_data,fd_index,NO_OP)) {
		printf("key_generator() failed, %s:%d.\n",F,L-2);
		free_record(rec,rec->fields_num);
		if(key)	
			free(key);
		
		goto exit_error;
	}
	
	if(export_key) {
		(*export_key) = strdup(key);
		if(!(*export_key)) {
			fprintf(stderr,"can't export key %s:%d.\n",F,L-2);
			*export_key = NULL;
		}
	}

	if(!__write(file_name,&fd_index,fd_data,NULL,rec,key,NULL,NO_OP)) {
		printf("key_generator() failed, %s:%d.\n",F,L-2);
		free_record(rec,rec->fields_num);
		free(key);
		goto exit_error;
	}

				
	free_record(rec,rec->fields_num);
	free(key);
	if(shared_locks) {
		int result_i = 0, result_d = 0;
		do {
			if(((result_i = release_lock_smo(&shared_locks,
				plp_i,plpa_i)) == 0) ||
	      		   ((result_d = release_lock_smo(&shared_locks,
				plp,plpa)) == 0)) {
				__er_release_lock_smo(F,L-5);
				close_file(1,fd_index);
				return 0;
			}

		}while(result_i == WTLK || result_d == WTLK);
		
		close_file(1,fd_index);
		return 1;	
	}

	close_file(1,fd_index);
	return 1;
exit_error:
	if(shared_locks) {
		int result_i = 0, result_d = 0;
		do {
			if(((result_i = release_lock_smo(&shared_locks,
				plp_i,plpa_i)) == 0) ||
				((result_d = release_lock_smo(&shared_locks,
				plp,plpa)) == 0)) {
				__er_release_lock_smo(F,L-5);
				close_file(1,fd_index);
				return 0;
			}

		}while(result_i == WTLK || result_d == WTLK);
		
		close_file(1,fd_index);
		return 0;	
	}

	close_file(1,fd_index);
	return 0;

}
