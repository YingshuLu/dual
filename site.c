#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

#include "libcask/co_define.h"

#include "http.h"

static const char *jailed_list_url = "https://cdn.jsdelivr.net/gh/Loyalsoldier/"
                                     "v2ray-rules-dat@release/china-list.txt";

static const char *sql_check_table =
    "SELECT name FROM sqlite_master WHERE type='table' AND name='site';";
static const char *sql_create = "CREATE TABLE site (ID INTEGER PRIMARY KEY "
                                "AUTOINCREMENT, Key TEXT UNIQUE NOT NULL);";
static const char *jailed_sqlite = "jail.db";

int domain_char(char c) {
  return ('a' <= c && 'z' >= c) || ('A' <= c && 'Z' >= c) ||
         ('0' <= c && '9' >= c) || '-' == c || '.' == c;
}

int verify_domain(const char *domain, const size_t domain_len) {
  if (!domain || !domain_len || domain_len > 255) {
    return -1;
  }

  const char *ptr = domain, *end = domain + domain_len;
  while (ptr < end) {
    if (!domain_char(*ptr)) {
      return -1;
    }
    ptr += 1;
  }
  return 0;
}

const char *domain_start(const char *ptr, const char *end) {
  while (ptr < end && !domain_char(*ptr)) {
    ptr += 1;
  }

  return ptr >= end ? NULL : ptr;
}

const char *domain_end(const char *ptr, const char *end) {
  while (ptr < end && domain_char(*ptr)) {
    ptr += 1;
  }
  return ptr >= end ? end : ptr;
}

int database_init() {
  sqlite3 *db = NULL;
  char *err_str = NULL;
  int rc = sqlite3_open(jailed_sqlite, &db);
  if (rc != SQLITE_OK) {
    return -1;
  }

  rc = sqlite3_exec(db, sql_create, NULL, 0, &err_str);
  if (rc != SQLITE_OK) {
    ERR_LOG("sqlite create table error: %s", err_str);
    sqlite3_free(err_str);
  }

  sqlite3_close(db);
  return rc;
}

int database_open(sqlite3 **db) { return sqlite3_open(jailed_sqlite, db); }

int database_close(sqlite3 *db) { return sqlite3_close(db); }

int database_insert(const char *domain, const size_t domain_len) {
  if (0 != verify_domain(domain, domain_len)) {
    return -1;
  }

  sqlite3 *db;
  int rc = database_open(&db);
  if (rc != SQLITE_OK) {
    return rc;
  }

  char sql_buffer[1024] = {0};
  snprintf(sql_buffer, 1024, "INSERT INTO site (Key) VALUES ('%.*s');",
           (int)domain_len, domain);

  char *err_str = NULL;
  rc = sqlite3_exec(db, sql_buffer, NULL, 0, &err_str);
  if (rc != SQLITE_OK) {
    ERR_LOG("sqlite insert domain %.*s error: [%d] - %s", domain_len, domain,
            rc, err_str);
    sqlite3_free(err_str);
  }
  database_close(db);
  return rc;
}

static int find_callback(void *userdata, int argc, char **argv,
                         char **azColName) {
  int *pcnt = (int *)userdata;
  *pcnt = argc;
  return 0;
}

int database_find(const char *domain, const size_t domain_len) {
  if (0 != verify_domain(domain, domain_len)) {
    return -1;
  }

  sqlite3 *db;
  int rc = database_open(&db);
  if (rc != SQLITE_OK) {
    return rc;
  }

  char sql_buffer[1024] = {0};
  snprintf(sql_buffer, 1024, "SELECT * FROM site WHERE Key= ('%.*s');",
           (int)domain_len, domain);

  char *err_str = NULL;
  int count = 0;
  rc = sqlite3_exec(db, sql_buffer, find_callback, &count, &err_str);
  if (rc != SQLITE_OK) {
    ERR_LOG("sqlite find domain %.*s error: [%d] - %s", domain_len, domain, rc,
            err_str);
    sqlite3_free(err_str);
  }
  database_close(db);
  return count > 0;
}

int site_load() {
  database_init();

  const char *body = NULL;
  size_t body_len = 0;
  if (0 != http_get(jailed_list_url, &body, &body_len)) {
    return -1;
  }

  const char *ptr = body, *end = body + body_len;
  const char *ds, *de;
  while (ptr && ptr < end) {
    ds = domain_start(ptr, end);
    if (!ds) {
      break;
    }

    de = domain_end(ds, end);
    DBG_LOG("domain: %.*s", de - ds, ds);
    if (0 == database_find(ds, de - ds)) {
      database_insert(ds, de - ds);
    }
    ptr = de + 1;
  }

  free((void *)body);
  return 0;
}

int site_jailed(const char *host, const size_t host_len) {
  int found = database_find(host, host_len);
  if (found) {
    return found;
  }

  const char *ptr = host, *end = host + host_len;
  const char *left = NULL;
  int count = 0;
  while (ptr < end) {
    if ('.' == *ptr) {
      if (count == 0) {
        left = ptr + 1;
      }
      count += 1;
    }
    ptr += 1;
  }

  if (!left || left == end || count < 2) {
    found = 0;
  } else {
    found = site_jailed(left, end - left);
  }
  return found;
}
