#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h> /* for sockaddr_in */
#include <sys/epoll.h>
#include <errno.h>
#include "file.h"
#include "parse.h"
#include "business.h"
#include "str_op.h"
#include "key_op.h"
#include "crud.h"
#include "common.h"
#include "record.h"
#include "lock.h"
#include "index.h"
#include "date.h"
#include "restaurant_t.h"
#include "db_instruction.h"
#include "timecard.h"
#include "json.h"
#include "multi_th.h"
#include "queue.h"
#include "com.h" /*for listen_set_up()*/
#include "debug.h"


int main(int argc, char** argv)
{
		
		if(argc == 1)
		{	
			printf("you must provide an option!\n");
			return 1;
		}

		int arg = atoi(argv[1]);
if(arg == 0)
{	

	/*this should be the real main program*/
        int smo = 0;
        int buff_size = 1000;
        char instruction[buff_size];
	Th_args* arg_st = NULL;

        /* epoll() setup variables*/
        struct epoll_event ev;
        struct epoll_event events[10];
        int nfds = 0;
        int epoll_fd = -1;
               
        /*checking if the regex compiled succesfully for json parsng*/
        int rgx = -1;
        if((rgx = init_rgx()) != 0)
                goto handle_crash;

        /*
	 * SOCKET setup
	 * listen_set_up will create a NON BLOCKING stream socket,
	 * and call the function bind() and listen() to set up 
	 * the connection
	 * */
	int fd_client = -1;
	int fd_socket = -1;
	if(!listen_set_up(&fd_socket,AF_INET,SOCK_STREAM | SOCK_NONBLOCK,5555)) {
		printf("listen_set_up() failed %s:%d.\n",F,L-2);
                goto handle_crash;
	}
        

       if(start_SSL(&ctx,"null") == -1)
               goto handle_crash;


        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {  
                fprintf(stderr,"can't generate epoll.\n");
                goto handle_crash;
        }

        ev.events = EPOLLIN;
        ev.data.fd = fd_socket;

        if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD, fd_socket, &ev) == -1) {
                fprintf(stderr,"epoll_ctl failed");        
                goto handle_crash;
        }

        /*init the thread pool */
	Thread_pool pool = {0};

	Queue q = {NULL,NULL,0};
	if(!q_init(&q)) {	
        	printf("q_init() failed, %s:%d,\n",F,L-2);
                goto handle_crash;
	}

	pool.tasks = &q;
	if(!pool_init(&pool))
	{
		printf("thread pool init failed, %s:%d.\n",F,L-2);
                goto handle_crash;
	}

        /*shared locks is declared as a global variable in lock.h and define as NULL
             inside lock.c */
	if(!set_memory_obj(&shared_locks)) {
		printf("set_memory_obj() failed, %s:%d.\n",F,L-2);
                goto handle_crash;
        }

        smo = 1;
		
        

	for(;;)
        {
                nfds = epoll_wait(epoll_fd, events, 10,-1);
                if(nfds == -1) {
			if(errno == EINTR)
				continue;

                        fprintf(stderr,"epoll_wait() failed.\n");
                        goto handle_crash;
                }

                for(int i = 0; i < nfds; ++i) {
                        int res = 0;
			if(events[i].events == EPOLLIN && 
					events[i].data.fd == fd_socket) {
				if((res = accept_instructions(&fd_socket,&fd_client,
							instruction,buff_size,epoll_fd)) == 0) {
					printf("accept_instructions() failed %s:%d.\n",F,L-2);
					continue;
				}

				if(res == EAGAIN || res == EWOULDBLOCK || res == EPOLL_ADD_E)
					continue;

				if(res == CLI_NOT) {
				/*
				* a not authorized client tried to connect
				* so we resume the loop without perform any instruction
				**/
                                close(fd_client);
                                continue;
				}

				if(res == DT_INV) {
					close(fd_client);
					continue;
				}

				arg_st = calloc(1,sizeof(Th_args));
				if(!arg_st) {
					__er_calloc(F,L-3);
					goto handle_crash;
				}

				arg_st->socket_client = fd_client;
				arg_st->data_from_socket = strdup(instruction);
			
				/*put the function in a task que*/
				void* (*interface)(void*) = principal_interface;
			
				task_db* task = calloc(1,sizeof(task_db));
				task->interface = interface;
				task->arg = (void*)arg_st;

				if(!enqueue(&q,(void*)task)) {
					printf("enqueue() failed, %s:%d,\n",F,L-2);
					goto handle_crash;
				}
				pthread_cond_signal(&pool.notify);
				memset(instruction,0,buff_size);

			} else if() {
				/*
				 * the file descriptor is a 
				 * client so we have to retry
				 * reading or write operation 
				 * */
					
			}
                }
	}
	/*handle a gracefull crash*/
        
