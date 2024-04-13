#include <arpa/inet.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "maxmind/maxminddb.h"

#include "http.h"

static const char *ipgeo_database_url =
    "https://cdn.jsdelivr.net/gh/alecthw/mmdb_china_ip_list@release/"
    "Country.mmdb";

volatile MMDB_s *g_mmdb = NULL;

static int file_rotate = 0;

int ipgeo_close(MMDB_s *pm) {
  if (pm == NULL) {
    return -1;
  }

  MMDB_close(pm);
  free(pm);
  return 0;
}

int ipgeo_load(const char *filename) {
  MMDB_s *pm = (MMDB_s *)malloc(sizeof(MMDB_s));
  int status = MMDB_open(filename, MMDB_MODE_MMAP, pm);
  if (MMDB_SUCCESS != status) {
    return -1;
  }

  MMDB_s *om = (MMDB_s *)atomic_exchange(&g_mmdb, pm);
  ipgeo_close(om);
  return 0;
}

int ipgeo_jailed(const struct sockaddr *ipaddr) {
  int mmdb_error;
  MMDB_s *pm = (MMDB_s *)atomic_load(&g_mmdb);
  MMDB_lookup_result_s result = MMDB_lookup_sockaddr(pm, ipaddr, &mmdb_error);
  if (MMDB_SUCCESS != mmdb_error) {
    return -1;
  }

  if (result.found_entry) {
    MMDB_entry_data_s entry_data;
    int status =
        MMDB_get_value(&result.entry, &entry_data, "country", "iso_code", NULL);
    if (MMDB_SUCCESS != status) {
      return -1;
    }

    if (entry_data.has_data && MMDB_DATA_TYPE_UTF8_STRING == entry_data.type) {
      if (strncmp("CN", entry_data.utf8_string, 2) == 0) {
        return 1;
      }
    }
  }
  return 0;
}

int ipgeo_fetch(const char *filename) {
  const char *body = NULL;
  size_t body_len = 0;
  int res = http_get(ipgeo_database_url, &body, &body_len);
  if (res != 0) {
    return -2;
  }

  FILE *file = fopen(filename, "w");
  res = fwrite(body, 1, body_len, file);
  fclose(file);
  free((void *)body);

  return res > 0? 0 : -1;
}

int ipgeo_update() {
  char filename[64] = {0};
  snprintf(filename, 64, "ipgeo-%d.mmdb", file_rotate);
  file_rotate = 1 - file_rotate;
  int res = ipgeo_fetch(filename);
  if (res != 0) {
    return res;
  }

  return ipgeo_load(filename);
}