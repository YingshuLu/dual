#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libcask/co_define.h"
#include "socks5.h"

#define TPROXY_BIND_PORT 1234
#define TUNNEL_BIND_PORT 1212
#define TUNNEL_BUFFER_SIZE 65536

struct sockaddr_in tunnel_addr;

struct proxy_info 
{
    int client_fd;
    int target_fd;
    struct sockaddr_in client_addr;
    struct sockaddr_in intended_addr;
};

struct tunnel_state {
    int closed;
    int ref_cnt;
};

struct tunnel_info
{
    int read_fd;
    int write_fd;
    struct tunnel_state* state;
};

void init()
{
  tunnel_addr.sin_family = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &tunnel_addr.sin_addr.s_addr);
  tunnel_addr.sin_port = htons(TUNNEL_BIND_PORT);
}

void crash(const char* msg)
{
  perror(msg);
  exit(1);
}

struct tunnel_state* new_state()
{
    struct tunnel_state *pstate = malloc(sizeof(struct tunnel_state));
    pstate->closed = 0;
    pstate->ref_cnt = 0;
    return pstate;
}

struct tunnel_state* refer_state(struct tunnel_state* pstate) 
{
    pstate->ref_cnt = pstate->ref_cnt + 1;
    return pstate;
}

void defer_state(struct tunnel_state* pstate)
{
    pstate->ref_cnt = pstate->ref_cnt - 1;
    if (pstate->ref_cnt == 0) {
	free(pstate);
    }
}

struct tunnel_info* new_tunnel_info(int rd, int wd, struct tunnel_state* ps) 
{
    struct tunnel_info* pinfo = malloc(sizeof(struct tunnel_info));
    pinfo->read_fd = rd;
    pinfo->write_fd = wd;
    pinfo->state = refer_state(ps);
    return pinfo;
} 

void do_tunnel(void *ip, void *op) {
    struct tunnel_info *pinfo = (struct tunnel_info*) ip;

    char *buffer = malloc(TUNNEL_BUFFER_SIZE);
    while(1)
    {
        int n = read(pinfo->read_fd, buffer, TUNNEL_BUFFER_SIZE);
	if (n <= 0) {
	    break;
	}

	DBG_LOG("recv fd[%d] with %d bytes", pinfo->read_fd, n);

	n = write(pinfo->write_fd, buffer, n);
	if (n <= 0) {
            break;
	}

	DBG_LOG("send fd[%d] with %d bytes", pinfo->write_fd, n);
    }
    free(buffer);

    close(pinfo->read_fd);
    if (pinfo->state->closed == 0) {
	pinfo->state->closed = 1;
	DBG_LOG("shutdown socket %d", pinfo->write_fd);
	shutdown(pinfo->write_fd, SHUT_RDWR);
    }

    defer_state(pinfo->state);
    free(pinfo);
}

int jailed_ipaddr(const struct sockaddr_in *addr) {
    return 0;
}

int connect_target(const struct sockaddr_in* target_addr) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int ret = connect(sockfd, (const struct sockaddr *)target_addr, addr_len);
    if(ret) {
	close(sockfd);
        return -1;
    }
    return sockfd;
}

void do_proxy(void* ip, void* op) 
{
    struct proxy_info* pinfo = (struct proxy_info*) ip;

    const struct sockaddr_in* target_addr = NULL;;
    int escape = 0;
    if (jailed_ipaddr(&pinfo->intended_addr)) {
	target_addr = &pinfo->intended_addr;
    } else {
	target_addr = &tunnel_addr;
	escape = 1;
    }

    if (escape) {
        DBG_LOG("%s:%d escaping through -> %s:%d",
           inet_ntoa(pinfo->intended_addr.sin_addr),
           ntohs(pinfo->intended_addr.sin_port),
           inet_ntoa(target_addr->sin_addr),
           ntohs(target_addr->sin_port));
    }

    pinfo->target_fd = connect_target(target_addr);
    if (pinfo->target_fd <= 0) {
	DBG_LOG("connect to target error %d", pinfo->target_fd);
	close(pinfo->client_fd);
	free(pinfo);
	return;
    }

    if (escape) {
	int res = ss5_handshake(pinfo->target_fd, &pinfo->intended_addr);
	DBG_LOG("socks5 handshake success? %d", res);
        if (res != 0) {
            close(pinfo->client_fd);
	    free(pinfo);
	    return;
        }
    }

    struct tunnel_state *pstate = new_state();

    struct tunnel_info *tinfo = new_tunnel_info(pinfo->client_fd, pinfo->target_fd, pstate);
    co_create(do_tunnel, tinfo, 0);

    struct tunnel_info *rinfo = new_tunnel_info(pinfo->target_fd, pinfo->client_fd, pstate);
    co_create(do_tunnel, rinfo, 0);
    
    free(pinfo);
}

void server(void* ip, void* op)
{
  const char* listen_addr = "127.0.0.1";
  int listener_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listener_fd < 0)
    crash("couldn't allocate socket");
  const int yes = 1;
  if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    crash("SO_REUSEADDR failed");
  if (setsockopt(listener_fd, SOL_IP, IP_TRANSPARENT, &yes, sizeof(yes)) < 0)
    crash("IP_TRANSPARENT failed. Are you root?");

  struct sockaddr_in bind_addr;
  bind_addr.sin_family = AF_INET;
  inet_pton(AF_INET, listen_addr, &bind_addr.sin_addr.s_addr);
  bind_addr.sin_port = htons(TPROXY_BIND_PORT);

  if (bind(listener_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0)
    crash("bind failed");

  if (listen(listener_fd, 10) < 0)
    crash("listen failed");
  DBG_LOG("now TPROXY-listening on %s:%d",
         listen_addr, TPROXY_BIND_PORT);
  DBG_LOG("but actually accepting any TCP SYN with dport 80, regardless of"\
         " dest IP, that hits the loopback interface!");
  while (1)
  {
    struct proxy_info* pinfo = malloc(sizeof(struct proxy_info));
    socklen_t client_addr_len = sizeof(pinfo->client_addr);
    socklen_t dest_addr_len = sizeof(pinfo->intended_addr);
    int client_fd = accept(listener_fd, (struct sockaddr*)&(pinfo->client_addr), &client_addr_len);

    DBG_LOG("accept: client_fd %d", client_fd);
    if (client_fd <= 0) {
	free(pinfo);
	break;
    }
    pinfo->client_fd = client_fd;

    getsockname(client_fd, (struct sockaddr*)&(pinfo->intended_addr),
                                             &dest_addr_len);

    DBG_LOG(">>> proxy: accepted socket from %s:%d",
           inet_ntoa(pinfo->client_addr.sin_addr),
           ntohs(pinfo->client_addr.sin_port));
    DBG_LOG("they think they're talking to %s:%d",
           inet_ntoa(pinfo->intended_addr.sin_addr),
           ntohs(pinfo->intended_addr.sin_port));

    co_create(do_proxy, pinfo, NULL);
  }

  close(listener_fd);
}

int main(int argc, char** argv)
{
  init();
  co_create(server, 0, 0);
  schedule();
  return 0;
}
