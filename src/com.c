#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <sys/socket.h>
#include <errno.h>
#include <openssl/err.h> /* SSL errors */
#include <arpa/inet.h>
#include <netinet/in.h> /* for sockaddr_in */
#include <sys/un.h>
#include <unistd.h> /* for read(), close()*/
#include "str_op.h"
#include "com.h"

static const char cache_id[] = "Restaurant man Server";
/*
 * Global variable for the SSL context
 * */
SSL_CTX *ctx = NULL;
BIO* acceptor_bio = NULL;

int open_socket(int domain, int type)
{
	int fd = socket(domain,type,0);
	if(fd == -1)
	{
		perror("socket: ");
		return fd;
	}
	return fd;
}


unsigned char listen_set_up(int* fd_sock,int domain, int type, short port)
{
	if(domain == AF_INET)
	{
		struct sockaddr_in server_info = {0};
		server_info.sin_family = domain;
		server_info.sin_addr.s_addr = 0; /*to bind to each ip owned by the computer*/
		server_info.sin_port = htons(port); 
	
		*fd_sock = open_socket(domain,type);
		if(*fd_sock == -1)
		{
			fprintf(stderr,
					"open_socket(), failed, %s:%d.\n",
					__FILE__,__LINE__-3);
			return 0;
		}
	
		/*bind to the port*/
		if(bind(*fd_sock,(struct sockaddr*) &server_info,sizeof(server_info)) == -1)
		{
			perror("bind: ");
			fprintf(stderr,
					"bind() failed, %s:%d.\n",
					__FILE__,__LINE__-3);
			close(*fd_sock);
			return 0;
		}		

		if(listen(*fd_sock,0) == -1)
		{
			perror("listen: ");
			fprintf(stderr,"listen() failed, %s:%d.\n",
					__FILE__,__LINE__-3);
			close(*fd_sock);
			return 0;	
		}

		return 1;

	} else if(domain == AF_UNIX) {
		struct sockaddr_un intercom = {0};
		intercom.sun_family = domain;	
		strncpy(intercom.sun_path,SOCKET_NAME,sizeof(intercom.sun_path)-1);
		
		*fd_sock = open_socket(domain,type);
		if(!(*fd_sock == -1))
		{
			fprintf(stderr,
					"open_socket(), failed, %s:%d.\n",
					__FILE__,__LINE__-3);
			return 0;
		}

		/*set ptions for the socket*/
		int opt = 1;
		if(setsockopt(*fd_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)) == -1) { 
			perror("setsockopt: ");
			close(*fd_sock);
			return 0;
		}
		/*bind the soket for internal communication*/

		if(bind(*fd_sock,(const struct sockaddr*)&intercom,sizeof(intercom)) == -1)
		{
			perror("bind: ");
			fprintf(stderr,"bind() failed, %s:%d.\n",
					__FILE__,__LINE__-3);
			close(*fd_sock);
			return 0;
		}		

		if(listen(*fd_sock,0) == -1)
		{
			perror("listen: ");
			fprintf(stderr,
					"listen() failed, %s:%d.\n",
					__FILE__,__LINE__-3);
			close(*fd_sock);
			return 0;	
		}

		return 1;
	}

	return 1; /*UNREACHABLE*/
}

/*
 * for non blocking set up,
 * pass the paramter port with "null" value
 *
 * */
