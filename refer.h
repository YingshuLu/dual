#ifndef DUAL_REFER_H
#define DUAL_REFER_H

struct ref_t {
  int cnt;
};

void *refer(void *obj);

void defer(void *obj);

#endif