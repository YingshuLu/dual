#include "site_matcher.h"
#include "http.h"

#include "libcask/co_define.h"

static const char *SQL_CREATE = "CREATE TABLE site (ID INTEGER PRIMARY KEY "
                                "AUTOINCREMENT, Key TEXT UNIQUE NOT NULL);";

typedef struct {
    int (*sqlite_init)(const char* database_name, sqlite3** pdb);
    int (*sqlite_insert)(sqlite3* db, const char* domain, const size_t length);
    int (*sqlite_open)(const char* database_name, sqlite3** pdb);
    int (*sqlite_close)(sqlite3* db);
    int (*sqlite_find)(sqlite3* db, const char* domain, const size_t length);
} sqlite_operator;

int domain_char(char c) {
  return ('a' <= c && 'z' >= c) || ('A' <= c && 'Z' >= c) ||
         ('0' <= c && '9' >= c) || '-' == c || '.' == c;
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

int verify_domain(const char *domain, const int domain_len) {
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

int sqlite_init(const char* databse_name, sqlite3** pdb) {
    char *err_str = 0;
    int rc = sqlite3_open(databse_name, pdb);
    if (rc != SQLITE_OK) {
        return -1;
    }

    rc = sqlite3_exec(*pdb, SQL_CREATE, 0, 0, &err_str);
    if (rc != SQLITE_OK) {
        ERR_LOG("sqlite create table %s error: %s", databse_name, err_str);
        sqlite3_free(err_str);
    }

    return rc;
}

int sqlite_insert(sqlite3* db, const char* domain, const size_t domain_len) {
    if (0 != verify_domain(domain, domain_len)) {
        return -1;
    }

    char sql_buffer[1024] = {0};
    snprintf(sql_buffer, 1024, "INSERT INTO site (Key) VALUES ('%.*s');",
               (int)domain_len, domain);

    char *err_str = 0;
    int rc = sqlite3_exec(db, sql_buffer, 0, 0, &err_str);
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

int sqlite_find(sqlite3* db, const char* domain, const size_t length) {
    if (0 != verify_domain(domain, length)) {
        return -1;
    }

    char sql_buffer[1024] = {0};
    snprintf(sql_buffer, 1024, "SELECT * FROM site WHERE Key= ('%.*s');",
               (int)length, domain);

    char *err_str = 0;
    int count = 0;
    int rc = sqlite3_exec(db, sql_buffer, __callback, &count, &err_str);
    if (rc != SQLITE_OK) {
        ERR_LOG("sqlite find domain %.*s error: [%d] - %s", (int)length, domain, rc,
                err_str);
        sqlite3_free(err_str);
    }
    return count > 0;
}

sqlite_operator _sqlite_operator = {
    .sqlite_init = sqlite_init,
    .sqlite_open = sqlite_open,
    .sqlite_close = sqlite_close,
    .sqlite_insert = sqlite_insert,
    .sqlite_find = sqlite_find,
};

site_matcher* new_site_matcher(const char* url, const char* database_name) {
    site_matcher* psite = (site_matcher*) malloc(sizeof(site_matcher));
    psite->db = 0;
    psite->url = url;
    psite->database_name = database_name;

    _sqlite_operator.sqlite_init(psite->database_name, &psite->db);
    return (site_matcher*)psite;
}

int site_matcher_find(site_matcher* p, const char *host, const size_t host_len) {
  int found = _sqlite_operator.sqlite_find(p->db, host, host_len);
  if (found) {
    return found;
  }

  const char *ptr = host, *end = host + host_len;
  const char *left = 0;
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
    found = site_matcher_find(p, left, end - left);
  }
  return found;
}

void free_site_matcher(site_matcher* pmatcher) {
    _sqlite_operator.sqlite_close(pmatcher->db);
    free((void*) pmatcher);
}

int skip(const char* start, const char* end) {
  int count = 0;
  const char* ptr = start;
  while(ptr < end && *ptr == ' ') {
    ptr++;
  }

  if (ptr < end && !domain_char(*ptr)) {
    while(ptr < end && *ptr != '\n') {
      ptr++;
    }
    return ptr - start + 1;
  }
  return ptr - start;
}

int site_matcher_reload(site_matcher* pmatcher) {
  const char* resource_url = pmatcher->url;
  const char *body = 0;
  size_t body_len = 0;
  if (0 != http_get(resource_url, &body, &body_len)) {
    return -1;
  }

  sqlite3* db;
  int rc = _sqlite_operator.sqlite_init(pmatcher->database_name, &db);
  if (rc == SQLITE_OK) {
    const char *ptr = body, *end = body + body_len;
    const char *ds, *de;
    while (ptr && ptr < end) {
      ptr += skip(ptr, end);

      ds = domain_start(ptr, end);
      if (!ds) {
        break;
      }

      de = domain_end(ds, end);
      DBG_LOG("extract domain: %.*s", de - ds, ds);
      if (0 == _sqlite_operator.sqlite_find(db, ds, de - ds)) {
        _sqlite_operator.sqlite_insert(db, ds, de - ds);
      }
      ptr = de + 1;
    }
    _sqlite_operator.sqlite_close(db);
  }

  free((void *)body);
  return 0;
}