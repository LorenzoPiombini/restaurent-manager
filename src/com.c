#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
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
                SSL_CTX_free(*ctx);
                return -1;
        }

        if(SSL_CTX_use_PrivateKey_file(*ctx, "pkey.pem",SSL_FILETYPE_PEM) <= 0) {
                fprintf(stderr,"error use privatekey ");
                SSL_CTX_free(*ctx);
                return -1;
        }

        SSL_CTX_set_session_id_context(*ctx,(void*)cache_id,sizeof(cache_id));
        SSL_CTX_set_session_cache_mode(*ctx,SSL_SESS_CACHE_SERVER);
        SSL_CTX_sess_set_cache_size(*ctx, 1024);
        SSL_CTX_set_timeout(*ctx,3600);
        SSL_CTX_set_verify(*ctx,SSL_VERIFY_NONE, NULL);

        return EXIT_SUCCESS;

}

unsigned char accept_instructions(int* fd_sock,int* client_sock, char* instruction_buff, int buff_size)
{
	struct sockaddr_in client_info = {0};
	socklen_t client_size = sizeof(client_info);

	*client_sock = accept(*fd_sock,(struct sockaddr*)&client_info, &client_size);
	if(*client_sock == -1)
	{
        return NO_CON;
	}

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
        return 0;                                                                                         
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
