
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include "libcask/co_await.h"
#include "libcask/co_define.h"
#include "libcask/sys_helper.h"
#include "libcask/inner_fd.h"

#include "config.h"
#include "http.h"
#include "ip_geo.h"
#include "record.h"
#include "refer.h"
#include "site.h"
#include "socks5.h"

#define TUNNEL_BUFFER_SIZE 65536
#define LOCALHOST "127.0.0.1"

struct sockaddr_in tunnel_addr;

struct proxy_info {
  int client_fd;
  int target_fd;
  struct sockaddr_in client_addr;
  struct sockaddr_in intended_addr;
  struct record *record;
};

struct tunnel_state {
  struct ref_t ref;
  int closed;
};

struct tunnel_info {
  int read_fd;
  int write_fd;
  struct record *record;
  struct tunnel_state *state;
};

void set_ulimit() {
  struct rlimit limit;
  int res = 0;
  res = getrlimit(RLIMIT_NOFILE, &limit);
  if (res != 0) {
    ERR_LOG("getrlimit failure: %d", res);
    exit(EXIT_FAILURE);
  }

  limit.rlim_cur = 65536;
  limit.rlim_max = 131072;

  res = setrlimit(RLIMIT_NOFILE, &limit);
  if (res != 0) {
    ERR_LOG("setrlimit faile");
    exit(EXIT_FAILURE);
  }
}

void init() {
  set_ulimit();

  int ret = parse_configuration("./dual.json");
  if (ret != 0) {
    ERR_LOG("ERROR: parse configuration error %d", ret);
  }

  tunnel_addr.sin_family = AF_INET;
  inet_pton(AF_INET, LOCALHOST, &tunnel_addr.sin_addr.s_addr);
  tunnel_addr.sin_port = htons(config()->tunnel_port);

  if (0 != ipgeo_load("./GeoLite2-Country.mmdb")) {
    INF_LOG("ERROR: ipgeo failed to load MMDB");
    exit(1);
  }

  int rc = record_init();
  if (0 != rc) {
    INF_LOG("ERROR: record init failed");
  }
}

void crash(const char *msg) {
  perror(msg);
  exit(1);
}

struct proxy_info *new_proxy_info() {
  return calloc(1, sizeof(struct proxy_info));
}

void free_proxy_info(struct proxy_info *pinfo) {
  defer_record(pinfo->record);
  free(pinfo);
}

struct tunnel_state *new_state() {
  struct tunnel_state *pstate = calloc(1, sizeof(struct tunnel_state));
  return pstate;
}

struct tunnel_state *refer_state(struct tunnel_state *pstate) {
  refer(pstate);
  return pstate;
}

void defer_state(struct tunnel_state *pstate) { defer(pstate); }

struct tunnel_info *new_tunnel_info(int rd, int wd, struct tunnel_state *ps) {
  struct tunnel_info *pinfo = calloc(1, sizeof(struct tunnel_info));
  pinfo->read_fd = rd;
  pinfo->write_fd = wd;
  pinfo->state = refer_state(ps);
  return pinfo;
}

void free_tunnel_info(struct tunnel_info *pinfo) {
  defer_state(pinfo->state);
  defer_record(pinfo->record);
  free(pinfo);
}

