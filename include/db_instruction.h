#ifndef _DB_INSTRUCTION_H
#define _DB_INSTRUCTION_H

/* for BST */
#include "bst.h"

/* for Instructions */
#include "common.h"

/**/
#define REST_HM "restaurant hm"
#define REST_ID "restaurant id"

/* files in the restaurant system*/
#define EMPL "employee.dat"
#define FSYS "/u/file_str.txt"
#define GLUSR "/u"


/* login succesful */
#define S_LOG 20

struct login_u{
	char* home_pth;
	int uid;	
};

extern struct login_u user_login;

unsigned char convert_pairs_in_db_instruction(BST pairs_tree,Instructions inst);


#endif /* db_instruction.h */
