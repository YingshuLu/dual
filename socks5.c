#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "socks5.h"

#define SS5_VERSION 5
#define SS5_NO_AUTH 0
#define SS5_CONNECT 1
#define SS5_IPV4 1
#define SS5_IPV6 4
#define SS5_SUCCEED 0

struct ss5_chello
{
    char version;
    char nmethods;
    char methods;
};

struct ss5_shello
{
    char version;
    char method;
};

struct ss5_conn
{
    char version;
    char cmd;
    char rsv;
    char atyp;
    uint32_t ipv4_addr;
    uint32_t ipv6_addr[4];
    uint16_t port;
};

int ss5_handshake(int sockfd, struct sockaddr_in* intended_addr) {
    char chello[3] = {0};
    chello[0] = SS5_VERSION;
    chello[1] = 1;
    chello[2] = SS5_NO_AUTH;

    int n = write(sockfd, chello, 3);
    if (n < 0) {
	return n;
    }

    char shello[2] = {0};
    n = read(sockfd, shello, 2);
    if (n < 0) {
	return n;
    }

    if (shello[0] != SS5_VERSION || shello[1] != SS5_NO_AUTH) {
	return SS5_HELLO_ERROR;
    }
  
    struct ss5_conn req;
    req.version = SS5_VERSION;
    req.cmd = SS5_CONNECT;
    req.rsv = 0;
    req.atyp = SS5_IPV4;
    req.ipv4_addr = intended_addr->sin_addr.s_addr;
    req.port = intended_addr->sin_port;

    char* req_buffer = (char*) malloc(10);
    req_buffer[0] = req.version;
    req_buffer[1] = req.cmd;
    req_buffer[2] = req.rsv;
    req_buffer[3] = req.atyp;
    memcpy(req_buffer+4, &req.ipv4_addr, 4);
    memcpy(req_buffer+8, &req.port, 2);

    n = write(sockfd, req_buffer, 10);
    if (n < 0) {
        free(req_buffer);
	return n;
    }

    char *reply_buffer = req_buffer;
    n = read(sockfd, reply_buffer, 10);
    if (n < 0) {
        free(req_buffer);
        return n;
    }
    
    char rep = reply_buffer[1];
    if (rep != SS5_SUCCEED) {
        free(req_buffer);
        return rep;
    }
     
    return SS5_OK;
}
