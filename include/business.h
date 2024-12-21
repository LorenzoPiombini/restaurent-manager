#ifndef BUSINESS_H
#define BUSINESS_H

#define TODAY 1
#define CUR_WK 2
#define LST_WK 3

/* for Record_f*/
#include "record.h"

/* for Instructions */
#include "common.h"

/* for JsonPair */
#include "json.h"

/* get tips each employees for the current week*/
unsigned char get_tips_employees_week(int week, int tips_index, int tips_data, struct Record_f ****recs_tr,
					int **sizes, int *pr_sz);
unsigned char filter_tip_service(int index,int fd_index, int fd_data, struct Record_f ***recs, struct Record_f ****result,
				struct Record_f ****filtered_r, int service, int* ps,
				int **sizes,int result_size, int **sizes_fts);
unsigned char total_credit_card_tips_each_employee(struct Record_f ****result, int size, int* sizes,
					 float **totals, char ***emp_k,
					 int *pemp_t, int *ptot_t);

unsigned char compute_tips(float credit_card_tips, float cash_tips, int service, char **employee_list, int size);
unsigned char submit_cash_tips(float cash, char *employee_key, int service);
unsigned char submit_tips(float c_card, float cash, int service);
/* get tips for each employees for the current month */ 
/* get schedule this week and past week */
/* get clock in and out for a specific day */
/* tips current week for an employee */
/* get tips specific day */



#endif
