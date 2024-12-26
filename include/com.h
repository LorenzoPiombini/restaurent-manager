#ifndef _COM_H_
#define _COM_H_


#include <openssl/ssl.h>

#define SOCKET_NAME "/tmp/usr_crt"

#define SSL_free(ctx) SSL_CTX_free(ctx)

#define IP_ADR "71.187.166.234"

#define CLI_NOT 15
#define NO_CON 16

extern SSL_CTX *ctx;

int open_socket(int domain,int type);
unsigned char listen_set_up(int* fd_sock, int domain, int type, short port);
unsigned char accept_instructions(int* fd_sock,int* client_sock, char* instruction_buff, int buff_size);
int start_SSL(SSL_CTX **ctx);

#endif
