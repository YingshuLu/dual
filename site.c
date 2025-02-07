#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

#include "libcask/co_define.h"

#include "http.h"
#include "site_matcher.h"

static const char *jailed_list_url = "https://cdn.jsdelivr.net/gh/Loyalsoldier/"
                                     "v2ray-rules-dat@release/china-list.txt";
static const char *jailed_sqlite = "jail.db";

static const char *ad_list_url = "https://anti-ad.net/domains.txt";
static const char *ad_sqlite = "ad.db";

static site_matcher* jail_matcher = 0;
static site_matcher* ad_matcher = 0;

int site_init() {
  if (jail_matcher == 0) {
    jail_matcher = new_site_matcher(jailed_list_url, jailed_sqlite);
  }

  if (ad_matcher == 0) {
    ad_matcher = new_site_matcher(ad_list_url, ad_sqlite);
  }
  return 0;
}

int site_load() {
  if (jail_matcher == 0) {
    jail_matcher = new_site_matcher(jailed_list_url, jailed_sqlite);
  }

  if (ad_matcher == 0) {
    ad_matcher = new_site_matcher(ad_list_url, ad_sqlite);
  }

  site_matcher_reload(jail_matcher);
  site_matcher_reload(ad_matcher);
  return 0;
}

int site_jailed(const char *host, const size_t host_len) {
  return site_matcher_find(jail_matcher, host, host_len);
}

int site_ad(const char *host, const size_t host_len) {
  return site_matcher_find(ad_matcher, host, host_len);
}