int start_SSL(SSL_CTX **ctx,char *port)
{
        long opts;

        *ctx = SSL_CTX_new(TLS_server_method());
        if(!(*ctx)) {
                fprintf(stderr,"failed to create SSL context");
                return -1;
        }


        if(!SSL_CTX_set_min_proto_version(*ctx,TLS1_2_VERSION)) {
                fprintf(stderr,"failed to set minimum TLS version\n");
                SSL_CTX_free(*ctx);
                return -1;
        }

        /*
        * setting the option for the SSL context
        *
        * for documentation on what this option are please see
        * openSSL documentaion at 
        * https://docs.openssl.org/master/man7/ossl-guide-tls-server-block/
        * or 
        * https://github.com/openssl/openssl/blob/master/demos/guide/tls-server-block.c 
        **/

        opts = SSL_OP_IGNORE_UNEXPECTED_EOF;
        opts |= SSL_OP_NO_RENEGOTIATION;
        opts |= SSL_OP_CIPHER_SERVER_PREFERENCE;

        /*apply the selction options */
        SSL_CTX_set_options(*ctx, opts);

        if(SSL_CTX_use_certificate_chain_file(*ctx,"chain.pem") <= 0 ) {
                fprintf(stderr,"error use certificate.\n");
                return -1;
        }

        if(SSL_CTX_use_PrivateKey_file(*ctx, "pkey.pem",SSL_FILETYPE_PEM) <= 0) {
                fprintf(stderr,"error use privatekey ");
                return -1;
        }

        SSL_CTX_set_session_id_context(*ctx,(void*)cache_id,sizeof(cache_id));
        SSL_CTX_set_session_cache_mode(*ctx,SSL_SESS_CACHE_SERVER);
        SSL_CTX_sess_set_cache_size(*ctx, 1024);
        SSL_CTX_set_timeout(*ctx,3600);
        SSL_CTX_set_verify(*ctx,SSL_VERIFY_NONE, NULL);
	
	if(strcmp(port,"null") != 0) {	
		acceptor_bio = BIO_new_accept(port);
		if(acceptor_bio == NULL) {
			SSL_CTX_free(*ctx);
		        fprintf(stderr,"creating listeining socket failed.\n");
			return -1;
		}	
		
		BIO_set_bind_mode(acceptor_bio,BIO_BIND_REUSEADDR);
		if(BIO_do_accept(acceptor_bio) < 0) {
			SSL_CTX_free(*ctx);
		        fprintf(stderr,"error creating socket.\n");
			return -1;
		}
	}

        return EXIT_SUCCESS;
}

/*
 * this fucntion is to be use in a webserver
 * like application.
 * */
int accept_connection(int *fd_sock, int *client_sock,char* request, int req_size, 
		SSL_CTX *ctx, SSL **ssl, int epoll_fd,int max_ev)
{
	struct sockaddr_in client_info = {0};
	socklen_t client_size = sizeof(client_info);

	*client_sock = accept4(*fd_sock,(struct sockaddr*)&client_info, &client_size,SOCK_NONBLOCK);
	if(*client_sock == -1)
	{
		return NO_CON;
	}

	if((*ssl = SSL_new(ctx)) == NULL) {
		fprintf(stderr,"error creating SSL handle for new connection");
		close(*client_sock);
		return SSL_HD_F;
	}

	if(!SSL_set_fd(*ssl,*client_sock)) {
		ERR_print_errors_fp(stderr);
		fprintf(stderr,"error setting socket to SSL context");
		close(*client_sock);
		SSL_free(*ssl);
		return SSL_SET_E;		
	}		
	/*try handshake with the client*/	
	int hs_res = 0;
	if((hs_res = SSL_accept(*ssl)) <= 0) {
		int err = SSL_get_error(*ssl,hs_res);
		if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
			/* 
			 * socket is not ready
			 * so we add the file descriptor to the epoll system
			 * and return;
			 * */
			struct epoll_event ev;
			ev.events = err == SSL_ERROR_WANT_READ ? EPOLLIN | EPOLLET : EPOLLOUT | EPOLLET;
			ev.data.fd = *client_sock;

			if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD, *client_sock, &ev) == -1 ) {
				fprintf(stderr,"failed to add fd to  epoll.\n");
				SSL_free(*ssl);
				close(*client_sock);
				return -1;
			}
			
			return HANDSHAKE;		
		}else {
			ERR_print_errors_fp(stderr);
			fprintf(stderr,"handshake failed.\n");
			SSL_free(*ssl);
			close(*client_sock);
			return -1;
		}
	}

	size_t bread = 0;
	int result = 0;
	if((result = SSL_read_ex(*ssl,request,req_size,&bread)) == 0) {
		int err = SSL_get_error(*ssl,result);
		if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
			/* 
			 * socket is not ready
			 * so we add the file descriptor to the epoll system
			 * and return;
			 * */
			struct epoll_event ev;
			ev.events = err == SSL_ERROR_WANT_READ ? EPOLLIN | EPOLLET : EPOLLOUT | EPOLLET;
			ev.data.fd = *client_sock;

			if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD, *client_sock, &ev) == -1 ) {
				fprintf(stderr,"failed to add fd to  epoll.\n");
				SSL_free(*ssl);
				close(*client_sock);
				return -1;
			}

			return SSL_READ_E; 
		}else {
			ERR_print_errors_fp(stderr);
			fprintf(stderr,"read failed.\n");
			SSL_free(*ssl);
			close(*client_sock);
			return -1;
		}

	}
	
	if(bread == req_size)
		request[bread - 1]= '\0';
	else 
		request[bread] = '\0';


	return bread;
}


