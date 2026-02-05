/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#ifndef CPU_H
#define CPU_H

#include "spkt_common.h"

/* Field index in /proc/stat (1-based, after label) */
enum cpu_jiffies_field {
  CPU_STAT_FIELD_USER = 1,
  CPU_STAT_FIELD_NICE,
  CPU_STAT_FIELD_SYSTEM,
  CPU_STAT_FIELD_IDLE,
  CPU_STAT_FIELD_IOWAIT,
  CPU_STAT_FIELD_IRQ,
  CPU_STAT_FIELD_SOFTIRQ,
  CPU_STAT_FIELD_STEAL,
  CPU_STAT_FIELD_GUEST,
  CPU_STAT_FIELD_GUEST_NICE,
};

#define CPU_STAT_MIN_REQUIRED_FIELDS CPU_STAT_FIELD_IDLE

/* CPU time counters in jiffies from /proc/stat */
struct cpu_jiffies {
  unsigned long long user;
  unsigned long long nice;
  unsigned long long system;
  unsigned long long idle;
  unsigned long long iowait;
  unsigned long long irq;
  unsigned long long softirq;
  unsigned long long steal;
  unsigned long long guest;
  unsigned long long guest_nice;
};

/* Read CPU jiffies from /proc/stat. Index 0 = total, 1+ = per-core */
spkt_status_t cpu_read_jiffies(struct cpu_jiffies *jiffies, int max_cores);

/* Calculate per-core CPU usage % from two jiffies snapshots */
spkt_status_t cpu_calc_usage_pct_batch(const struct cpu_jiffies *old_jiffies,
                                       const struct cpu_jiffies *new_jiffies,
                                       int num_cores, double *out_usage);

/* Sum all jiffies (excluding guest & guest_nice to avoid double-counting) */
unsigned long long total_jiffies(const struct cpu_jiffies *j);

#endif
