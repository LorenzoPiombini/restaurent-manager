#ifndef CRUD_H
#define CRUD_H

/* for Record_f */
#include "record.h"

/* for HashTable */
#include "hash_tbl.h"

/* for Schema */
#include "parse.h"


/*pointer to an indexing fucntion (indexing functions prototypes are declared in index.h)*/
typedef unsigned char (*indexing)(HashTable *ht, char* key);


unsigned char read_indexes(char* file_name,int fd_index,int index_nr,HashTable* ht, int* ht_array_size);
unsigned char is_a_db_file(int fd_data, char* file_name, struct Header_d *hd_caller);
unsigned char get_rec(int fd_dat,int fd_inx, int* index, void* key, int key_type, struct Record_f ***recs, char* file_name, int lock);
unsigned char schema_control(int fd_data, unsigned char* check_s, char* file_name,char* data_to_add,
				 struct Record_f **rec_c,struct Schema **sch_c, unsigned char update,
				 int *pcount, int crud);
unsigned char write_rec(int fd_data, int fd_index, HashTable* ht,struct Record_f *rec, void* key, int key_type);
unsigned char delete_rec(int fd_inx, void* key,int key_type, HashTable *ht, int* p_i);
unsigned char update_rec(int fd_data, HashTable* ht, struct Schema* sch, char* file_path,
			 struct Record_f *rec, char* buffer, unsigned char check, unsigned char update,
			 int fields_count, void* key, int key_type);
unsigned char write_all_indexes(HashTable* ht,int fd, int index);
unsigned char __write(char* file_name, int* fd_index, int fd_data, char* data_to_add,
			struct Record_f *rec, void* key,int key_type, indexing ixgn, int lock);
unsigned char __update(char* file_name, int fd_index, int fd_data, char* data_to_add,void *key, int key_type);
unsigned char __delete(char* file_name, int* fd_index, void* key, int key_type);
unsigned char __write_safe(int fd_data,char* db_data, char* file_name, char **export_key);

#endif /*crud.h*/
