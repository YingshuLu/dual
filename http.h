#ifndef DUAL_HTTP_H
#define DUAL_HTTP_H

#include <stdint.h>

int http_port(const uint16_t port);

int http_host(const char *buffer, const size_t buffer_len, const char **host,
              size_t *host_len);

int ssl_host(const char *bytes, size_t bytes_len, const char **host,
             size_t *len);

int http_get(const char *url, const char **body, size_t *body_len);

#endif
