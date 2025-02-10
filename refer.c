#include <stdlib.h>
#include <string.h>

#include "libcask/types.h"

#include "refer.h"

void *refer(void *obj) {
  if (!obj)
    return obj;
  struct ref_t *r = (struct ref_t *)obj;
  atomic_fetch_add(&(r->cnt), 1);
  return obj;
}

void defer(void *obj) {
  if (!obj)
    return;
  struct ref_t *r = (struct ref_t *)obj;
  if (1 == atomic_fetch_sub(&(r->cnt), 1)) {
    free(obj);
  }
}