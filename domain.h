#ifndef DUAL_DOMAIN_H
#define DUAL_DOMAIN_H

#include <stddef.h>

int domain_char(char c);

int domain_verify(const char *domain, const size_t domain_len);

// compare host with domain from end to start
// return 0 if host is the same as domain (exact match)
// return 1 if host is a subdomain of domain
// return -1 if host is not a subdomain of domain
int domain_match(const char *host, const size_t host_len, const char *domain, const size_t domain_len);

#endif
