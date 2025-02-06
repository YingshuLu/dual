#ifndef DUAL_SITE_MATCHER_H
#define DUAL_SITE_MATCHER_H

struct site_matcher* new_site_matcher(const char* url, const char* database_name);

int site_matcher_reload(struct site_matcher* pmatcher);

int site_matcher_find(struct site_matcher* pmatcher, const char* domain, int length);

void free_site_mathcer(struct site_matcher* pmatcher);

#endif