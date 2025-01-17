#ifndef _COM_H_
#define _COM_H_


#include <openssl/ssl.h>
#include <sys/epoll.h>

#define SOCKET_NAME "/tmp/usr_crt"

#define SSL_ctx_free(ctx) SSL_CTX_free(ctx)
#define SSL_close(ssl) SSL_free(ssl)

#define IP_ADR "71.187.166.234"

#define EPOLL_ADD_E 14
#define CLI_NOT 15
#define NO_CON 16
#define DT_INV 17
#define SSL_HD_F 18
#define SSL_SET_E 19
#define HANDSHAKE 20
#define SSL_READ_E 21
#define SSL_WRITE_E 22

extern SSL_CTX *ctx;
extern BIO *acceptor_bio;



/*struct to save connection info */
struct con_i
{
	SSL *ssl_handle;
	int err;
	int client_socket;
	char *key;
	char *buffer; /*buffer to repeat the write() on*/
	long buffer_size; /*size of the buffer*/
};


int con_set_up(struct con_i ***vector);
int open_socket(int domain,int type);
unsigned char listen_set_up(int* fd_sock, int domain, int type, short port);
unsigned char accept_instructions(int* fd_sock,int* client_sock,
	       	char* instruction_buff, int buff_size, int epoll_fd);
int start_SSL(SSL_CTX **ctx, char *port);
int accept_connection(int *fd_sock, int *client_sock,char* request, int req_size, 
		SSL_CTX *ctx, SSL **ssl, int epoll_fd,int max_ev);
int retry_SSL_handshake(SSL **ssl);
int retry_SSL_read(SSL **ssl,char *request, int req_size);

#endif
