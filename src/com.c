#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <openssl/err.h> /* SSL errors */
#include <arpa/inet.h>
#include <netinet/in.h> /* for sockaddr_in */
#include <sys/un.h>
#include <unistd.h> /* for read()*/
#include "str_op.h"
#include "com.h"
#include "file.h" /* for close_file() */
#include "debug.h"

static const char cache_id[] = "Restaurant man Server";
SSL_CTX *ctx = NULL;
SSL *ssl = NULL;
static int set_non_blocking(int fd);


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
			printf("open_socket(), failed, %s:%d.\n",F,L-3);
			return 0;
		}
	
		/*bind to the port*/
		if(bind(*fd_sock,(struct sockaddr*) &server_info,sizeof(server_info)) == -1)
		{
			perror("bind: ");
			printf("bind() failed, %s:%d.\n",F,L-3);
			close_file(1,*fd_sock);
			return 0;
		}		

		if(listen(*fd_sock,0) == -1)
		{
			perror("listen: ");
			printf("listen() failed, %s:%d.\n",F,L-3);
			close_file(1,fd_sock);
			return 0;	
		}

		return 1;
	}

	else if(domain == AF_UNIX)
	{
		struct sockaddr_un intercom = {0};
		intercom.sun_family = domain;	
		strncpy(intercom.sun_path,SOCKET_NAME,sizeof(intercom.sun_path)-1);
		
		*fd_sock = open_socket(domain,type);
		if(!(*fd_sock == -1))
		{
			printf("open_socket(), failed, %s:%d.\n",F,L-3);
			return 0;
		}

		/*bind the soket for internal communication*/

		if(bind(*fd_sock,(const struct sockaddr*)&intercom,sizeof(intercom)) == -1)
		{
			perror("bind: ");
			printf("bind() failed, %s:%d.\n",F,L-3);
			close_file(1,*fd_sock);
			return 0;
		}		

		if(listen(*fd_sock,0) == -1)
		{
			perror("listen: ");
			printf("listen() failed, %s:%d.\n",F,L-3);
			close_file(1,fd_sock);
			return 0;	
		}

		return 1;
	}

	return 1; /*UNREACHABLE*/
}


int start_SSL(SSL_CTX **ctx)
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

        return EXIT_SUCCESS;
}

/*
 * this fucntion is to be use in a webserver
 * like application.
 * */
unsigned char accept_connection(int *fd_sock, int *client_sock,char* request, int req_size, 
		SSL_CTX *ctx, SSL *ssl)
{
	struct sockaddr_in client_info = {0};
	socklen_t client_size = sizeof(client_info);

	*client_sock = accept4(*fd_sock,(struct sockaddr*)&client_info, &client_size,SOCK_NONBLOCK);
	if(*client_sock == -1)
	{
		return NO_CON;
	}

	if((ssl = SSL_new(ctx)) == NULL) {
		fprintf(stderr,"error creating SSL handle for new connection");
		close(*clien_sock);
		return SSL_HD_F;
	}

	if(!SSL_set_fd(ssl,*clien_sock)) {
		ERR_print_errors_fp(stderr);
		fprintf(stderr,"error setting socket to SSL context");
		close(*clien_sock);
		SSL_free(ssl);
		return SSL_SET_E;		
	}		

	/*try handshake with the client*/	
	int hs_res = 0;
	if((hs_res = SSL_accept(ssl)) <= 0) {
		int err = SSL_get_error(ssl,0);
		if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
			/* 
			 * socket is not ready
			 * so we add the file descriptor to the epoll system
			 * and return;
			 *  
			 * */

			
		} else if(err == SSL_ERROR_WANT_WRITE) {
			fprintf(stderr,"socket needs to be writable.\n");
			return SSL_ERROR_WANT_WRITE;	
		}else {
			fprintf(stderr,"read failed.\n");
			SSL_free(ssl);
			close(*client_sock);
			return -1;
		}

		ERR_print_errors_fp(stderr);
		fprintf(stderr,"error setting socket to SSL context");
		close(*clien_sock);
		SSL_free(ssl);
		return HANDSHAKE;		
	}

	/*handshake succesfull so we read the data*/
	size_t bread = 0;
	int result = 0;
	if((result = SSL_read_ex(ssl,request,req_size,&bread)) == 0) {
	}
	
	if(bread == req_size)
		request[bread - 1]= '\0';
	else 
		request[bread] = '\0';


	return bread;
}


/*
 * this fucntion is to be used 
 * in application that exchange data over a TCP socket
 * */
unsigned char accept_instructions(int* fd_sock,int* client_sock, char* instruction_buff, int buff_size)
{
	struct sockaddr_in client_info = {0};
	socklen_t client_size = sizeof(client_info);

	*client_sock = accept4(*fd_sock,(struct sockaddr*)&client_info, &client_size,SOCK_NONBLOCK);
	if(*client_sock == -1)
	{
		return NO_CON;
	}
	
	/*
	 * here you have to pass the client_socket 
	 * to the SSL context created, to perform SSL 
	 * handshake and have secure comunication 
	 * */

    struct sockaddr_in addr = {0};
    /*convert the ip adress from human readable to network endian*/
    inet_pton(AF_INET, IP_ADR, &addr.sin_addr);

    if(client_info.sin_addr.s_addr != addr.sin_addr.s_addr ) {
        fprintf(stderr,"client not allowed. connection dropped.\n");
        return CLI_NOT; 
    }

	int instruction_size = read(*client_sock,instruction_buff,buff_size);
	if(instruction_size <= 0)
	{
		printf("%s() failed to read instruction, or socket is closed, %s:%d.\n",__func__,F,L-3);
		return 0;
	}

	if(instruction_size > buff_size)
	{
		printf("data to large!.\n");
		return 0;
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
            return 0;                                                                                         
    } else if((index + 1) < instruction_size) {                                                               
            memset(&instruction_buff[index + 1],0,(instruction_size - (index + 1)));                          
    } else if ((index + 1) == instruction_size) {                                                             
            instruction_buff[instruction_size] = '\0';                                                        
    }

	return 1;
}


static int set_non_blocking(int fd)
{
    int flags = fcntl(fd,F_GETFL,0);
    
    if(flags == -1) {
        fprintf(stderr,"can't get file flags.\n");
        return flags;
    }

    flags |= O_NONBLOCK;

    if(fcntl(fd,F_GETFL,0) == -1) {
        fprintf(stderr,"change file flag failed.\n");
        return -1;
    }

    return EXIT_SUCCESS;
}
