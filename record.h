#ifndef DUAL_RECORD_H
#define DUAL_RECORD_H

#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>

#include "refer.h"

struct record {
  struct ref_t ref;
  time_t start_unixtime;
  time_t end_unixtime;
  struct sockaddr_in from;
  struct sockaddr_in to;
  struct sockaddr_in target;
  char host[256];
  int ip_jailed;
  int site_jailed;
  int success;
  int bytes;
};

int record_init();

struct record *new_record();

struct record *refer_record(struct record *record);

void defer_record(struct record *record);

#endif