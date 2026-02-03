/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#define _POSIX_C_SOURCE 200809L

#include "proc.h"
#include "cpu.h"
#include "spkt_common.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROC_PATH "/proc"
#define STAT_BUF_SIZE 512
#define STATM_BUF_SIZE 128
#define DEFAULT_BASELINE_ALPHA 0.3 /* EMA smoothing for baseline (higher = more responsive) */

static long page_size_bytes = 0;

static long get_page_size(void) {
  if (page_size_bytes == 0) {
    page_size_bytes = sysconf(_SC_PAGESIZE);
    if (page_size_bytes <= 0) {
      page_size_bytes = 4096; // fallback
    }
  }
  return page_size_bytes;
}

/* Read system-wide CPU ticks */
static unsigned long long read_total_system_ticks(void) {
  struct cpu_jiffies jiffies[1] = {0};
  if (cpu_read_jiffies(jiffies, 1) != SPKT_OK) {
    return 0;
  }
  return total_jiffies(&jiffies[0]);
}

/* Parse utime+stime, ppid, pgid, and comm from /proc/[pid]/stat */
static int parse_proc_stat(int pid, unsigned long long *out_ticks,
                           int32_t *out_ppid, int32_t *out_pgid,
                           char *out_comm, size_t comm_size) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);

  FILE *fp = fopen(path, "r");
  if (!fp) {
    return -1;
  }

  char buf[STAT_BUF_SIZE];
  if (!fgets(buf, sizeof(buf), fp)) {
    fclose(fp);
    return -1;
  }
  fclose(fp);

  // Find comm field (enclosed in parentheses)
  char *comm_start = strchr(buf, '(');
  char *comm_end = strrchr(buf, ')');

  if (!comm_start || !comm_end || comm_end <= comm_start) {
    return -1;
  }

  /* Extract comm */
  size_t comm_len = (size_t)(comm_end - comm_start - 1);
  if (comm_len >= comm_size) {
    comm_len = comm_size - 1;
  }
  strncpy(out_comm, comm_start + 1, comm_len);
  out_comm[comm_len] = '\0';

  // Parse fields after comm: state is field 3, ppid is 4, pgrp is 5
  // utime is 14, stime is 15
  char *fields_start = comm_end + 2; // skip ") "

  unsigned long utime = 0, stime = 0;
  int ppid_raw = 0, pgid_raw = 0;

  int scanned = sscanf(
      fields_start,
      "%*c "             // 3: state
      "%d %d "           // 4-5: ppid, pgrp
      "%*d %*d %*d "     // 6-8: session, tty, tpgid
      "%*u %*u %*u %*u %*u " // 9-13: flags, minflt, cminflt, majflt, cmajflt
      "%lu %lu",         // 14-15: utime, stime
      &ppid_raw, &pgid_raw, &utime, &stime);

  if (scanned != 4) {
    return -1;
  }

  *out_ticks = (unsigned long long)(utime + stime);
  *out_ppid = ppid_raw;
  *out_pgid = pgid_raw;
  return 0;
}

/* Parse /proc/[pid]/statm for RSS */
static int parse_proc_statm(int pid, uint64_t *out_rss_kib) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/statm", pid);

  FILE *fp = fopen(path, "r");
  if (!fp) {
    return -1;
  }

  unsigned long size_pages, resident_pages;
  int scanned = fscanf(fp, "%lu %lu", &size_pages, &resident_pages);
  fclose(fp);

  if (scanned != 2) {
    return -1;
  }

  *out_rss_kib = (uint64_t)resident_pages * get_page_size() / 1024;
  return 0;
}

/* Find previous sample for a PID */
static proc_sample_t *find_prev_sample(proc_context_t *ctx, int32_t pid) {
  for (size_t i = 0; i < ctx->count; i++) {
    if (ctx->samples[i].valid && ctx->samples[i].pid == pid) {
      return &ctx->samples[i];
    }
  }
  return NULL;
}

/* Comparison function for qsort - sort by CPU% descending */
static int compare_samples_by_cpu(const void *a, const void *b) {
  const proc_sample_t *sa = (const proc_sample_t *)a;
  const proc_sample_t *sb = (const proc_sample_t *)b;

  /* Invalid entries go to the end */
  if (!sa->valid && !sb->valid)
    return 0;
  if (!sa->valid)
    return 1;
  if (!sb->valid)
    return -1;

  /* Sort by cpu_pct descending */
  if (sb->cpu_pct > sa->cpu_pct)
    return 1;
  if (sb->cpu_pct < sa->cpu_pct)
    return -1;

  /* Tiebreaker: RSS descending */
  if (sb->rss_kib > sa->rss_kib)
    return 1;
  if (sb->rss_kib < sa->rss_kib)
    return -1;

  return 0;
}

/* Comparison function for qsort - sort by RSS descending */
static int compare_samples_by_rss(const void *a, const void *b) {
  const proc_sample_t *sa = (const proc_sample_t *)a;
  const proc_sample_t *sb = (const proc_sample_t *)b;

  /* Invalid entries go to the end */
  if (!sa->valid && !sb->valid)
    return 0;
  if (!sa->valid)
    return 1;
  if (!sb->valid)
    return -1;

  /* Sort by rss_kib descending */
  if (sb->rss_kib > sa->rss_kib)
    return 1;
  if (sb->rss_kib < sa->rss_kib)
    return -1;

  /* Tiebreaker: CPU descending */
  if (sb->cpu_pct > sa->cpu_pct)
    return 1;
  if (sb->cpu_pct < sa->cpu_pct)
    return -1;

  return 0;
}

