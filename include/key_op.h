#ifndef _KEY_OP_H_
#define _KEY_OP_H_

#include "record.h"
#include "hash_tbl.h"


#define TIMECARD 100
#define TIPS 101

#define FSYSK "/u/file_sys.txt"

unsigned char extract_employee_key(char* key_src, char** key_fnd, int mode);
unsigned char extract_time(char* key_src, long* time);
unsigned char key_generator(struct Record_f *rec, char** key, int fd_data, int fd_index,int lock);

#endif /* key_op.h */
