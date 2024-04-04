#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "ip_geo.h"
#include "libcask/co_define.h"
#include "socks5.h"

#define TPROXY_BIND_PORT 1234
#define TUNNEL_BIND_PORT 1212
#define TUNNEL_BUFFER_SIZE 65536
#define LOCALHOST "127.0.0.1"

struct sockaddr_in tunnel_addr;

struct proxy_info {
  int client_fd;
  int target_fd;
  struct sockaddr_in client_addr;
  struct sockaddr_in intended_addr;
};

struct tunnel_state {
  int closed;
  int ref_cnt;
};

struct tunnel_info {
  int read_fd;
  int write_fd;
  struct tunnel_state *state;
};

void init() {
  tunnel_addr.sin_family = AF_INET;
  inet_pton(AF_INET, LOCALHOST, &tunnel_addr.sin_addr.s_addr);
  tunnel_addr.sin_port = htons(TUNNEL_BIND_PORT);

  if (0 != ipgeo_load("./GeoLite2-Country.mmdb")) {
    DBG_LOG("ERROR: ipgeo failed to load MMDB");
    exit(1);
  }
}

void crash(const char *msg) {
  perror(msg);
  exit(1);
}

struct tunnel_state *new_state() {
  struct tunnel_state *pstate = malloc(sizeof(struct tunnel_state));
  pstate->closed = 0;
  pstate->ref_cnt = 0;
  return pstate;
}

struct tunnel_state *refer_state(struct tunnel_state *pstate) {
  pstate->ref_cnt += 1;
  return pstate;
}

void defer_state(struct tunnel_state *pstate) {
  pstate->ref_cnt -= 1;
  if (pstate->ref_cnt == 0) {
    free(pstate);
  }
}

struct tunnel_info *new_tunnel_info(int rd, int wd, struct tunnel_state *ps) {
  struct tunnel_info *pinfo = malloc(sizeof(struct tunnel_info));
  pinfo->read_fd = rd;
  pinfo->write_fd = wd;
  pinfo->state = refer_state(ps);
  return pinfo;
}

void free_tunnel_info(struct tunnel_info *pinfo) {
  defer_state(pinfo->state);
  free(pinfo);
}

void do_tunnel(void *ip, void *op) {
  struct tunnel_info *pinfo = (struct tunnel_info *)ip;

  int count = 0;
  char buffer[TUNNEL_BUFFER_SIZE];
  while (1) {
    int n = read(pinfo->read_fd, buffer, TUNNEL_BUFFER_SIZE);
    if (n <= 0) {
      break;
    }

    n = write(pinfo->write_fd, buffer, n);
    if (n <= 0) {
      break;
    }
    count += n;
  }

  DBG_LOG("tunnel [%d] -> [%d] with %d bytes", pinfo->read_fd, pinfo->write_fd,
          count);
  close(pinfo->read_fd);
  if (pinfo->state->closed == 0) {
    pinfo->state->closed = 1;
    DBG_LOG("shutdown socket %d", pinfo->write_fd);
    shutdown(pinfo->write_fd, SHUT_RDWR);
  }

  free_tunnel_info(pinfo);
}

int jailed_ipaddr(const struct sockaddr_in *addr) {
  int res = ipgeo_jailed((const struct sockaddr *)addr);
  if (res < 0) {
    ERR_LOG("ipgeo_jailed error");
    return 0;
  }
  return res;
}

int peek_http_host(const int client_fd, int is_https, char *buffer,
                   size_t buffer_len, const char **host, size_t *host_len) {
  *host = NULL;
  *host_len = 0;
  if (buffer == NULL) {
    return -1;
  }

  const char *proto = is_https ? "https" : "http";
  int res = -1;
  do {
    int nread = recv(client_fd, buffer, buffer_len, MSG_PEEK);
    if (nread < 0) {
      break;
    }

    // TODO: why can not comment this log???, it causes http parser error...
    INF_LOG("[%s] try to peek host", proto);
    res = is_https ? ssl_host(buffer, nread, host, host_len)
                       : http_host(buffer, nread, host, host_len);
    if (res == 1) {
      INF_LOG("[%s] tunnel %.*s", proto, *host_len, *host);
      res = 0;
    } else if (res == -2) {
      if (nread < buffer_len) {
        continue;
      }
    } else {
      res = -1;
    }
  } while (0);

  if (res != 0) {
     ERR_LOG("[%s] peek host error: %d", proto, res);
  }
  return res;
}

