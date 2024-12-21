#ifndef COMMON_H
#define COMMON_H

#define STATUS_ERROR -1
#define SCHEMA_ERR 2
#define SCHEMA_EQ 3
#define SCHEMA_NW 4
#define SCHEMA_CT 5
#define UPDATE_OLD 6 //you can overwrite the old record with no worries!
#define UPDATE_OLDN 8
#define ALREADY_KEY 9
#define IND_FLD 10
#define REC_NTF 11
#define UPDATE 12 // update a record -Used in the function schema_control()
#define STD_OP 13 // write delete and add record -Used in the function schema_control()
#define LK_REQ 14 // used in schema_control to aquire locks if needed!
#define NO_OP 15 // used in schema control as a std value to not perform locks.
#define NO_EMPLOYEE_CLOCKED_IN 14 // 
#define EUSER 16 /* new user creation failed*/


/* instruction included in the json object */
typedef enum
{
	WRITE_EMP, /* 0 */
	UPDATE_EMP,
	WRITE_TIP,
	CK_IN,
	CK_OUT,
	NW_REST
}Instructions;


#endif