spkt_status_t proc_context_init(proc_context_t *ctx) {
  if (!ctx) {
    return SPKT_ERR_NULL_POINTER;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->baseline_alpha = DEFAULT_BASELINE_ALPHA;
  ctx->top_processes_limit = MAX_PROCS; /* Default to max */
  return SPKT_OK;
}

spkt_status_t proc_context_cleanup(proc_context_t *ctx) {
  if (!ctx) {
    return SPKT_ERR_NULL_POINTER;
  }
  memset(ctx, 0, sizeof(*ctx));
  return SPKT_OK;
}

spkt_status_t proc_collect_snapshot(proc_context_t *ctx,
                                    spkt_proc_snapshot_t *out) {
  if (!ctx || !out) {
    return SPKT_ERR_NULL_POINTER;
  }

  memset(out, 0, sizeof(*out));

  // Get current total system ticks
  unsigned long long curr_total_ticks = read_total_system_ticks();

  // Underflow protection
  unsigned long long tick_delta = 0;
  if (curr_total_ticks > ctx->last_total_ticks) {
    tick_delta = curr_total_ticks - ctx->last_total_ticks;
  }

  // First call - just establish baseline
  int is_first_call = (ctx->last_total_ticks == 0);

  // Temporary storage for current samples
  proc_sample_t curr_samples[PROC_MAX_TRACKED];
  memset(curr_samples, 0, sizeof(curr_samples));
  size_t curr_count = 0;

  DIR *proc_dir = opendir(PROC_PATH);
  if (!proc_dir) {
    return SPKT_ERR_PROC_OPEN_DIR;
  }

  struct dirent *entry;
  while ((entry = readdir(proc_dir)) != NULL && curr_count < PROC_MAX_TRACKED) {
    // Skip non-numeric directories
    if (!isdigit((unsigned char)entry->d_name[0])) {
      continue;
    }

    int pid = atoi(entry->d_name);
    if (pid <= 0) {
      continue;
    }

    proc_sample_t sample = {0};
    sample.pid = pid;
    sample.valid = 1;
    sample.cpu_pct = 0.0;
    sample.baseline_cpu_pct = 0.0;
    sample.sample_count = 0;
    sample.is_new = true; /* Assume new until we find prev */

    /* Read process stats */
    if (parse_proc_stat(pid, &sample.ticks, &sample.ppid, &sample.pgid,
                        sample.comm, sizeof(sample.comm)) != 0) {
      continue;
    }

    if (parse_proc_statm(pid, &sample.rss_kib) != 0) {
      continue;
    }

    /* Calculate CPU% and track baseline */
    proc_sample_t *prev = find_prev_sample(ctx, sample.pid);

    if (!is_first_call && tick_delta > 0) {
      if (prev && sample.ticks >= prev->ticks) {
        unsigned long long proc_delta = sample.ticks - prev->ticks;
        sample.cpu_pct = 100.0 * (double)proc_delta / (double)tick_delta;
      }
    }

    if (prev) {
      /* Existing process - inherit and update baseline */
      sample.is_new = false;
      sample.sample_count =
          (prev->sample_count < 255) ? prev->sample_count + 1 : 255;
      /* EMA: baseline = alpha * current + (1 - alpha) * prev_baseline */
      sample.baseline_cpu_pct = ctx->baseline_alpha * sample.cpu_pct +
                                (1.0 - ctx->baseline_alpha) * prev->baseline_cpu_pct;
    } else {
      /* New process - establish initial baseline */
      sample.is_new = true;
      sample.sample_count = 1;
      sample.baseline_cpu_pct = sample.cpu_pct;
    }

    curr_samples[curr_count++] = sample;
  }

  closedir(proc_dir);

  if (curr_count == 0) {
    ctx->last_total_ticks = curr_total_ticks;
    return SPKT_OK;
  }

  /* Sort by CPU and copy top processes */
  qsort(curr_samples, curr_count, sizeof(proc_sample_t), compare_samples_by_cpu);

  size_t copy_count = (curr_count < ctx->top_processes_limit) ? curr_count : ctx->top_processes_limit;
  if (copy_count > MAX_PROCS) {
    copy_count = MAX_PROCS; /* Clamp to array size */
  }
  for (size_t i = 0; i < copy_count; i++) {
    out->entries[i].pid = curr_samples[i].pid;
    strncpy(out->entries[i].comm, curr_samples[i].comm,
            sizeof(out->entries[i].comm) - 1);
    out->entries[i].comm[sizeof(out->entries[i].comm) - 1] = '\0';
    out->entries[i].cpu_usage_pct = curr_samples[i].cpu_pct;
    out->entries[i].rss_kib = curr_samples[i].rss_kib;
  }
  out->valid_entry_count = (uint32_t)copy_count;

  /* Re-sort by RSS and copy top memory consumers */
  qsort(curr_samples, curr_count, sizeof(proc_sample_t), compare_samples_by_rss);

  for (size_t i = 0; i < copy_count; i++) {
    out->top_rss_entries[i].pid = curr_samples[i].pid;
    strncpy(out->top_rss_entries[i].comm, curr_samples[i].comm,
            sizeof(out->top_rss_entries[i].comm) - 1);
    out->top_rss_entries[i].comm[sizeof(out->top_rss_entries[i].comm) - 1] = '\0';
    out->top_rss_entries[i].cpu_usage_pct = curr_samples[i].cpu_pct;
    out->top_rss_entries[i].rss_kib = curr_samples[i].rss_kib;
  }
  out->valid_rss_count = (uint32_t)copy_count;

  /* Update context for next call */
  memset(ctx->samples, 0, sizeof(ctx->samples));
  memcpy(ctx->samples, curr_samples, curr_count * sizeof(proc_sample_t));
  ctx->count = curr_count;
  ctx->last_total_ticks = curr_total_ticks;

  return SPKT_OK;
}
