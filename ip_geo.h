#ifndef IPGEO_LOOKUP_H
#define IPGEO_LOOKUP_H

#include <arpa/inet.h>

int ipgeo_load(const char *filename);

int ipgeo_jailed(const struct sockaddr *ipaddr);

#endif
