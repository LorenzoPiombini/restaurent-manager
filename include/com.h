#ifndef _COM_H_
#define _COM_H_


#include <openssl/ssl.h>
#include <sys/epoll.h>

#define SOCKET_NAME "/tmp/usr_crt"

#define SSL_ctx_free(ctx) SSL_CTX_free(ctx)
#define SSL_close(ssl) SSL_free(ssl)

#define IP_ADR "71.187.166.234"

 /*
  * non blocking socket error during read, 
  * we still have to send an error message before closing
  * the connections
  * */
#define ER_WR 13
#define EPOLL_ADD_E 14	/*error suring EPOLL_CTL_ADD*/
#define CLI_NOT 15	/*CLIENT NOT ALLOWED*/
#define NO_CON 16	/* no connection */
#define DT_INV 17	/* invalid data */
#define SSL_HD_F 18	/*SSL handshake failed*/	
#define SSL_SET_E 19	/*set SSL handle failed*/
#define HANDSHAKE 20	/*SSL handshake failed but we can try again*/
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
int retry_RDIO(int *client_sock, char *instruction_buff, int buff_size, int epoll_fd);
int write_err(int *client_sock, int epoll_ed);
#endif
