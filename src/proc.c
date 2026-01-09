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

/* Parse utime+stime and comm from /proc/[pid]/stat */
static int parse_proc_stat(int pid, unsigned long long *out_ticks,
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

  // Parse fields after comm: state is field 3, utime is 14, stime is 15
  // Fields are space-separated after the closing paren
  char *fields_start = comm_end + 2; // skip ") "

  unsigned long utime = 0, stime = 0;

  /* utime/stime follow comm and state fields */
  int scanned = sscanf(
      fields_start,
      "%*c "                 // 3: state
      "%*d %*d %*d %*d %*d " // 4-8: ppid, pgrp, session, tty, tpgid
      "%*u %*u %*u %*u %*u " // 9-13: flags, minflt, cminflt, majflt, cmajflt
      "%lu %lu",             // 14-15: utime, stime
      &utime, &stime);

  if (scanned != 2) {
    return -1;
  }

  *out_ticks = (unsigned long long)(utime + stime);
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
static int compare_samples(const void *a, const void *b) {
  const proc_sample_t *sa = (const proc_sample_t *)a;
  const proc_sample_t *sb = (const proc_sample_t *)b;

  // Invalid entries go to the end
  if (!sa->valid && !sb->valid)
    return 0;
  if (!sa->valid)
    return 1;
  if (!sb->valid)
    return -1;

  // Sort by ticks descending (higher = more CPU)
  if (sb->ticks > sa->ticks)
    return 1;
  if (sb->ticks < sa->ticks)
    return -1;

  // Tiebreaker: RSS descending
  if (sb->rss_kib > sa->rss_kib)
    return 1;
  if (sb->rss_kib < sa->rss_kib)
    return -1;

  return 0;
}

spkt_status_t proc_context_init(proc_context_t *ctx) {
  if (!ctx) {
    return SPKT_ERR_NULL_POINTER;
  }
  memset(ctx, 0, sizeof(*ctx));
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
  unsigned long long tick_delta = curr_total_ticks - ctx->last_total_ticks;

  // First call - just establish baseline
  int is_first_call = (ctx->last_total_ticks == 0);

  // Temporary storage for current samples
  proc_sample_t curr_samples[PROC_MAX_TRACKED];
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

    // Read process stats
    if (parse_proc_stat(pid, &sample.ticks, sample.comm, sizeof(sample.comm)) !=
        0) {
      continue;
    }

    if (parse_proc_statm(pid, &sample.rss_kib) != 0) {
      continue;
    }

    curr_samples[curr_count++] = sample;
  }

  closedir(proc_dir);

  if (curr_count == 0) {
    ctx->last_total_ticks = curr_total_ticks;
    return SPKT_OK;
  }

  // Calculate CPU% for each process based on delta from previous sample
  double cpu_pcts[PROC_MAX_TRACKED] = {0};

  if (!is_first_call && tick_delta > 0) {
    for (size_t i = 0; i < curr_count; i++) {
      proc_sample_t *curr = &curr_samples[i];
      proc_sample_t *prev = find_prev_sample(ctx, curr->pid);

      if (prev && curr->ticks >= prev->ticks) {
        unsigned long long proc_delta = curr->ticks - prev->ticks;
        cpu_pcts[i] = 100.0 * (double)proc_delta / (double)tick_delta;

        if (cpu_pcts[i] > 100.0) {
          cpu_pcts[i] = 100.0;
        }
      }
    }
  }

  proc_sample_t sorted[PROC_MAX_TRACKED];
  memcpy(sorted, curr_samples, sizeof(sorted));

  for (size_t i = 0; i < curr_count; i++) {
    sorted[i].ticks = (unsigned long long)(cpu_pcts[i] * 1000000.0);
  }

  qsort(sorted, curr_count, sizeof(proc_sample_t), compare_samples);

  // Copy top processes to output
  size_t copy_count = (curr_count < MAX_PROCS) ? curr_count : MAX_PROCS;
  for (size_t i = 0; i < copy_count; i++) {
    out->entries[i].pid = sorted[i].pid;
    strncpy(out->entries[i].comm, sorted[i].comm,
            sizeof(out->entries[i].comm) - 1);
    out->entries[i].comm[sizeof(out->entries[i].comm) - 1] = '\0';
    out->entries[i].cpu_usage_pct = (double)sorted[i].ticks / 1000000.0;
    out->entries[i].rss_kib = sorted[i].rss_kib;
  }
  out->valid_entry_count = (uint32_t)copy_count;

  // Update context for next call
  memcpy(ctx->samples, curr_samples, sizeof(ctx->samples));
  ctx->count = curr_count;
  ctx->last_total_ticks = curr_total_ticks;

  return SPKT_OK;
}
