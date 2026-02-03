/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#define _POSIX_C_SOURCE 200809L

#include "spike_dump.h"
#include "json_writer.h"
#include "time_format.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* File extension for temporary files during atomic write */
#define TEMP_FILE_SUFFIX ".tmp"

/* Serialize a single process entry to JSON */
static spkt_status_t serialize_proc_entry(spkt_json_writer_t *w,
                                          const spkt_proc_entry_t *entry) {
  spkt_status_t s;

  s = spkt_json_begin_object(w);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "pid");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_int(w, entry->pid);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "comm");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_string(w, entry->comm);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "cpu_pct");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_double(w, entry->cpu_usage_pct);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "rss_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, entry->rss_kib);
  if (s != SPKT_OK)
    return s;

  /* Human-readable MiB value */
  s = spkt_json_key(w, "rss_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(entry->rss_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_end_object(w);
  return s;
}

/* Serialize a single snapshot to JSON */
static spkt_status_t serialize_snapshot(spkt_json_writer_t *w,
                                        const spkt_snapshot_t *snap,
                                        uint64_t trigger_timestamp_ns) {
  spkt_status_t s;

  s = spkt_json_begin_object(w);
  if (s != SPKT_OK)
    return s;

  /* Timestamp (nanoseconds, for backward compatibility) */
  s = spkt_json_key(w, "timestamp_ns");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->timestamp_monotonic_ns);
  if (s != SPKT_OK)
    return s;

  /* Human-readable timestamp in seconds */
  s = spkt_json_key(w, "uptime_seconds");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_double(w, spkt_ns_to_seconds(snap->timestamp_monotonic_ns));
  if (s != SPKT_OK)
    return s;

  /* Offset from trigger in seconds (negative = before trigger) */
  s = spkt_json_key(w, "offset_seconds");
  if (s != SPKT_OK)
    return s;
  double offset = spkt_ns_to_seconds(snap->timestamp_monotonic_ns) -
                  spkt_ns_to_seconds(trigger_timestamp_ns);
  s = spkt_json_double(w, offset);
  if (s != SPKT_OK)
    return s;

  /* CPU */
  s = spkt_json_key(w, "cpu");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_begin_object(w);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "global_pct");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_double(w, snap->cpu.global_usage_pct);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "per_core_pct");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_begin_array(w);
  if (s != SPKT_OK)
    return s;
  for (int i = 0; i < snap->cpu.valid_core_count; i++) {
    s = spkt_json_double(w, snap->cpu.per_core_usage_pct[i]);
    if (s != SPKT_OK)
      return s;
  }
  s = spkt_json_end_array(w);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_end_object(w); /* end cpu */
  if (s != SPKT_OK)
    return s;

  /* Memory */
  s = spkt_json_key(w, "mem");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_begin_object(w);
  if (s != SPKT_OK)
    return s;

  /* KiB fields (backward compatibility) */
  s = spkt_json_key(w, "total_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.total_ram_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "available_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.available_ram_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "free_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.free_ram_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_total_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.swap_total_ram_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_free_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.swap_free_ram_kib);
  if (s != SPKT_OK)
    return s;

  /* Extended memory fields (schema v2) */
  s = spkt_json_key(w, "active_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.active_ram_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "inactive_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.inactive_ram_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "dirty_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.dirty_ram_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "slab_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.slab_ram_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "shmem_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, snap->mem.shmem_ram_kib);
  if (s != SPKT_OK)
    return s;

  /* Human-readable MiB fields (schema v3) */
  s = spkt_json_key(w, "total_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.total_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "available_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.available_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "free_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.free_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_total_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.swap_total_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_free_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.swap_free_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_used_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.swap_total_ram_kib -
                                        snap->mem.swap_free_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "active_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.active_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "inactive_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.inactive_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "dirty_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.dirty_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "slab_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.slab_ram_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "shmem_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(snap->mem.shmem_ram_kib));
  if (s != SPKT_OK)
    return s;

  /* Computed used percentage (schema v3) */
  s = spkt_json_key(w, "used_pct");
  if (s != SPKT_OK)
    return s;
  double used_pct = 100.0 * (double)(snap->mem.total_ram_kib -
                                     snap->mem.available_ram_kib) /
                    (double)snap->mem.total_ram_kib;
  s = spkt_json_double(w, used_pct);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_end_object(w); /* end mem */
  if (s != SPKT_OK)
    return s;

  /* Processes (sorted by CPU) */
  s = spkt_json_key(w, "procs");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_begin_array(w);
  if (s != SPKT_OK)
    return s;

  for (uint32_t i = 0; i < snap->procs.valid_entry_count; i++) {
    s = serialize_proc_entry(w, &snap->procs.entries[i]);
    if (s != SPKT_OK)
      return s;
  }

  s = spkt_json_end_array(w);
  if (s != SPKT_OK)
    return s;

  /* Top RSS processes (sorted by memory) */
  s = spkt_json_key(w, "top_rss_procs");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_begin_array(w);
  if (s != SPKT_OK)
    return s;

  for (uint32_t i = 0; i < snap->procs.valid_rss_count; i++) {
    s = serialize_proc_entry(w, &snap->procs.top_rss_entries[i]);
    if (s != SPKT_OK)
      return s;
  }

  s = spkt_json_end_array(w);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_end_object(w); /* end snapshot */
  return s;
}

/* Serialize anomaly result to JSON */
static spkt_status_t serialize_anomaly(spkt_json_writer_t *w,
                                       const anomaly_result_t *anomaly) {
  spkt_status_t s;
  const char *type_str;
  const char *type_desc;

  switch (anomaly->type) {
  case ANOMALY_TYPE_CPU_DELTA:
    type_str = "cpu_delta";
    type_desc = "Process CPU usage jumped significantly from baseline";
    break;
  case ANOMALY_TYPE_CPU_NEW_PROC:
    type_str = "cpu_new_process";
    type_desc = "New process spawned with high initial CPU usage";
    break;
  case ANOMALY_TYPE_MEM_DROP:
    type_str = "mem_drop";
    type_desc = "Available memory dropped suddenly";
    break;
  case ANOMALY_TYPE_MEM_PRESSURE:
    type_str = "mem_pressure";
    type_desc = "System under high memory pressure";
    break;
  case ANOMALY_TYPE_SWAP_SPIKE:
    type_str = "swap_spike";
    type_desc = "Swap usage increased suddenly";
    break;
  default:
    type_str = "unknown";
    type_desc = "Unknown anomaly type";
    break;
  }

  s = spkt_json_begin_object(w);
  if (s != SPKT_OK)
    return s;

  /* Type */
  s = spkt_json_key(w, "type");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_string(w, type_str);
  if (s != SPKT_OK)
    return s;

  /* Human-readable type description (schema v3) */
  s = spkt_json_key(w, "type_description");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_string(w, type_desc);
  if (s != SPKT_OK)
    return s;

  /* CPU fields (always included for backward compatibility) */
  s = spkt_json_key(w, "pid");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_int(w, anomaly->spike_pid);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "comm");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_string(w, anomaly->spike_comm);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "cpu_pct");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_double(w, anomaly->spike_cpu_pct);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "baseline_pct");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_double(w, anomaly->spike_baseline_pct);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "delta_pct");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_double(w, anomaly->spike_delta);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "is_new_process");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_bool(w, anomaly->is_new_process_spike);
  if (s != SPKT_OK)
    return s;

  /* Memory fields (for MEM anomalies) */
  s = spkt_json_key(w, "mem_available_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, anomaly->mem_available_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "mem_baseline_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, anomaly->mem_baseline_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "mem_delta_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_int(w, anomaly->mem_delta_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "mem_used_pct");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_double(w, anomaly->mem_used_pct);
  if (s != SPKT_OK)
    return s;

  /* Human-readable memory MiB fields (schema v3) */
  s = spkt_json_key(w, "mem_available_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(anomaly->mem_available_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "mem_baseline_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(anomaly->mem_baseline_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "mem_delta_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_int(w, (int64_t)anomaly->mem_delta_kib / 1024);
  if (s != SPKT_OK)
    return s;

  /* Swap fields (KiB for backward compatibility) */
  s = spkt_json_key(w, "swap_used_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, anomaly->swap_used_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_baseline_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, anomaly->swap_baseline_kib);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_delta_kib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_int(w, anomaly->swap_delta_kib);
  if (s != SPKT_OK)
    return s;

  /* Human-readable swap MiB fields (schema v3) */
  s = spkt_json_key(w, "swap_used_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(anomaly->swap_used_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_baseline_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_uint(w, spkt_kib_to_mib(anomaly->swap_baseline_kib));
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "swap_delta_mib");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_int(w, (int64_t)anomaly->swap_delta_kib / 1024);
  if (s != SPKT_OK)
    return s;

  /* Trigger policy context (schema v4) */
  s = spkt_json_key(w, "policy");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_begin_object(w);
  if (s != SPKT_OK)
    return s;

  const char *scope_str;
  const char *scope_desc;
  switch (anomaly->trigger_scope) {
  case TRIGGER_SCOPE_PROCESS_GROUP:
    scope_str = "process_group";
    scope_desc = "Grouped by PGID";
    break;
  case TRIGGER_SCOPE_PARENT:
    scope_str = "parent";
    scope_desc = "Grouped by PPID";
    break;
  case TRIGGER_SCOPE_SYSTEM:
    scope_str = "system";
    scope_desc = "System-wide grouping";
    break;
  case TRIGGER_SCOPE_PROCESS:
  default:
    scope_str = "per_process";
    scope_desc = "Per-process (no grouping)";
    break;
  }

  s = spkt_json_key(w, "scope");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_string(w, scope_str);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "scope_key");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_int(w, anomaly->scope_key);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_key(w, "description");
  if (s != SPKT_OK)
    return s;
  s = spkt_json_string(w, scope_desc);
  if (s != SPKT_OK)
    return s;

  s = spkt_json_end_object(w); /* end policy */
  if (s != SPKT_OK)
    return s;

  s = spkt_json_end_object(w);
  return s;
}

/* Write buffer to file atomically (write to temp, fsync, rename) */
static spkt_status_t write_atomic(const char *final_path, const char *data,
                                  size_t len) {
  char temp_path[SPIKE_DUMP_PATH_MAX + 8];
  int written;
  FILE *fp = NULL;
  spkt_status_t result = SPKT_OK;

  written = snprintf(temp_path, sizeof(temp_path), "%s%s", final_path,
                     TEMP_FILE_SUFFIX);
  if (written < 0 || (size_t)written >= sizeof(temp_path)) {
    return SPKT_ERR_INVALID_PARAM;
  }

  fp = fopen(temp_path, "w");
  if (!fp) {
    fprintf(stderr, "spike_dump: failed to open %s: %s\n", temp_path,
            strerror(errno));
    return SPKT_ERR_DUMP_OPEN_FAILED;
  }

  size_t written_bytes = fwrite(data, 1, len, fp);
  if (written_bytes != len) {
    fprintf(stderr, "spike_dump: failed to write %s: %s\n", temp_path,
            strerror(errno));
    result = SPKT_ERR_DUMP_WRITE_FAILED;
    goto cleanup;
  }

  /* Flush and sync to disk */
  if (fflush(fp) != 0) {
    fprintf(stderr, "spike_dump: failed to flush %s: %s\n", temp_path,
            strerror(errno));
    result = SPKT_ERR_DUMP_WRITE_FAILED;
    goto cleanup;
  }

  if (fsync(fileno(fp)) != 0) {
    fprintf(stderr, "spike_dump: failed to fsync %s: %s\n", temp_path,
            strerror(errno));
    result = SPKT_ERR_DUMP_WRITE_FAILED;
    goto cleanup;
  }

  fclose(fp);
  fp = NULL;

  /* Atomic rename */
  if (rename(temp_path, final_path) != 0) {
    fprintf(stderr, "spike_dump: failed to rename %s -> %s: %s\n", temp_path,
            final_path, strerror(errno));
    return SPKT_ERR_DUMP_RENAME_FAILED;
  }

  return SPKT_OK;

cleanup:
  if (fp) {
    fclose(fp);
  }
  /* Remove temp file on error */
  unlink(temp_path);
  return result;
}

spkt_status_t spike_dump_init(spike_dump_ctx_t *ctx, const char *dir) {
  if (!ctx) {
    return SPKT_ERR_NULL_POINTER;
  }

  memset(ctx, 0, sizeof(*ctx));

  const char *use_dir = dir ? dir : SPIKE_DUMP_DEFAULT_DIR;
  size_t dir_len = strlen(use_dir);

  if (dir_len >= sizeof(ctx->output_dir)) {
    return SPKT_ERR_INVALID_PARAM;
  }

  strncpy(ctx->output_dir, use_dir, sizeof(ctx->output_dir) - 1);
  ctx->output_dir[sizeof(ctx->output_dir) - 1] = '\0';

  /* Remove trailing slash if present */
  if (dir_len > 0 && ctx->output_dir[dir_len - 1] == '/') {
    ctx->output_dir[dir_len - 1] = '\0';
  }

  /* Validate directory exists and is writable */
  if (access(ctx->output_dir, W_OK) != 0) {
    fprintf(stderr, "spike_dump: directory '%s' does not exist or is not "
                    "writable: %s\n",
            ctx->output_dir, strerror(errno));
    return SPKT_ERR_DUMP_OPEN_FAILED;
  }

  ctx->dump_count = 0;

  return SPKT_OK;
}

spkt_status_t spike_dump_write(spike_dump_ctx_t *ctx,
                               const spkt_snapshot_t *snapshots,
                               size_t snapshot_count,
                               const anomaly_result_t *anomaly,
                               uint64_t timestamp_ns) {
  spkt_json_writer_t writer;
  spkt_status_t s;
  char filepath[SPIKE_DUMP_PATH_MAX];
  int written;

  if (!ctx || !snapshots || !anomaly) {
    return SPKT_ERR_NULL_POINTER;
  }

  if (snapshot_count == 0) {
    return SPKT_ERR_INVALID_PARAM;
  }

  /* Limit to max snapshots */
  if (snapshot_count > SPIKE_DUMP_MAX_SNAPSHOTS) {
    snapshot_count = SPIKE_DUMP_MAX_SNAPSHOTS;
  }

  /* Generate unique filename using wall-clock timestamp and counter.
   * Format: spike_YYYY-MM-DD_HH-MM-SS_<count>.json
   */
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char time_str[64];

  if (tm_info) {
    strftime(time_str, sizeof(time_str), "%Y-%m-%d_%H-%M-%S", tm_info);
  } else {
    /* Fallback if time calls fail, use timestamp */
    snprintf(time_str, sizeof(time_str), "unknown_%lu", (unsigned long)timestamp_ns);
  }

  written =
      snprintf(filepath, sizeof(filepath), "%s/spike_%s_%lu.json",
               ctx->output_dir, time_str, ctx->dump_count);
  if (written < 0 || (size_t)written >= sizeof(filepath)) {
    return SPKT_ERR_INVALID_PARAM;
  }

  /* Initialize JSON writer */
  s = spkt_json_init(&writer, 0); /* Use default buffer size */
  if (s != SPKT_OK) {
    return s;
  }

  /* Build JSON document */
  s = spkt_json_begin_object(&writer);
  if (s != SPKT_OK)
    goto cleanup;

  /* Schema version */
  s = spkt_json_key(&writer, "schema_version");
  if (s != SPKT_OK)
    goto cleanup;
  s = spkt_json_int(&writer, SPIKE_DUMP_SCHEMA_VERSION);
  if (s != SPKT_OK)
    goto cleanup;

  /* ISO8601 wall-clock timestamp (schema v3) */
  char iso_buf[32];
  if (spkt_format_iso8601(iso_buf, sizeof(iso_buf)) > 0) {
    s = spkt_json_key(&writer, "created_at");
    if (s != SPKT_OK)
      goto cleanup;
    s = spkt_json_string(&writer, iso_buf);
    if (s != SPKT_OK)
      goto cleanup;
  }

  /* Human-readable uptime in seconds (schema v3) */
  s = spkt_json_key(&writer, "uptime_seconds");
  if (s != SPKT_OK)
    goto cleanup;
  s = spkt_json_double(&writer, spkt_ns_to_seconds(timestamp_ns));
  if (s != SPKT_OK)
    goto cleanup;

  /* Dump timestamp (nanoseconds, for backward compatibility) */
  s = spkt_json_key(&writer, "dump_timestamp_ns");
  if (s != SPKT_OK)
    goto cleanup;
  s = spkt_json_uint(&writer, timestamp_ns);
  if (s != SPKT_OK)
    goto cleanup;

  /* Anomaly that triggered the dump */
  s = spkt_json_key(&writer, "trigger");
  if (s != SPKT_OK)
    goto cleanup;
  s = serialize_anomaly(&writer, anomaly);
  if (s != SPKT_OK)
    goto cleanup;

  /* Snapshots array (newest first) */
  s = spkt_json_key(&writer, "snapshots");
  if (s != SPKT_OK)
    goto cleanup;
  s = spkt_json_begin_array(&writer);
  if (s != SPKT_OK)
    goto cleanup;

  for (size_t i = 0; i < snapshot_count; i++) {
    s = serialize_snapshot(&writer, &snapshots[i], timestamp_ns);
    if (s != SPKT_OK)
      goto cleanup;
  }

  s = spkt_json_end_array(&writer);
  if (s != SPKT_OK)
    goto cleanup;

  s = spkt_json_end_object(&writer);
  if (s != SPKT_OK)
    goto cleanup;

  /* Check for overflow */
  if (spkt_json_has_error(&writer)) {
    fprintf(stderr, "spike_dump: JSON buffer overflow\n");
    s = SPKT_ERR_JSON_OVERFLOW;
    goto cleanup;
  }

  /* Write atomically */
  s = write_atomic(filepath, spkt_json_get_buffer(&writer),
                   spkt_json_get_length(&writer));
  if (s == SPKT_OK) {
    ctx->dump_count++;
    fprintf(stderr, "spike_dump: wrote %s (%zu bytes)\n", filepath,
            spkt_json_get_length(&writer));
  }

cleanup:
  spkt_json_cleanup(&writer);
  return s;
}

void spike_dump_cleanup(spike_dump_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  /* Currently no resources to free; structure is stack-allocated */
  memset(ctx, 0, sizeof(*ctx));
}