int dup_socket(int fd) {
  inner_fd* ifd = get_inner_fd(fd);
  if (!ifd) return -1;
  int nfd = dup(fd);
  inner_fd* infd = new_inner_fd(nfd);
  infd->flags = ifd->flags;
  infd->user_flags = ifd->user_flags;
  infd->timeout = ifd->timeout;
  return nfd;
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

  DBG_LOG("tunnel [%d] -> [%d] with %d bytes", 
          pinfo->read_fd, pinfo->write_fd, count);
  close(pinfo->read_fd);
  if (pinfo->state->closed == 0) {
    pinfo->state->closed = 1;
    shutdown(pinfo->write_fd, SHUT_RDWR);
  }
  close(pinfo->write_fd);

  if (pinfo->record) {
    pinfo->record->bytes += count;
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
  set_inner_fd_timeout(sockfd, config()->tunnel_timeout);
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
  struct record *record = pinfo->record;

  // check dst ipaddr
  int ip_jailed = jailed_ipaddr(&pinfo->intended_addr);
  record->ip_jailed = ip_jailed;
  INF_LOG("ip %s jailed: %d", inet_ntoa(pinfo->intended_addr.sin_addr),
          ip_jailed);

  // check site
  int host_jailed = 0;
  int site_peeked = 0;
  int enforce = 0;
  int intended_port = ntohs(pinfo->intended_addr.sin_port);
  if (http_port(intended_port)) {
    const char *intended_host = NULL;
    size_t host_len = 0;
    char buffer[TUNNEL_BUFFER_SIZE];

    int is_https = 443 == intended_port;
    site_peeked =
        0 == peek_http_host(pinfo->client_fd, is_https, buffer,
                            TUNNEL_BUFFER_SIZE, &intended_host, &host_len);

    if (site_peeked) {
      host_jailed = site_jailed(intended_host, host_len);
      INF_LOG("site %.*s jailed: %d", host_len, intended_host, host_jailed);
      record->site_jailed = host_jailed;
      memcpy(&(record->host[0]), intended_host, host_len);
    }
  }

  // policy to decide if tunneling or not
  int escape = 0;
  const struct sockaddr_in *target_addr = NULL;
  if (1 == host_jailed || 1 == ip_jailed) {
    target_addr = &pinfo->intended_addr;
  } else {
    target_addr = &tunnel_addr;
    escape = 1;
  }
  record->target = *target_addr;

  // connect with target
  pinfo->target_fd = connect_target(target_addr);
  if (pinfo->target_fd <= 0) {
    ERR_LOG("connect to target %s:%d error %d",
            inet_ntoa(target_addr->sin_addr), ntohs(target_addr->sin_port),
            pinfo->target_fd);
    close(pinfo->client_fd);
    free_proxy_info(pinfo);
    return;
  }

  const char *dest_addr =
      site_peeked ? record->host : inet_ntoa(pinfo->intended_addr.sin_addr);

  INF_LOG("dst %s tunnel? %s", dest_addr, escape ? "yes" : "no");

  // switch socks5 if need
  if (escape) {
    int res = site_peeked
                  ? ss5_handshake_hostname(pinfo->target_fd, record->host,
                                           strlen(record->host), intended_port)
                  : ss5_handshake(pinfo->target_fd, &pinfo->intended_addr);

    DBG_LOG("socks5 handshake with %s success? %s", site_peeked ? "host" : "ip",
            res == 0 ? "yes" : "no");
    if (res != 0) {
      ERR_LOG("socks5 handshake with %s:%d error",
              inet_ntoa(target_addr->sin_addr), ntohs(target_addr->sin_port));
      close(pinfo->target_fd);
      close(pinfo->client_fd);
      free_proxy_info(pinfo);
      return;
    }
  }

  record->success = 1;

  // tunnel established
  struct tunnel_state *pstate = new_state();

  struct tunnel_info *tinfo =
      new_tunnel_info(pinfo->client_fd, pinfo->target_fd, pstate);
  tinfo->record = refer_record(record);
  co_create(do_tunnel, tinfo, 0);

  struct tunnel_info *rinfo =
      new_tunnel_info(dup_socket(pinfo->target_fd), dup_socket(pinfo->client_fd), pstate);
  rinfo->record = refer_record(record);
  co_create(do_tunnel, rinfo, 0);

  free_proxy_info(pinfo);
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
  bind_addr.sin_port = htons(config()->listen_port);

  if (bind(listener_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
    crash("bind failed");

  if (listen(listener_fd, 10) < 0)
    crash("listen failed");

  INF_LOG("TPROXY listening on %s:%d", listen_addr, config()->listen_port);

  while (1) {
    struct proxy_info *pinfo = new_proxy_info();
    socklen_t client_addr_len = sizeof(pinfo->client_addr);
    socklen_t dest_addr_len = sizeof(pinfo->intended_addr);

    int client_fd =
        accept(listener_fd, (struct sockaddr *)&(pinfo->client_addr),
               &client_addr_len);
    if (client_fd <= 0) {
      free_proxy_info(pinfo);
      break;
    }

    set_inner_fd_timeout(client_fd, config()->tunnel_timeout);
    pinfo->client_fd = client_fd;
    getsockname(client_fd, (struct sockaddr *)&(pinfo->intended_addr),
                &dest_addr_len);

    DBG_LOG("tunnel src from %s:%d", inet_ntoa(pinfo->client_addr.sin_addr),
            ntohs(pinfo->client_addr.sin_port));

    struct record *record = new_record();
    time(&record->start_unixtime);
    record->from = pinfo->client_addr;
    record->to = pinfo->intended_addr;
    pinfo->record = refer_record(record);

    co_create(do_proxy, pinfo, NULL);
  }

  close(listener_fd);
}

int site_reload(void *ip) { return site_load(); }
int ipgeo_reload(void *ip) { return ipgeo_update(); }

void site_branch(void *ip, void *op) {
  do {
    int rc = co_await(site_reload, ip);
    if (rc != 0) {
      ERR_LOG("site reload error: %d", rc);
    } else {
      INF_LOG("site reloaded!");
    }

    rc = co_await(ipgeo_reload, ip);
    if (rc != 0) {
      ERR_LOG("ipgeo reload error: %d", rc);
    } else {
      INF_LOG("ipgeo reloaded!");
    }
    co_sleep(config()->database_rotation);
  } while (1);
}

int main(int argc, char **argv) {
  init();
  co_create(site_branch, 0, 0);
  co_create(server, 0, 0);
  schedule();
  return 0;
}
