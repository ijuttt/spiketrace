#include "cpu.h"
#include "spkt_common.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAT_BUFF 256
#define CPU_PREFIX "cpu"
#define CPU_PREFIX_LEN 3
#define LABEL_MAX 8
#define LABEL_FMT "%7s"

// get current each cpu core jiffies from /proc/stat
spkt_status_t get_curr_stats(struct curr_stat *curr, int max_cores) {
  FILE *stat_fp = fopen("/proc/stat", "r");
  if (!stat_fp) {
    return SPKT_ERR_GET_CPU_STAT;
  }

  char buf[STAT_BUFF];
  int cores_found = 0;

  while (fgets(buf, sizeof(buf), stat_fp)) {
    // check prefix "cpu"
    if (strncmp(buf, CPU_PREFIX, CPU_PREFIX_LEN) != 0)
      break;

    char label[LABEL_MAX];
    struct curr_stat *s = NULL;

    if (sscanf(buf, LABEL_FMT, label) != 1)
      continue;

    if (strcmp(label, CPU_PREFIX) == 0) {
      s = &curr[0]; // TOTAL
    } else if (label[3] && isdigit((unsigned char)label[3])) {
      char *end = NULL;
      long core = strtol(label + 3, &end, 10);

      if (*end != '\0' || core < 0)
        continue;
      if (core + 1 >= max_cores)
        continue;
      s = &curr[core + 1];
    }

    if (!s)
      continue;
    int scanned = sscanf(buf, "%*s %llu %llu %llu %llu %llu %llu %llu",
                         &s->user, &s->nice, &s->system, &s->idle, &s->iowait,
                         &s->irq, &s->softirq);
    if (scanned != 7)
      continue;
    // success if at least one cpu line parsed
    cores_found++;
  }

  fclose(stat_fp);
  return (cores_found > 0) ? SPKT_OK : SPKT_ERR_GET_CPU_STAT;
}

// helper to calculate total jiffies
static inline unsigned long long total_jiffies(const struct curr_stat *s) {
  return s->user + s->nice + s->system + s->idle + s->iowait + s->irq +
         s->softirq;
}

// helper to calculate idle jiffies
static inline unsigned long long idle_jiffies(const struct curr_stat *s) {
  return s->idle + s->iowait;
}

spkt_status_t cpu_calc_usage_delta_batch(const struct curr_stat *prev,
                                         const struct curr_stat *curr,
                                         int num_cores, double *out_usage) {

  if (!prev || !curr || !out_usage || num_cores <= 0) {
    return SPKT_ERR_INVALID_PARAM;
  }

  // expects curr size >= num_cores + 1 (including TOTAL)
  for (int i = 1; i <= num_cores; i++) {
    unsigned long long total_curr = total_jiffies(&curr[i]);
    unsigned long long total_prev = total_jiffies(&prev[i]);

    unsigned long long idle_curr = idle_jiffies(&curr[i]);
    unsigned long long idle_prev = idle_jiffies(&prev[i]);

    unsigned long long total_diff = total_curr - total_prev;
    unsigned long long idle_diff = idle_curr - idle_prev;

    if (total_diff == 0 || idle_diff > total_diff) {
      out_usage[i - 1] = 0.0;
      continue;
    }

    double idle_ratio = (double)idle_diff / total_diff;
    out_usage[i - 1] = (1.0 - idle_ratio) * 100.0;
  }
  return SPKT_OK;
}
