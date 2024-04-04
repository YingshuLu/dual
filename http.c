#include <arpa/inet.h>
#include <ctype.h>

#include "picohttpparser.h"

const char *HOST_NAME = "host";

int http_port(const uint16_t port) {
  if (port == 80 || port == 443) {
    return 1;
  }
  return 0;
}

int http_host(const char *buffer, size_t buffer_len, const char **host,
              size_t *host_len) {
  const char *method, *path;
  int minor_version;
  struct phr_header headers[100];
  size_t method_len, path_len, num_headers;

  int pret = phr_parse_request(buffer, buffer_len, &method, &method_len, &path,
                               &path_len, &minor_version, headers, &num_headers,
                               buffer_len);

  if (pret < 0) {
    return pret;
  }

  for (int i = 0; i != num_headers; ++i) {
    if (headers[i].name_len != 4) {
      continue;
    }

    int matched = 1;
    for (int j = 0; j < 4; ++j) {
      if (tolower(headers[i].name[j]) != HOST_NAME[j]) {
        matched = 0;
        break;
      }
    }

    if (matched) {
      *host = headers[i].value;
      *host_len = headers[i].value_len;
      return 1;
    }
  }

  return 0;
}

// https://stackoverflow.com/questions/17832592/extract-server-name-indication-sni-from-tls-client-hello
int ssl_host(const char *bytes, size_t bytes_len, const char **host,
             size_t *len) {
  // need more data
  if (bytes_len < 5 ||
      (5 + ntohs(*(unsigned short *)(bytes + 3))) > bytes_len) {
    return -2;
  }

  const char *curr;
  const char sidlen = bytes[43];
  curr = bytes + 1 + 43 + sidlen;
  unsigned short cslen = ntohs(*(unsigned short *)curr);
  curr += 2 + cslen;
  const char cmplen = *curr;
  curr += 1 + cmplen;
  const char *maxchar = curr + 2 + ntohs(*(unsigned short *)curr);
  curr += 2;
  unsigned short ext_type = 1;
  unsigned short ext_len;
  while (curr < maxchar && ext_type != 0) {
    ext_type = ntohs(*(const unsigned short *)curr);
    curr += 2;
    ext_len = ntohs(*(const unsigned short *)curr);
    curr += 2;
    if (ext_type == 0) {
      curr += 3;
      unsigned short namelen = ntohs(*(unsigned short *)curr);
      curr += 2;
      *len = namelen;
      *host = curr;
      return 1;
    } else
      curr += ext_len;
  }

  return 0;
}