int retry_SSL_handshake(SSL **ssl)
{
	return SSL_accept(*ssl);
}

int retry_SSL_read(SSL **ssl,char *request, int req_size)
{
	size_t bread = 0;
	int result = 0;
	if((result = SSL_read_ex(*ssl,request,req_size,&bread)) == 0) {
		int err = SSL_get_error(*ssl,result);
		if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {

			return SSL_READ_E; 
		}else {
			ERR_print_errors_fp(stderr);
			fprintf(stderr,"read failed.\n");
			return -1;
		}
	}

	return  (int)bread;

}

/*
 * retry_RDIO retry on read operation on a client socket
 *	the fucntions return -1 in case of failure.
 *	client sock gets close on failure and the epoll API 
 *	is taking care of, removing the file descriptor
 *	accordingly.
 * */
int retry_RDIO(int client_sock, char *instruction_buff, int buff_size, int epoll_fd)
{
	errno = 0;
	int err = -1;
	int instruction_size = read(client_sock,instruction_buff,buff_size);
	if(instruction_size <= 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			 return errno;
		} else if (errno > 0) {
			printf("%s() failed to read instruction,"\
					" or socket is closed, %s:%d.\n"
					,__func__,__FILE__,__LINE__-3);
			errno = 0;
			if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,
						client_sock,NULL) == -1) {
				if(errno == ENOENT) {
					close(client_sock);
					return -1;
				}
			}
			close(client_sock);
			return -1;
		}
	}

	if(instruction_size == buff_size) {
		printf("data too large!.\n");
		err = -1;
		goto send_error;
	}

	 /*trimming the string, eleminating all garbage from the network*/                                         
	 int index = 0;                                                                                            
	 if((index = find_last_char('}',instruction_buff)) == -1) {                                                
		printf("invalid data.\n");
		err = DT_INV;
		goto send_error;
	}                                                                                                         
                                                                                                                  
	if((index + 1) > instruction_size) {                                                                      
		printf("error in socket data.\n");                                                                
		err = DT_INV;
		goto send_error;
	} else if((index + 1) < instruction_size) {                                                               
		memset(&instruction_buff[index + 1],0,(instruction_size - (index + 1)));                          
	} else if ((index + 1) == instruction_size) {                                                             
            instruction_buff[instruction_size] = '\0';                                                        
	}

	/* reading the socket has completed,
	 * we deregister the fd*/
	if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,
			client_sock,NULL) == -1) {
		if(errno == ENOENT) {
			close(client_sock);
			return -1;
		}
	}

	return 0;

send_error:
	char* mes ="{\"status\":\"error\"}";
	if(write(client_sock,mes,strlen(mes)+1) == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			/*
			 * error during write operation in the
			 * reading process. 
			 * */
			struct epoll_event ev;
			ev.events = EPOLLOUT | EPOLLET;
			ev.data.fd = client_sock;
			errno = 0;
			if(epoll_ctl(epoll_fd,
					EPOLL_CTL_MOD,
					client_sock,&ev) == -1) {
				if(errno == ENOENT) {
					close(client_sock);
					return -1;
				} else if(errno == ENOMEM) {
					close(client_sock);
					return ENOMEM;
				}
				return -1;
			}
		return ER_WR;
		}
	}
	/*
	 * if the write op is succesfull we do not need the socket anymore
	 * so we deregister it from the epoll API 
	 * and we close it 
	 * */
	if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,
				client_sock,NULL) == -1) {
			if(errno == ENOENT) {
				close(client_sock);
				return -1;
			}
		close(client_sock);
		return -1;
	}

	close(client_sock);
	return err;                                                                                         
                 
}


