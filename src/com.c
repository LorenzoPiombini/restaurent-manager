#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h> /* for sockaddr_in */
#include <sys/un.h>
#include <openssl/ssl.h>
#include <unistd.h> /* for read()*/
#include "debug.h"
#include "str_op.h"
#include "com.h"
#include "file.h" /* for close_file() */

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

unsigned char accept_instructions(int* fd_sock,int* client_sock, char* instruction_buff, int buff_size)
{
	struct sockaddr_in client_info = {0};
	socklen_t client_size = sizeof(client_info);

	*client_sock = accept(*fd_sock,(struct sockaddr*)&client_info, &client_size);
	if(*client_sock == -1)
	{
		perror("accept: ");
		printf("accept() failed, %s:%d.\n",F,L-2);
		return 0;
	}
	/*
     *TODO: start an SSL section here 
     * */

    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if(!ctx) {
        fprintf(stderr,"failed to create SSL context");
    }

    SSL_CTX_free(ctx);
    struct sockaddr_in addr = {0};
    /*convert the ip adress from human readable to network endian*/
    inet_pton(AF_INET, IP_ADR, &addr.sin_addr);

    if(client_info.sin_addr.s_addr != addr.sin_addr.s_addr ) {
        fprintf(stderr,"client not allowed. connection dropped.\n");
        return 1; 
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
