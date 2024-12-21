#ifndef _COM_H_
#define _COM_H_

#define SOCKET_NAME "/tmp/usr_crt"

int open_socket(int domain,int type);
unsigned char listen_set_up(int* fd_sock, int domain, int type, short port);
unsigned char accept_instructions(int* fd_sock,int* client_sock, char* instruction_buff, int buff_size);

#endif
