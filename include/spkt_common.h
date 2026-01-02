#ifndef SPKT_COMMON_H
#define SPKT_COMMON_H

typedef enum {
  SPKT_OK = 0,
  SPKT_ERR_GET_CPU_STAT = -1,
  SPKT_ERR_INVALID_PARAM = -2,
} spkt_status_t;

struct curr_stat {
  unsigned long long user;
  unsigned long long nice;
  unsigned long long system;
  unsigned long long idle;
  unsigned long long iowait;
  unsigned long long irq;
  unsigned long long softirq;
};

#endif
