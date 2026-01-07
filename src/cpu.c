#include "cpu.h"
#include "spkt_common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_STAT_PATH "/proc/stat"
#define STAT_BUFF 256
#define CPU_PREFIX "cpu"
#define CPU_PREFIX_LEN 3
#define LABEL_MAX 8

/* Parse single CPU line from /proc/stat */
static spkt_status_t parse_cpu_line(const char *line, struct cpu_jiffies *out) {
  if (!line || !out) {
    return SPKT_ERR_NULL_POINTER;
  }

  *out = (struct cpu_jiffies){0};

  int scanned = sscanf(
      line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &out->user,
      &out->nice, &out->system, &out->idle, &out->iowait, &out->irq,
      &out->softirq, &out->steal, &out->guest, &out->guest_nice);

  return (scanned >= CPU_STAT_MIN_REQUIRED_FIELDS) ? SPKT_OK
                                                   : SPKT_ERR_CPU_PARSE_FAILED;
}

/* Extract core number from label like "cpu0", "cpu1" */
static int parse_core_num(const char *label) {
  if (strcmp(label, CPU_PREFIX) == 0) {
    return 0; // Total CPU
  }

  if (!isdigit((unsigned char)label[3])) {
    return -1;
  }

  char *end = NULL;
  long core = strtol(label + 3, &end, 10);

  if (*end != '\0' || core < 0) {
    return -1;
  }

  return (int)core + 1; // Offset by 1 (index 0 = total)
}

spkt_status_t cpu_read_jiffies(struct cpu_jiffies *jiffies, int max_cores) {
  if (!jiffies || max_cores <= 0) {
    return SPKT_ERR_INVALID_PARAM;
  }

  FILE *fp = fopen(PROC_STAT_PATH, "r");
  if (!fp) {
    return SPKT_ERR_CPU_OPEN_PROC;
  }

  char buf[STAT_BUFF];
  int parsed_count = 0;

  while (fgets(buf, sizeof(buf), fp)) {
    // Stop when we hit non-cpu lines
    if (strncmp(buf, CPU_PREFIX, CPU_PREFIX_LEN) != 0) {
      break;
    }

    char label[LABEL_MAX] = {0};
    if (sscanf(buf, "%7s", label) != 1) {
      continue;
    }

    int idx = parse_core_num(label);
    if (idx < 0 || idx >= max_cores) {
      continue;
    }

    if (parse_cpu_line(buf, &jiffies[idx]) == SPKT_OK) {
      parsed_count++;
    }
  }

  fclose(fp);

  return (parsed_count > 0) ? SPKT_OK : SPKT_ERR_CPU_PARSE_FAILED;
}

/* Sum all jiffies
 * Excluding 'guest' & 'guest_nice' to avoid double-counting
 */
static inline unsigned long long total_jiffies(const struct cpu_jiffies *j) {
  return j->user + j->nice + j->system + j->idle + j->iowait + j->irq +
         j->softirq + j->steal;
}

/* Sum idle categories */
static inline unsigned long long idle_jiffies(const struct cpu_jiffies *j) {
  return j->idle + j->iowait;
}

spkt_status_t cpu_calc_usage_pct_batch(const struct cpu_jiffies *old_jiffies,
                                       const struct cpu_jiffies *new_jiffies,
                                       int num_cores, double *out_usage) {
  if (!old_jiffies || !new_jiffies || !out_usage) {
    return SPKT_ERR_NULL_POINTER;
  }

  if (num_cores <= 0) {
    return SPKT_ERR_INVALID_PARAM;
  }

  // Process per-core stats (skip index 0 which is total)
  for (int i = 0; i < num_cores; i++) {
    const struct cpu_jiffies *new_j = &new_jiffies[i + 1];
    const struct cpu_jiffies *old_j = &old_jiffies[i + 1];

    unsigned long long total_delta =
        total_jiffies(new_j) - total_jiffies(old_j);
    unsigned long long idle_delta = idle_jiffies(new_j) - idle_jiffies(old_j);

    // Handle edge cases
    if (total_delta == 0) {
      out_usage[i] = 0.0;
      continue;
    }

    // Sanity check: idle shouldn't exceed total
    if (idle_delta > total_delta) {
      out_usage[i] = 0.0;
      continue;
    }

    double usage = 100.0 * (1.0 - (double)idle_delta / total_delta);
    out_usage[i] = (usage < 0.0) ? 0.0 : (usage > 100.0) ? 100.0 : usage;
  }

  return SPKT_OK;
}
