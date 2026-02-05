/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#define _POSIX_C_SOURCE 200809L

#include "snapshot_builder.h"
#include "cpu.h"
#include "mem.h"
#include "proc.h"
#include "time_utils.h"

#include <string.h>
#include <time.h>

spkt_status_t snapshot_builder_init(snapshot_builder_t *builder,
                                    int num_cores) {
  if (!builder) {
    return SPKT_ERR_NULL_POINTER;
  }

  if (num_cores <= 0 || num_cores > MAX_CORES) {
    return SPKT_ERR_INVALID_PARAM;
  }

  memset(builder, 0, sizeof(*builder));

  builder->num_cores = num_cores;
  builder->max_cores = num_cores + 1; // index 0 = total

  // Initialize proc context
  spkt_status_t status = proc_context_init(&builder->proc_ctx);
  if (status != SPKT_OK) {
    return status;
  }

  // Initial CPU jiffies read (baseline)
  status = cpu_read_jiffies(builder->prev_jiffies, builder->max_cores);
  if (status != SPKT_OK) {
    proc_context_cleanup(&builder->proc_ctx);
    return status;
  }

  return SPKT_OK;
}

spkt_status_t snapshot_builder_collect(snapshot_builder_t *builder,
                                       spkt_snapshot_t *out) {
  if (!builder || !out) {
    return SPKT_ERR_NULL_POINTER;
  }

  // Always zero snapshot first
  memset(out, 0, sizeof(*out));

  // Always set timestamp
  out->timestamp_monotonic_ns = spkt_get_monotonic_ns();

  if (cpu_read_jiffies(builder->curr_jiffies, builder->max_cores) == SPKT_OK) {
    cpu_calc_usage_pct_batch(builder->prev_jiffies, builder->curr_jiffies,
                             builder->num_cores, builder->per_core_usage);

    double sum = 0.0;
    for (int i = 0; i < builder->num_cores; i++) {
      out->cpu.per_core_usage_pct[i] = builder->per_core_usage[i];
      sum += builder->per_core_usage[i];
    }

    out->cpu.valid_core_count = builder->num_cores;
    out->cpu.global_usage_pct = sum / builder->num_cores;

    // Update baseline for next delta
    memcpy(builder->prev_jiffies, builder->curr_jiffies,
           sizeof(builder->prev_jiffies));
  }

  struct meminfo mi;
  if (mem_read_kibibytes(&mi) == SPKT_OK) {
    out->mem.total_ram_kib = mi.total;
    out->mem.available_ram_kib = mi.available;
    out->mem.free_ram_kib = mi.free;
    out->mem.active_ram_kib = mi.active;
    out->mem.inactive_ram_kib = mi.inactive;
    out->mem.dirty_ram_kib = mi.dirty;
    out->mem.slab_ram_kib = mi.slab;
    out->mem.swap_total_ram_kib = mi.swap_total;
    out->mem.swap_free_ram_kib = mi.swap_free;
    out->mem.shmem_ram_kib = mi.shmem;
  }

  proc_collect_snapshot(&builder->proc_ctx, &out->procs);

  return SPKT_OK;
}

spkt_status_t snapshot_builder_cleanup(snapshot_builder_t *builder) {
  if (!builder) {
    return SPKT_ERR_NULL_POINTER;
  }

  proc_context_cleanup(&builder->proc_ctx);
  memset(builder, 0, sizeof(*builder));

  return SPKT_OK;
}

const proc_sample_t *
snapshot_builder_get_proc_samples(const snapshot_builder_t *builder,
                                  size_t *out_count) {
  if (!builder || !out_count) {
    if (out_count) {
      *out_count = 0;
    }
    return NULL;
  }

  *out_count = builder->proc_ctx.count;
  return builder->proc_ctx.samples;
}

void snapshot_builder_set_baseline_alpha(snapshot_builder_t *builder,
                                        double alpha) {
  if (!builder) {
    return;
  }
  builder->proc_ctx.baseline_alpha = alpha;
}

void snapshot_builder_set_top_processes_limit(snapshot_builder_t *builder,
                                              uint32_t limit) {
  if (!builder) {
    return;
  }
  if (limit > MAX_PROCS) {
    limit = MAX_PROCS; /* Clamp to array size */
  }
  if (limit == 0) {
    limit = 1; /* Minimum 1 */
  }
  builder->proc_ctx.top_processes_limit = limit;
}
