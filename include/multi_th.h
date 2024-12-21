#ifndef _MULTI_TH_H_
#define _MULTI_TH_H_

#define TH_ERR "multi_th error"
#define TH_SUC "multi_th succeed"
#define THREAD_POOL 10 /* number of threads */

/* for Queue*/
#include "queue.h"

/* arguments structure to hold thread arguments */
typedef struct
{
	int socket_client;
	char* data_from_socket;

}Th_args;

/*pointer function*/
extern void* (*interface)(void*);


typedef struct
{
	void* (*interface)(void*);
	void* arg; /*to be casted to Th_args*/	
}task_db;

typedef struct
{
	pthread_t threads[THREAD_POOL];
	pthread_mutex_t lock;
	pthread_cond_t notify;
	Queue* tasks;
	int stop;	
}Thread_pool;

unsigned char new_thread(void* arg,void* (*interface)(void*));
void* principal_interface(void* arg);
unsigned char pool_init(Thread_pool* pool);
void* working_thread(void* th_pool);
#endif
