#ifndef DUAL_SITE_MATCHER_H
#define DUAL_SITE_MATCHER_H

#include <stddef.h>
#include <sqlite3.h>

typedef struct {
    sqlite3* db;
    const char* url;
    const char* database_name;
} site_matcher;

site_matcher* new_site_matcher(const char* url, const char* database_name);

int site_matcher_reload(site_matcher* pmatcher);

int site_matcher_find(site_matcher* pmatcher, const char* domain, const size_t length);

void free_site_matcher(site_matcher* pmatcher);

#endif