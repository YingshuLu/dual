#ifndef DUAL_IPGEO_H
#define DUAL_IPGEO_H

#include <arpa/inet.h>

int ipgeo_update();

int ipgeo_load(const char *filename);

int ipgeo_jailed(const struct sockaddr *ipaddr);

#endif
