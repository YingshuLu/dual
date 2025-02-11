#ifndef DUAL_REFER_H
#define DUAL_REFER_H

#include <stdatomic.h>

struct ref_t {
  atomic_int cnt;
};

void *refer(void *obj);

void defer(void *obj);

#endif