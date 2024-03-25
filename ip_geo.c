#include "ip_geo.h"


int jailed_ipaddr(struct sockaddr*ipaddr) {

MMDB_lookup_result_s result =
    MMDB_lookup_sockaddr(&mmdb, address->ai_addr, &mmdb_error);
MMDB_entry_data_s entry_data;
int status =
    MMDB_get_value(&result.entry, &entry_data,
                   "names", "en", NULL);
if (MMDB_SUCCESS != status) { ... }
if (entry_data.has_data) { ... }
}
