#include <stdlib.h>
#include <string.h>

#include "libcask/types.h"

#include "refer.h"

void *refer(void *obj) {
  if (!obj)
    return obj;
  struct ref_t *r = (struct ref_t *)obj;
  r->cnt += 1;
  return obj;
}

void defer(void *obj) {
  if (!obj)
    return;
  struct ref_t *r = (struct ref_t *)obj;
  r->cnt -= 1;
  if (0 == r->cnt) {
    free(obj);
  }
}