handle_crash:
        /*free regex for json parsing*/
        if(rgx == 0)
                REG_FREE

        if(smo)
	        free_memory_object(SH_ILOCK);

        /*free resourses for thread pool*/
        if(pool.tasks)
                pool_destroy(&pool);

        if(q.front)
                q_free(&q);

        if(fd_socket > -1 )
	        close(fd_socket);

        //if(fd_client > -1 )
	        //close(fd_client);

	//if(arg_st) {
               // free(arg_st->data_from_socket);
               // free(arg_st);
        //}

        if(ctx)
                SSL_ctx_free(ctx);


        return EXIT_FAILURE;
}
		/*---- main program end*/
if(arg == 1)
{
	/*char* json[] ={ "{\"empty\":\"\"}",  this should fail 
			"{\"e_array\":\"[]\"}", this should fail 
			"{\"active\":false,\"active\":false,\"name\":\"example\",\"score\":99.5,\"result\":null}",
			"{\"this\":true}",
			"{\"this\":false}",
			"{\"this\":\"json\",\"but\":\"this_is_complex\", \"array\":[\"string\",\"string\",[1,2.3,3]]}",
			"{\"this\":\"json\"}",
			"{\"this\":null}",
			"{\"number\":678.99}",
			"{\"number\":[678.99,23,90.98]}",
			"{\"number\":[678.99,23,90.98,[678.99,23,90.98,78],[678.99,23,90.98,78],98.88]}",
			"{\"outer\":{\"inner\":{\"key\":\"value\",\"number\":[678.99,23,90.98]}}}",
			"{\"number\":[{\"e16Piombini\":\"clock_out\"},678.99,23,90.98,{\"key\":\"value\",\"number\":[1,2,3]}]}",
			"{\"number\":[{\"e16Piombini\":\"clock_out\"},678.99,23,90.98,{\"key\":\"value\",\"number\":[1,2,3],\"nested\":{\"this\":false}}]}"
			};	
	*/
	int fd_mo = shm_open(SH_ILOCK,O_RDWR,0666);
        if(fd_mo != -1)
        {
          /*shared locks is declared as a global variable in lock.h and define as NULL
              inside lock.c */
          shared_locks = mmap(NULL, sizeof(lock_info)*MAX_NR_FILE_LOCKABLE,
                                  PROT_READ | PROT_WRITE, MAP_SHARED, fd_mo, 0);
        }

	char* test = "{\"instruction\":5,\"username\":\"Robert\",\"password\":\"help98!\"}";
	JsonPair** pairs = NULL;
	int psize = 0;
	if(!parse_json_object(test,&pairs,&psize))
		return EXIT_FAILURE;

	BST_init;
	if(!create_json_pair_tree(&pairs, psize, &BST_tree))
		return EXIT_FAILURE;


	int inst = (int)pairs[0]->value->data.i;	
	free_json_pairs_array(&pairs,psize);
					
	if(!convert_pairs_in_db_instruction(BST_tree,inst)) {
       free_BST(&BST_tree); 
		return EXIT_FAILURE;
    }
	/*int k = 0, l = 1;
	for(k = 0; k < l; k++)
		if(!parse_json_object(test))
			printf("\b %s\n",test);
	*/
	return EXIT_SUCCESS;

}

