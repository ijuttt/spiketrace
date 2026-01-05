#ifndef CPU_H
#define CPU_H

#include "spkt_common.h"

struct cpu_jiffies {
  unsigned long long user;
  unsigned long long nice;
  unsigned long long system;
  unsigned long long idle;
  unsigned long long iowait;
  unsigned long long irq;
  unsigned long long softirq;
};

spkt_status_t cpu_read_jiffies(struct cpu_jiffies *jiffies, int max_cores);

spkt_status_t cpu_calc_usage_pct_batch(const struct cpu_jiffies *old_jiffies,
                                       const struct cpu_jiffies *new_jiffies,
                                       int num_cores, double *out_usage);

#endif
