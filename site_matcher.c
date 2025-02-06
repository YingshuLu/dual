#include "site_matcher.h"

static const char *SQL_CREATE = "CREATE TABLE site (ID INTEGER PRIMARY KEY "
                                "AUTOINCREMENT, Key TEXT UNIQUE NOT NULL);";

typedef struct {
    int (*sqlite_init)(const char* database_name, const char* sql);
    int (*sqlite_insert)(sqlite3* db, const char* domain, int length);
    int (*sqlite_open)(const char* database_name, sqlite3** db);
    int (*sqlite_close)(sqlite3* db);
    int (*sqlite_find)(sqlite3* db, const char* domain, int length);
} sqlite_operator;

typedef struct {
    site_matcher* matcher;
    sqlite_operator* operator;
    sqlite3* db;
} site_matcjer;

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

int sqlite_init(const char* databse_name) {
    sqlite3 *db = NULL;
    char *err_str = NULL;
    int rc = sqlite3_open(databse_name, &db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    rc = sqlite3_exec(db, SQL_CREATE, NULL, 0, &err_str);
    if (rc != SQLITE_OK) {
        ERR_LOG("sqlite create table error: %s", err_str);
        sqlite3_free(err_str);
    }

    return rc;
}

int sqlite_insert(sqlite3* db, const char* domain, int domain_len) {
    if (0 != verify_domain(domain, domain_len)) {
        return -1;
    }

    char sql_buffer[1024] = {0};
    snprintf(sql_buffer, 1024, "INSERT INTO site (Key) VALUES ('%.*s');",
               domain_len, domain);

    char *err_str = NULL;
    rc = sqlite3_exec(db, sql_buffer, NULL, 0, &err_str);
    if (rc != SQLITE_OK) {
        ERR_LOG("sqlite insert domain %.*s error: [%d] - %s", domain_len, domain,
                rc, err_str);
        sqlite3_free(err_str);
    }
    return rc;
}

int sqlite_open(const char* database_name, sqlite3** db) {
    return sqlite3_open(database_name, db);
}

int sqlite_close(sqlite3* db) {
    return sqlite3_close(db);
}

static int __callback(void *userdata, int argc, char **argv,
                         char **azColName) {
  int *pcnt = (int *)userdata;
  *pcnt = argc;
  return 0;
}

int sqlite_find(sqlite3* db, const char* domain, int length) {
    if (0 != verify_domain(domain, domain_len)) {
        return -1;
    }

    char sql_buffer[1024] = {0};
    snprintf(sql_buffer, 1024, "SELECT * FROM site WHERE Key= ('%.*s');",
               (int)length, domain);

    char *err_str = NULL;
    int count = 0;
    rc = sqlite3_exec(db, sql_buffer, __callback, &count, &err_str);
    if (rc != SQLITE_OK) {
        ERR_LOG("sqlite find domain %.*s error: [%d] - %s", length, domain, rc,
                err_str);
        sqlite3_free(err_str);
    }
    return count > 0;
}

site_matcher _sqlite_operator = {
    .sqlite_init = sqlite_init,
    .sqlite_open = sqlite_open,
    .sqlite_close = sqlite_close,
    .sqlite_insert = sqlite_insert,
    .sqlite_find = sqlite_find,
};

site_matcher* new_site_matcher(const char* url, const char* database_name) {
    site_matcher* psite = (site_matcher*) malloc(sizeof(site_matcher));
    psite->operator = &_sqlite_operator;
    psite->db = NULL;
    psite->url = url;
    psite->database_name = database_name;
    return (site_matcher*)psite;
}

int site_matcher_find(site_matcher* p, char *host, const size_t host_len) {
  int found = p->find(p->db, host, host_len);
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
    found = site_find(p, left, end - left);
  }
  return found;
}

void free_site_matcher(site_matcher* pmatcher) {
    pmatcher->operator->sqlite_close(p->db);
    free((void*) pmatcher);
}