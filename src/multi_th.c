#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "multi_th.h"
#include "str_op.h"
#include "json.h"
#include "file.h"
#include "bst.h"
#include "debug.h"
#include "queue.h"
#include "db_instruction.h"


unsigned char pool_init(Thread_pool* pool)
{
	pool->stop = 0;

	pthread_mutex_init(&(*pool).lock,NULL);
	pthread_cond_init(&(*pool).notify,NULL);

	for(int i = 0; i < THREAD_POOL; i++ )
	{
		if(pthread_create(&pool->threads[i], NULL, working_thread,pool))
		{
			printf("pthread_create() failed, %s:%d.\n",F,L-2);
			return 0;
		}
	}

	return 1;
}


void* working_thread(void* th_pool)
{
	Thread_pool* pool= (Thread_pool*)th_pool;  
	
	for(;;)
	{
		pthread_mutex_lock(&(*pool).lock);	

		while((*pool).tasks->length == 0 && !(*pool).stop)
		{
			pthread_cond_wait(&(*pool).notify,&(*pool).lock);
		}

		if(pool->stop)
		{
			pthread_mutex_unlock(&(*pool).lock);
			pthread_exit(NULL);
		}

		task_db* task = (task_db*)(*pool).tasks->front->next->value;
	       	if(!dequeue((*pool).tasks))
		{
			printf("dequeue failed %s:%d.\n",F,L-2);
			pthread_mutex_unlock(&(*pool).lock);
			pthread_exit(NULL);
		}	
		
		pthread_mutex_unlock(&(*pool).lock);

		(*(task->interface))(task->arg);	
		free(task);
		
	}
}

void* principal_interface(void* arg)
{
	char* err = TH_ERR;
	char* suc = TH_SUC;

	struct Th_args* arg_st = ((struct Th_args*)arg);	
	
 	JsonPair** pairs = NULL;
	int psize = 0;
	if(!parse_json_object(arg_st->data_from_socket,&pairs,&psize))
	{
		printf("parse_json_object() failed, %s:%d.\n",F,L-2);
		char* message = "{\"status\":\"error\"}";
		size_t size = strlen(message);
		if(write(arg_st->socket_client,message,size) == -1)
		{
			close_file(1,arg_st->socket_client);
			return (void*)err;
		}
		close_file(1,arg_st->socket_client);
		free(arg_st->data_from_socket);
		free(arg_st);
		return (void*)err;
	}
	
	BST_init;/*declere the Binary search tree (AVL tree)*/

	if(!create_json_pair_tree(&pairs, psize, &BST_tree))
	{
		printf("create_json_pair_tree() failed %s:%d.\n",F,L-2);
		free_json_pairs_array(&pairs,psize);
		free_BST(&BST_tree);
		char* message = "{\"status\":\"error\"}";
		size_t size = strlen(message);
		if(write(arg_st->socket_client,message,size) == -1)
		{
			close_file(1,arg_st->socket_client);
			return (void*)err;
		}
		close_file(1,arg_st->socket_client);
		free(arg_st->data_from_socket);
		free(arg_st);
		return (void*)err;
	}
	
	/* extract the type of operation to perform on the database */
	int inst = (int)pairs[0]->value->data.i;

	free_json_pairs_array(&pairs,psize);
	
	int ret = convert_pairs_in_db_instruction(BST_tree,inst);

	if(ret == 0 || ret == EUSER) {
		printf("convert_pairs_in_db_instruction() failed, %s:%d.\n",F,L-2);
		free_BST(&BST_tree);
		char message[] = "{\"status\":\"error\"}";
		size_t size = strlen(message);
		if(write(arg_st->socket_client,message,size) == -1) {
			close_file(1,arg_st->socket_client);
			return (void*)err;
		}
		
		close_file(1,arg_st->socket_client);
		free(arg_st->data_from_socket);
		free(arg_st);
		return (void*)err;
	}
	

	if(ret == S_LOG) {
		char message[1000];
		memset(message,0,1000);
		/*
		 * if login ok, send back user id and user home path
		 * - 1 is for '\0'
		 * - 2 is for two count of ":"
		 * - 2 is for braces  "{}"
		 **/
		size_t l = strlen(user_login.home_pth) + 
				number_of_digit(user_login.uid)
				+ strlen("\"status\":\"succeed\",")
				+ strlen("\"home_path\":\"\",")
				+ strlen("\"uid\":\"\"") + 1 + 2 + 2;

		if(snprintf(message,l,
				"{\"status\":\"succeed\",\"home_path\":\"%s\",\
				\"uid\":\"%d\"}",user_login.home_pth, 
				user_login.uid) < 0) {
			fprintf(stderr,"snprintf() failed.%s:%d.\n",
					F,L-5);
			free_BST(&BST_tree);
			char error[] = "{\"status\":\"error\"}";
			size_t size = strlen(error);
			if(write(arg_st->socket_client,error,size) == -1) {
				close_file(1,arg_st->socket_client);
				free(arg_st->data_from_socket);
				free(user_login.home_pth);
				free(arg_st);
				return (void*)err;
			}
			close_file(1,arg_st->socket_client);
			free(arg_st->data_from_socket);
			free(arg_st);
			free(user_login.home_pth);
			return (void*)err;
		}

		free_BST(&BST_tree);
		size_t size = strlen(message);

		if(write(arg_st->socket_client,message,size) == -1) {
			close_file(1,arg_st->socket_client);
			free(user_login.home_pth);
			free(arg_st->data_from_socket);
			free(arg_st);
			return (void*)err;
		}
		
		free(user_login.home_pth);
		close_file(1,arg_st->socket_client);
		free(arg_st->data_from_socket);
		free(arg_st);
		return (void*)suc;
	}

	free_BST(&BST_tree);
	char* message = "{\"status\":\"succeed\"}";
	size_t size = strlen(message);

	if(write(arg_st->socket_client,message,size) == -1)
	{
		close_file(1,arg_st->socket_client);
		free(arg_st->data_from_socket);
		free(arg_st);
		return (void*)err;
	}

	close_file(1,arg_st->socket_client);
	free(arg_st->data_from_socket);
	free(arg_st);
	return (void*)suc;
}

void pool_destroy(Thread_pool* pool)
{
        pthread_mutex_lock(&pool->lock);
        pool->stop = 1;
        pthread_cond_broadcast(&pool->notify);

        pthread_mutex_unlock(&pool->lock);
        
        for(int i = 0; i < THREAD_POOL; i++)
                pthread_join(pool->threads[i], NULL);
        
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->notify);
}
