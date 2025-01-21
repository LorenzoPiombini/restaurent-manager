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
#include <signal.h>
#include "file.h"
#include "parse.h"
#include "str_op.h"
#include "common.h"
#include "lock.h"
#include "date.h"
#include "restaurant_t.h"
#include "db_instruction.h"
#include "timecard.h"
#include "json.h"
#include "multi_th.h"
#include "queue.h"
#include "com.h" /*for listen_set_up()*/
#include "debug.h"


void handle_sig_SIGSEGV(int sig); 

int main(void)
{
		

	/*this should be the real main program*/
	int send_err = 0;
        int smo = 0;
        int buff_size = 1000;
        char instruction[buff_size];
	struct Th_args* arg_st = NULL;

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
		
        
	void (*sig_handle)(int);
	sig_handle= signal(SIGSEGV,handle_sig_SIGSEGV);

	if(sig_handle == SIG_ERR) 
		goto handle_crash;

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
							instruction,buff_size,epoll_fd)) == -1) {
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

				if(res == ER_WR) {
					send_err = 1;
					continue;
				}

				arg_st = calloc(1,sizeof(struct Th_args));
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

			} else if(events[i].events == EPOLLIN){
				/*
				 * the file descriptor is a 
				 * client so we have to retry
				 * reading from the socket 
				 * */
				int ret = retry_RDIO(events[i].data.fd, 
						instruction,buff_size,
						epoll_fd);

				switch(ret) {
				case 0:
				{
					arg_st = calloc(1,sizeof(struct Th_args));
					if(!arg_st) {
						__er_calloc(F,L-3);
						goto handle_crash;
					}
	
					arg_st->socket_client = events[i].data.fd;
					arg_st->data_from_socket = strdup(instruction);
					if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,
								events[i].data.fd,
								NULL) == -1) {
					/*handle this */	
					}
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

					break;
				}
				case EAGAIN:
				case ENOMEM:
				case DT_INV:
					continue;
				case ER_WR:
					send_err = 1;
					break;		
				default:
					break;
				}
				
				continue;
			} else if(events[i].events == EPOLLOUT) {
				size_t written = 0;
				if(send_err) {
					 written = write_err(events[i].data.fd,
							 epoll_fd);
					 if(written == ER_WR)
						 continue;

					send_err = 0;
				}
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

        if(ctx)
                SSL_ctx_free(ctx);


        return EXIT_FAILURE;
}
		/*---- main program end*/
	
void handle_sig_SIGSEGV(int sig)
{
	free_memory_object(SH_ILOCK);
	REG_FREE
        SSL_ctx_free(ctx);
	exit(EXIT_FAILURE);
}	
