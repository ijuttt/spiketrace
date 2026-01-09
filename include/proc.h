#ifndef PROC_H
#define PROC_H

#include "snapshot.h"
#include "spkt_common.h"

#include <stddef.h>

/* Maximum processes to track for CPU delta calculation */
#define PROC_MAX_TRACKED 256

/* Single process sample for CPU delta calculation */
typedef struct {
  int32_t pid;
  unsigned long long ticks; // utime + stime
  uint64_t rss_kib;
  double cpu_pct; // calculated CPU% (0..100 per core, can exceed 100 on
                  // multi-core)
  char comm[16];
  int valid;
} proc_sample_t;

/* Context for tracking process samples between collections */
typedef struct {
  proc_sample_t samples[PROC_MAX_TRACKED];
  size_t count;
  unsigned long long last_total_ticks; // system-wide CPU ticks at last sample
} proc_context_t;

/* Initialize process collector context */
spkt_status_t proc_context_init(proc_context_t *ctx);

/* Cleanup process collector context */
spkt_status_t proc_context_cleanup(proc_context_t *ctx);

/* Collect top processes by CPU usage into snapshot
 * Requires two calls to get accurate CPU% (first call establishes baseline)
 */
spkt_status_t proc_collect_snapshot(proc_context_t *ctx,
                                    spkt_proc_snapshot_t *out);

#endif
