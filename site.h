#ifndef DUAL_SITE_H
#define DUAL_SITE_H

#include <stdint.h>

int site_init();

int site_load();

int site_jailed(const char *host, const size_t host_len);

int site_ad(const char *host, const size_t host_len);

#endif
