#ifndef TIMECARD_H
#define TIMECARD_H

unsigned char clock_in(char* employee_id);
unsigned char clock_out(char* employee_id); 
unsigned char list_clocked_in_employee(char*** employee_list, int* size);


#endif
