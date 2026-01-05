#ifndef CPU_H
#define CPU_H

#include "spkt_common.h"

/* CPU time counters in jiffies from /proc/stat */
struct cpu_jiffies {
  unsigned long long user;
  unsigned long long nice;
  unsigned long long system;
  unsigned long long idle;
  unsigned long long iowait;
  unsigned long long irq;
  unsigned long long softirq;
};

/* Read CPU jiffies from /proc/stat. Index 0 = total, 1+ = per-core */
spkt_status_t cpu_read_jiffies(struct cpu_jiffies *jiffies, int max_cores);

/* Calculate per-core CPU usage % from two jiffies snapshots */
spkt_status_t cpu_calc_usage_pct_batch(const struct cpu_jiffies *old_jiffies,
                                       const struct cpu_jiffies *new_jiffies,
                                       int num_cores, double *out_usage);

#endif
