
#include <maxminddb.h>

int ipgeo_open(const char* file, MMDB_s* mmdb);

int ipgeo_jailed_ipaddr(MMDB_s* mmdb, const char* ipaddr);

int ipgeo_close(MMDB_s* mmdb);