if(arg == 2)
{ 
/*	for(int j = 0; j < 10000; j++)
	{*/	
		if(!submit_tips(0, 48,1))
		{
			printf("submit_tips() failed, %s:%d.\n",F,L-2);
			return 1;
		}
	
	/*	char* tipsKey[] = {
					"t2811-17-24e11De Jesus0",
					"t2611-17-24e8Bonilla0",
					"t2711-17-24e0Barboza0"
				  };

		int fd_ti = open_file("tips.inx",0);
		for(int i = 0; i < 3; i++)
		{
			if(!__delete("tips",&fd_ti,tipsKey[i]))
			{
				printf("__delete() failed, %s:%d.\n",F,L-2);
				close_file(1,fd_ti);
				return 0;
			}
		}
	
		close_file(1,fd_ti);
	
	}*/
	return 0;
}
#if 0 
      	if(!submit_cash_tips(100,"e16Piombini",1))
	{
		printf("submit cash failed.\n");
		return 1;
	}
	
	return 0;
#endif
if(arg == 3)	 
{
	if(!clock_in("e0Barboza"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	/*if(!clock_in("e16Piombini"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_in("e9Da_Silva"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_in("e18Torres"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}
*/
	if(!clock_in("e8Bonilla"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_in("e11De_Jesus"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	
	return 0;
}
#if 0
	if(!clock_in("e16Piombini"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}
#endif
#if 0
	if(!clock_in("e17Rosero"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_in("e18Torres"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}
	if(!clock_in("e11De Jesus"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}


#endif
#if 0 
	return 0;
#endif
if(arg == 4)
{
/*	if(!clock_out("e0Barboza"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_out("e16Piombini"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_out("e9Da_Silva"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_out("e8Bonilla"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_out("e18Torres"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}
*/
	if(!clock_out("e11De_Jesus"))
	{	
		printf("clock in failed. %s:%d.\n",F,L-2);
		return 1;
	}

	
	return 0;

}
#if 0
	if(!clock_out("e17Rosero"))
	{	
		printf("clock out failed. %s:%d.\n",F,L-2);
		return 1;
	}


	if(!clock_out("e11De Jesus"))
	{	
		printf("clock out failed. %s:%d.\n",F,L-2);
		return 1;
	}

	if(!clock_out("e16Piombini"))
	{	
		printf("clock out failed. %s:%d.\n",F,L-2);
		return 1;
	}
#endif



//	if(!clock_in("e0Barboza"))
//	{	
//		printf("clock in failed. %s:%d.\n",F,L-2);
///		return 1;
//	}

//	if(!clock_out("e0Barboza"))
//	{	
//		printf("clock out failed. %s:%d.\n",F,L-2);
//	}	

//	return 0;



#if 0
	clock_t start, finish;
	start = clock();

	int fd_td = open_file("tips.dat",0);
	int fd_ti = open_file("tips.inx",0);

	Record_f*** result = NULL;
	Record_f** rec = NULL;
	int size = 0, *ps = &size ;
	int* sizes = NULL;
	
	if(!filter_tip_service(CUR_WK,fd_ti,fd_td,&rec,&result,NULL,LUNCH,ps,&sizes,0,NULL))
	{
		printf("filter_tip_service failed. %s:%d.\n",F,L-2);
		close_file(2,fd_ti,fd_td); 
		return 1;
	}

	char** emp_k = NULL;
	float* totals = NULL;
	int emp_t = 0, *pemp_t = &emp_t, tot_t = 0, *ptot_t = &tot_t;	
			
	if(!total_credit_card_tips_each_employee(&result,*ps,sizes,&totals,&emp_k,pemp_t,ptot_t))
	{
		printf("totaling tips failed. %s:%d.\n",F,L-2);
		free_array_of_arrays(size,&result,sizes,size);
		free(sizes);
		close_file(2,fd_ti,fd_td); 
		return 1;
	}

	
	int i = 0;
	for(i = 0; i < *ptot_t; i++)
	{
		printf("%s total tips: %.2f.\n",emp_k[i],totals[i]);
	}

	free(totals);
	free_strs(*pemp_t,1,emp_k);
	free_array_of_arrays(size,&result,sizes,size);
	free(sizes);
	close_file(2,fd_ti,fd_td); 
	finish = clock();
	printf("\n process took %f seconds to execute.\n",((double) (finish - start))/CLOCKS_PER_SEC);
	return 0;
#endif
#if 0
	clock_t start, finish;
	start = clock();
	int fd_td = open_file("tips.dat",0);
	int fd_ti = open_file("tips.inx",0);
	//int fd_ed = open_file("employee.dat",0);
	//int fd_ei = open_file("employee.inx",0);
	
	/* get tips each employees current week*/

	Record_f*** result_t = NULL;
	int* sizes = NULL, result_size = 0, *pr_sz = &result_size;
	if(!get_tips_employees_week(CUR_WK,fd_ti,fd_td,&result_t,&sizes,pr_sz))
	{
		printf("get_tips_employees_current_week failed.%s:%d\n",F,L-2);
		close_file(2,fd_ti,fd_td); 
		return 1;
	}

	
	char** emp_k = NULL;
	float* totals = NULL;
	int emp_t = 0, *pemp_t = &emp_t, tot_t = 0, *ptot_t = &tot_t;	
			
	if(!total_credit_card_tips_each_employee(&result_t,*pr_sz,sizes,&totals,&emp_k,pemp_t,ptot_t))
	{
		printf("totaling tips failed. %s:%d.\n",F,L-2);
		free_array_of_arrays(result_size,&result_t,sizes,result_size);
		free(sizes);
		close_file(2,fd_ti,fd_td); 
		return 1;
	}
	
	free_array_of_arrays(result_size,&result_t,sizes,result_size);
	free(sizes);
	int i = 0;
	for(i = 0; i < *ptot_t; i++)
	{
		printf("%s total tips: %.2f.\n",emp_k[i],totals[i]);
	}

	free(totals);
	free_strs(*pemp_t,1,emp_k);
	close_file(2,fd_ti,fd_td); 
	finish = clock();
	
	
	printf("\n process took %f seconds to execute.\n",((double) (finish - start))/CLOCKS_PER_SEC);
	return 0;
#endif

#if 0 
	clock_t start, finish;
	start = clock();
	int fd_td = open_file("tips.dat",0);
	int fd_ti = open_file("tips.inx",0);
	//int fd_ed = open_file("employee.dat",0);
	//int fd_ei = open_file("employee.inx",0);
	
	/* get tips each employees current week*/

	Record_f*** result_t = NULL;
	int* sizes = NULL, result_size = 0, *pr_sz = &result_size;
	if(!get_tips_employees_week(CUR_WK,fd_ti,fd_td,&result_t,&sizes,pr_sz))
	{
		printf("get_tips_employees_current_week failed.%s:%d\n",F,L-2);
		close_file(2,fd_ti,fd_td); 
		return 1;
	}


	Record_f*** filtered_r = NULL;
	int size = 0, *ps = &size;
	int* sizes_fts = NULL;
	if(!filter_tip_service(CUR_WK,-1,-1,NULL,&result_t,&filtered_r,LUNCH,ps,&sizes,result_size,&sizes_fts))
	{
		printf("filter_tip_service failed. %s:%d.\n",F,L-2);
		free_array_of_arrays(result_size,&result_t,sizes,result_size);
		free(sizes);
		close_file(2,fd_ti,fd_td); 
		return 1;
	}

	free_array_of_arrays(result_size,&result_t,sizes,result_size);
	free(sizes);
	
	char** emp_k = NULL;
	float* totals = NULL;
	int emp_t = 0, *pemp_t = &emp_t, tot_t = 0, *ptot_t = &tot_t;	
			
	if(!total_credit_card_tips_each_employee(&filtered_r,*ps,sizes_fts,&totals,&emp_k,pemp_t,ptot_t))
	{
		printf("totaling tips failed. %s:%d.\n",F,L-2);
		free_array_of_arrays(size,&filtered_r,sizes_fts,size);
		free(sizes);
		close_file(2,fd_ti,fd_td); 
		return 1;
	}
	
	int i = 0;
	for(i = 0; i < *ptot_t; i++)
	{
		printf("%s total tips: %.2f.\n",emp_k[i],totals[i]);
	}

	free(totals);
	free_strs(*pemp_t,1,emp_k);
	free_array_of_arrays(size,&filtered_r,sizes_fts,size);
	free(sizes_fts);
	close_file(2,fd_ti,fd_td); 
	finish = clock();
	
	
	printf("\n process took %f seconds to execute.\n",((double) (finish - start))/CLOCKS_PER_SEC);
	return 0;
#endif
	/* get_rec works just fine */
	/* you pass a pointer to a variable index, as the index that you want to read from */
	/* the fucntions will use it and store the length of the array Record_f** recs.*/
	/* if the fuction fails it takes care of the memory */
	/* if the fucntions succeed we have our recs populated and is size in *p_index or index */
	
	/*----------------------- this shows how to use it --------------------------------------*/
	/*											 */
	/*	Record_f** recs = calloc(1,sizeof(Record_f*));					 */
	/*	if(!recs)									 */
	/*	{										 */
	/*		printf("calloc failed, %s:%d",F,L-3);					 */
	/*		return 0;								 */
	/*	}										 */
	/*											 */
	/*	int index = 0, *p_index = &index;						 */
	/*	if(!get_rec(fd_td,fd_ti,p_index,"t808-20-24e9Da Silva2",&recs, "tips")) 	 */
	/*	{										 */
	/*		printf("get_rec failed %s:%d.\n",F,L-2);				 */
	/*		close_file(4,fd_ei,fd_ti,fd_ed,fd_td); 					 */
	/*		free_record_array(*p_index,&recs);					 */
	/*		return 1;								 */
	/*	}										 */
	/*---------------------------------------------------------------------------------------*/
	
//	print_record(*p_index, recs);
//	free_record_array(*p_index,&recs);
	/* get schedule this week and past week */
	/* get clock in and out for a specific day */
	/* show all week for an emplyee */
	/* get tips specific day */
	
	
	//if(!__delete("tips", &fd_ei,"t008-14-24e0Saruppo"))
	//{
	//	printf("delete failed. %s:%d.\n",F,L-2);
	//	return 1;
	//}

	//char* data_to_add = "amount:t_f:67.90:date:t_s:08-14-24:employee_id:t_s:e0Saruppo";
	//if(!__write("tips",&fd_ti, fd_td, data_to_add))
	//{
	//	printf("write to file failed, %s:%d.\n",F,L-2);
	//	close_file(4,fd_ti,fd_td,fd_ei,fd_ed);
	//	return 1;
//	}
	
if(arg == 5)
{	
	int staff = 19;
	char* employees[staff];
	
	employees[0] = "name:t_s:Felipe:last_name:t_s:Barboza:shift_id:t_i:9:role:t_i:0";
	employees[1] = "name:t_s:Labinot:last_name:t_s:Alidemaj:shift_id:t_i:9:role:t_i:0";
	employees[2] = "name:t_s:Gloria:last_name:t_s:Alvaro:shift_id:t_i:9:role:t_i:2";
	employees[3] = "name:t_s:David:last_name:t_s:Arrieta:shift_id:t_i:9:role:t_i:0";
	employees[4] = "name:t_s:Maria:last_name:t_s:Badilla:shift_id:t_i:9:role:t_i:9";
	employees[5] = "name:t_s:Marin:last_name:t_s:Belluche:shift_id:t_i:9:role:t_i:6";
	employees[6] = "name:t_s:Arber:last_name:t_s:Berisha:shift_id:t_i:9:role:t_i:0";
	employees[7] = "name:t_s:Dilin:last_name:t_s:Bitici:shift_id:t_i:9:role:t_i:2";
	employees[8] = "name:t_s:Joel:last_name:t_s:Bonilla:shift_id:t_i:9:role:t_i:9";
	employees[9] = "name:t_s:Bruno:last_name:t_s:Da_Silva:shift_id:t_i:9:role:t_i:0";
	employees[10] = "name:t_s:Karen:last_name:t_s:Dayana:shift_id:t_i:9:role:t_i:9";
	employees[11] = "name:t_s:Johan:last_name:t_s:De_Jesus:shift_id:t_i:9:role:t_i:2";
	employees[12] = "name:t_s:Kushtrim:last_name:t_s:Laiqi:shift_id:t_i:9:role:t_i:2";
	employees[13] = "name:t_s:Nicole:last_name:t_s:Leka:shift_id:t_i:9:role:t_i:9";
	employees[14] = "name:t_s:Celina:last_name:t_s:Lopez:shift_id:t_i:9:role:t_i:2";
	employees[15] = "name:t_s:Kassandra:last_name:t_s:Martinez:shift_id:t_i:9:role:t_i:0";
	employees[16] = "name:t_s:Lorenzo:last_name:t_s:Piombini:shift_id:t_i:9:role:t_i:0";
	employees[17] = "name:t_s:Bismar:last_name:t_s:Rosero:shift_id:t_i:9:role:t_i:2";
	employees[18] = "name:t_s:Ada:last_name:t_s:Torres:shift_id:t_i:9:role:t_i:2";
//	char* tips[6];
//	tips[0] = "amount:t_f:67.7:date:t_s:09-03-24:employee_id:t_s:e0Barboza";	
//	tips[1] = "amount:t_f:32.6:date:t_s:09-02-24:employee_id:t_s:e16Piombini";	
//	tips[2] = "amount:t_f:23.7:date:t_s:09-02-24:employee_id:t_s:e9Da Silva";	
//	tips[3] = "amount:t_f:24.8:date:t_s:09-03-24:employee_id:t_s:e9Da Silva";	
//	tips[4] = "amount:t_f:25.23:date:t_s:09-02-24:employee_id:t_s:e3Arrieta";	
//	tips[5] = "amount:t_f:17.27:date:t_s:09-03-24:employee_id:t_s:e3Arrieta";	
//	int i = 0;
//	for(i = 0; i < 18; i++)
//	{
//		if(!__write("employee",&fd_ei,fd_ed,employees[i]))
//		{
//			printf("write to file failed, %s:%d.\n",F,L-2);
//			close_file(4,fd_ti,fd_td,fd_ei,fd_ed);
//			return 1;
//		}

//	} 

	int fd_ed = open_file("employee.dat",0);
	for(int i = 0; i < staff; i++)
	{
		
		
		if(!__write_safe(fd_ed,employees[i],"employee",NULL))
		{
			printf("__write_safe() failed %s:%d.\n",F,L-2);	
			close_file(1,fd_ed);
			return 1;
		}

	}	
	return 0;
}
//	Record_f*** result = NULL;
//	Record_f** rec = NULL;
//	int size = 0, *ps = &size, j = 0;
//	int* sizes = NULL;
	
//	if(!get_tip_service(1,fd_ti,fd_td,&rec,&result,LUNCH,ps,&sizes))
//	{
//		printf("get_tip_service failed. %s:%d.\n",F,L-2);
//		return 1;
//	}
	
//	char** emp_k = NULL;
//	float* totals = NULL;
//	int emp_t = 0, *pemp_t = &emp_t, tot_t = 0, *ptot_t = &tot_t;	
//	if(result)
//	{		
//		if(!total_credit_card_tips_each_employee(&result,*ps,sizes,&totals,&emp_k,pemp_t,ptot_t))
//		{
//			printf("totaling tips failed. %s:%d.\n",F,L-2);
//			free_array_of_arrays(*ps,&result,sizes,*ps);
//			free(sizes);
//			return 1;
//		}

//		for(i = 0; i < *ptot_t; i++)
//		{
//			printf("%s total tips: %.2f.\n",emp_k[i],totals[i]);
//		}

//		free(totals);
//		free_strs(*pemp_t,1,emp_k);
		
//		int ix = 0, *p_ix = &ix;
//		for(i = 0; i < *ps; i++)
//		{
//			for(j = 0; j < sizes[i]; j++)
//			{
//				Record_f** recs_emplo = calloc(1,sizeof(Record_f*));
//				if(!recs_emplo)
//				{
//					printf("calloc failed. %s:%d.\n",F,L-3);
//					close_file(4,fd_ti,fd_td,fd_ei,fd_ed);
//					return 1;
//				}
//				if(!get_rec(fd_ed,fd_ei,p_ix,result[i][j]->fields[4].data.s,
//						&recs_emplo, "employee"))
//				{
//					printf("get_rec failed %s:%d.\n",F,L-2);
//					free_record_array(*p_ix,&recs_emplo);
//					close_file(4,fd_ti,fd_td,fd_ei,fd_ed);
//					return 1;
//				}

//				printf("date %s, employee %s, amount %.2f.\n",result[i][j]->fields[2].data.s,
//								    recs_emplo[0]->fields[0].data.s,
//								    result[i][j]->fields[0].data.f);
			
			
//				free_record_array(*p_ix,&recs_emplo);
//				*p_ix = 0;
//			}
//		}

//		free_array_of_arrays(*ps,&result,sizes,*ps);
		
//	}

//	free(sizes);

	return 0;
}