/* this functions is 
 * used only after ER_WR already happend once
 * */
int write_err(int client_sock, int epoll_fd)
{
	char* mes ="{\"status\":\"error\"}";
	int bwrite = 0;
	if((bwrite = write(client_sock,mes,strlen(mes)+1)) == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			/*
			 * error during write operation
			 * we have already the fd in the epoll
			 * so there is no need to save it again
			 * */
			
		return ER_WR;
		}
	}
	/*
	 * if the write op is succesfull we do not need the socket anymore
	 * so we deregister it from the epoll API 
	 * and we close it 
	 * */
	if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,
				client_sock,NULL) == -1) {
			if(errno == ENOENT) {
				close(client_sock);
				return -1;
			}
		close(client_sock);
		return -1;
	}

	close(client_sock);
	return bwrite; 
}
/*
 * this fucntion is to be used 
 * in application that exchange data over a TCP socket
 * */
unsigned char accept_instructions(int* fd_sock,int* client_sock,
	       	char* instruction_buff, int buff_size, int epoll_fd)
{
	struct sockaddr_in client_info = {0};
	socklen_t client_size = sizeof(client_info);

	/*set errno to 0 */
	errno = 0;
	*client_sock = accept4(*fd_sock,(struct sockaddr*)&client_info, &client_size,SOCK_NONBLOCK);
	if(*client_sock == -1)
	{
		return NO_CON;
	}
	
	if(errno == EAGAIN || errno == EWOULDBLOCK) {
			 return NO_CON;
	} else if (errno > 0) {
		close(*client_sock);
		return -1;
	}

	/*
	 * here you have to pass the client_socket 
	 * to the SSL context created, to perform SSL 
	 * handshake and have secure comunication 
	 * */
	printf("socket connected %s:%d.\n",__FILE__,__LINE__);
	struct sockaddr_in addr = {0};
	/*convert the ip adress from human readable to network endian*/
	inet_pton(AF_INET, IP_ADR, &addr.sin_addr);

	 if(client_info.sin_addr.s_addr != addr.sin_addr.s_addr ) {
		fprintf(stderr,"client not allowed. connection dropped.\n");
		return CLI_NOT; 
	}
 
	printf("about to read the data%s:%d.\n",__FILE__,__LINE__);
	errno = 0;
	int instruction_size = read(*client_sock,instruction_buff,buff_size);
	if(instruction_size <= 0) {
		/*
		 * add the socket file descriptor 
		 * to the epoll system
		 * */
		printf("read the data failed, inside if %s:%d.\n",__FILE__,__LINE__);
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			 struct epoll_event ev;
			 ev.events = EPOLLIN | EPOLLET;
			 ev.data.fd = *client_sock;

			 if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,
						 *client_sock,&ev) == -1){
				close(*client_sock);
				return EPOLL_ADD_E;
			 }

			 return errno;
		} else if (errno > 0) {
			close(*client_sock);
			printf("%s() failed to read instruction,"\
					" or socket is closed, %s:%d.\n"
					,__func__,__FILE__,__LINE__-3);
			return -1;
		}
	}

	printf("about to check the data%s:%d.\n",__FILE__,__LINE__);
	if(instruction_size == buff_size) {
		printf("data too large!.\n");
		return DT_INV;
	}	

	printf("read %d bytes from buffer.\n",instruction_size);

	 /*trimming the string, eleminating all garbage from the network*/                                         
	 int index = 0;                                                                                            
	 if((index = find_last_char('}',instruction_buff)) == -1) {                                                
		printf("invalid data.\n");
		char* mes ="{\"status\":\"error\"}";
		write(*client_sock,mes,strlen(mes)+1);
		return DT_INV;                                                                                         
	}                                                                                                         
                                                                                                                  
	if((index + 1) > instruction_size) {                                                                      
            printf("error in socket data.\n");                                                                
            return DT_INV;                                                                                         
	} else if((index + 1) < instruction_size) {                                                               
		memset(&instruction_buff[index + 1],0,(instruction_size - (index + 1)));                          
	} else if ((index + 1) == instruction_size) {                                                             
            instruction_buff[instruction_size] = '\0';                                                        
	}

	printf("reading succesfull\n");
	return 1;
}
