#include "domain.h"

int domain_char(char c) {
  return ('a' <= c && 'z' >= c) || ('A' <= c && 'Z' >= c) ||
         ('0' <= c && '9' >= c) || '-' == c || '.' == c;
}

int domain_verify(const char *domain, const size_t domain_len) {
  if (!domain || !domain_len || domain_len > 255) {
    return -1;
  }

  const char *ptr = domain, *end = domain + domain_len;
  int dot_count = 0;
  while (ptr < end) {
    if (!domain_char(*ptr)) {
      return -1;
    }
    if ('.' == *ptr) {
      dot_count += 1;
    }
    ptr += 1;
  }
  if (dot_count < 1) {
    return -1;
  }
  return 0;
} 

int domain_match(const char *host, const size_t host_len, const char *domain, const size_t domain_len) {
  if (host_len < domain_len) {
    return -1;
  }

  const char *host_start = host + host_len - domain_len;
  if (host_len == domain_len) {
    return strncasecmp(host, domain, domain_len) == 0 ? 0 : -1;
  }

  if (*(host_start - 1) == '.') {
    return strncasecmp(host_start, domain, domain_len) == 0 ? 1 : -1;
  }

  return -1;
}
