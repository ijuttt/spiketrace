#ifndef SNAPSHOT_BUILDER_H
#define SNAPSHOT_BUILDER_H

#include "cpu.h"
#include "proc.h"
#include "snapshot.h"
#include "spkt_common.h"

/* Stateful snapshot builder - owns all CPU/proc state internally */
typedef struct {
  struct cpu_jiffies prev_jiffies[MAX_CORES + 1];
  struct cpu_jiffies curr_jiffies[MAX_CORES + 1];
  double per_core_usage[MAX_CORES];
  proc_context_t proc_ctx;
  int num_cores;
  int max_cores;
} snapshot_builder_t;

/* Initialize builder with baseline CPU read
 * num_cores: validated by caller (sysconf not called here)
 */
spkt_status_t snapshot_builder_init(snapshot_builder_t *builder, int num_cores);

/* Collect a consistent snapshot (best-effort, partial allowed)
 * Always zeros snapshot first, sets timestamp
 */
spkt_status_t snapshot_builder_collect(snapshot_builder_t *builder,
                                       spkt_snapshot_t *out);

/* Cleanup internal state (future-proof) */
spkt_status_t snapshot_builder_cleanup(snapshot_builder_t *builder);

#endif
