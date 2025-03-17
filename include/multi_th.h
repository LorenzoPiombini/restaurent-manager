#ifndef _MULTI_TH_H_
#define _MULTI_TH_H_

#define TH_ERR "multi_th error"
#define TH_SUC "multi_th succeed"
#define THREAD_POOL 10 /* number of threads */

/* for Queue*/
#include "queue.h"


enum IO 
{
	WRIO,
	RDIO
};

/* 
 * arguments structure to hold thread arguments 
 *  - we saved all the data regarding the client 
 *    conection, we need to keep track of all the operations
 *    on the client socket since is non blocking.
 *
 * */
struct Th_args {
	int socket_client;
	char data_from_socket[1000];
	int epoll_fd;
	int op;
};

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
	struct Queue* tasks;
	int stop;	
}Thread_pool;

unsigned char new_thread(void* arg,void* (*interface)(void*));
void* principal_interface(void* arg);
unsigned char pool_init(Thread_pool* pool);
void* working_thread(void* th_pool);
void pool_destroy(Thread_pool* pool);

#endif
