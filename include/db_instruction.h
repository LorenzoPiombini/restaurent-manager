#ifndef _DB_INSTRUCTION_H
#define _DB_INSTRUCTION_H

/* for BST */
#include "bst.h"

/* for Instructions */
#include "common.h"

/**/
#define REST_ID "restaurant_id"

/* files in the restaurant system*/
#define EMPL "employee.dat"
#define FSYS "/u/file_sys.txt"
#define GLUSR "/u/global_file_sys.txt"


/* login succesful */
#define S_LOG 20

struct login_u{
	char* home_pth;
	int uid;	
};

unsigned char convert_pairs_in_db_instruction(BST pairs_tree,Instructions inst);


#endif /* db_instruction.h */
