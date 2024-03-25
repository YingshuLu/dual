#ifndef SOCKS5_PROTO
#define SOCKS5_PROTO

#include <arpa/inet.h>

#define SS5_HELLO_ERROR -2
#define SS5_OK 0

int ss5_handshake(int sockfd, struct sockaddr_in* intended_addr);

#endif
