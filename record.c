#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libcask/co_define.h"

#include "record.h"
#include "refer.h"

/*
start | end | src | dst | target | host | ip_jailed | site_jailed | success | bytes
*/

static const char *access_sql_create =
    "CREATE TABLE access_log (id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "start INTEGER NOT NULL, end INTEGER NOT NULL, src TEXT NOT NULL,"
    "dst TEXT NOT NULL, target TEXT, host TEXT,"
    "ip_jailed INTEGER DEFAULT 0, site_jailed INTEGER DEFAULT 0,"
    "success INTEGER DEFAULT 0, bytes INTEGER DEFAULT 0);";

static const char *access_log = "logging.db";

char *addr_string(const struct sockaddr_in *addr) {
  char *buffer = calloc(128, 1);
  snprintf(buffer, 128, "%s:%d", inet_ntoa(addr->sin_addr),
           ntohs(addr->sin_port));
  return buffer;
}

int record_draw(struct record *record) {
  char sql_buffer[1024] = {0};

  char *src = addr_string(&record->from);
  char *dst = addr_string(&record->to);
  char *target = addr_string(&record->target);

  snprintf(sql_buffer, 1024,
           "INSERT INTO access_log (start, end, src, dst, target, host, "
           "ip_jailed, site_jailed, success, bytes)"
           "VALUES (%ld, %ld, '%s', '%s', '%s', '%s', %d, %d, %d, %d);",
           record->start_unixtime, record->end_unixtime, src, dst, target,
           record->host, record->ip_jailed, record->site_jailed,
           record->success, record->bytes);

  free(src);
  free(dst);
  free(target);

  sqlite3 *db = NULL;
  char *err_str = NULL;
  int rc = sqlite3_open(access_log, &db);
  if (rc != SQLITE_OK) {
    return -1;
  }

  rc = sqlite3_exec(db, sql_buffer, NULL, 0, &err_str);
  if (rc != SQLITE_OK) {
    ERR_LOG("sqlite insert access log error: %s", err_str);
    sqlite3_free(err_str);
  }

  sqlite3_close(db);

  return 0;
}

struct record *new_record() {
  return calloc(1, sizeof(struct record));
}

struct record *refer_record(struct record *record) {
  refer(record);
  return record;
}

void defer_record(struct record *record) {
  if (record && record->ref.cnt == 1) {
    time(&record->end_unixtime);
    record_draw(record);
  }
  defer(record);
}

int record_init() {
  sqlite3 *db = NULL;
  char *err_str = NULL;
  int rc = sqlite3_open(access_log, &db);
  if (rc != SQLITE_OK) {
    return -1;
  }

  rc = sqlite3_exec(db, access_sql_create, NULL, 0, &err_str);
  if (rc != SQLITE_OK) {
    ERR_LOG("sqlite create table %s error: %s", access_log, err_str);
    sqlite3_free(err_str);
  }

  sqlite3_close(db);
  return rc;
}