int connect_target(const struct sockaddr_in *target_addr) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  socklen_t addr_len = sizeof(struct sockaddr_in);
  int ret = connect(sockfd, (const struct sockaddr *)target_addr, addr_len);
  if (ret) {
    close(sockfd);
    return -1;
  }
  return sockfd;
}

void do_proxy(void *ip, void *op) {
  struct proxy_info *pinfo = (struct proxy_info *)ip;
  const struct sockaddr_in *target_addr = NULL;
  int escape = 0;

  if (jailed_ipaddr(&pinfo->intended_addr) != 0) {
    target_addr = &pinfo->intended_addr;
  } else {
    target_addr = &tunnel_addr;
    escape = 1;
  }

  int intended_port = ntohs(pinfo->intended_addr.sin_port);
  if (escape) {
    DBG_LOG("%s:%d escaping through -> %s:%d",
            inet_ntoa(pinfo->intended_addr.sin_addr), intended_port, LOCALHOST,
            TUNNEL_BIND_PORT);
  }

  pinfo->target_fd = connect_target(target_addr);
  if (pinfo->target_fd <= 0) {
    ERR_LOG("connect to target %s:%d error %d",
            inet_ntoa(target_addr->sin_addr), ntohs(target_addr->sin_port),
            pinfo->target_fd);
    close(pinfo->client_fd);
    free(pinfo);
    return;
  }

  if (!http_port(intended_port)) {
    // switch socks5 if need
    if (escape) {
      int res = ss5_handshake(pinfo->target_fd, &pinfo->intended_addr);
      DBG_LOG("socks5 handshake success? %s", res == 0 ? "yes" : "no");
      if (res != 0) {
        close(pinfo->target_fd);
        close(pinfo->client_fd);
        free(pinfo);
        return;
      }
    }
  } else {
    const char *intended_host = NULL;
    size_t host_len = 0;
    char buffer[TUNNEL_BUFFER_SIZE];

    int is_https = 443 == intended_port;
    int peeked =
        0 == peek_http_host(pinfo->client_fd, is_https, buffer,
                            TUNNEL_BUFFER_SIZE, &intended_host, &host_len);

    // switch socks5 if need
    if (escape) {
      int res = peeked ? ss5_handshake_hostname(pinfo->target_fd, intended_host,
                                                host_len, intended_port)
                       : ss5_handshake(pinfo->target_fd, &pinfo->intended_addr);

      DBG_LOG("socks5 handshake with %s success? %s", peeked ? "host" : "ip",
              res == 0 ? "yes" : "no");
      if (res != 0) {
        close(pinfo->target_fd);
        close(pinfo->client_fd);
        free(pinfo);
        return;
      }
    }
  }

  // tunnel established
  struct tunnel_state *pstate = new_state();

  struct tunnel_info *tinfo =
      new_tunnel_info(pinfo->client_fd, pinfo->target_fd, pstate);
  co_create(do_tunnel, tinfo, 0);

  struct tunnel_info *rinfo =
      new_tunnel_info(pinfo->target_fd, pinfo->client_fd, pstate);
  co_create(do_tunnel, rinfo, 0);

  free(pinfo);
}

void server(void *ip, void *op) {
  const char *listen_addr = LOCALHOST;

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

  if (bind(listener_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
    crash("bind failed");

  if (listen(listener_fd, 10) < 0)
    crash("listen failed");

  INF_LOG("TPROXY listening on %s:%d", listen_addr, TPROXY_BIND_PORT);

  while (1) {
    struct proxy_info *pinfo = calloc(1, sizeof(struct proxy_info));
    socklen_t client_addr_len = sizeof(pinfo->client_addr);
    socklen_t dest_addr_len = sizeof(pinfo->intended_addr);

    int client_fd =
        accept(listener_fd, (struct sockaddr *)&(pinfo->client_addr),
               &client_addr_len);
    if (client_fd <= 0) {
      free(pinfo);
      break;
    }

    pinfo->client_fd = client_fd;
    getsockname(client_fd, (struct sockaddr *)&(pinfo->intended_addr),
                &dest_addr_len);

    DBG_LOG("tunnel src from %s:%d",
            inet_ntoa(pinfo->client_addr.sin_addr),
            ntohs(pinfo->client_addr.sin_port));

    INF_LOG("tunnel for dest %s:%d",
            inet_ntoa(pinfo->intended_addr.sin_addr),
            ntohs(pinfo->intended_addr.sin_port));

    co_create(do_proxy, pinfo, NULL);
  }

  close(listener_fd);
}

int main(int argc, char **argv) {
  init();
  co_create(server, 0, 0);
  schedule();
  return 0;
}
