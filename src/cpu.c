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
#define LABEL_FMT "%7s"
#define CPU_JIFFIES_FIELD_COUNT 7

// get current each cpu core jiffies from /proc/stat
spkt_status_t cpu_read_jiffies(struct cpu_jiffies *jiffies, int max_cores) {
  FILE *stat_fp = fopen(PROC_STAT_PATH, "r");
  if (!stat_fp) {
    return SPKT_ERR_CPU_OPEN_PROC;
  }

  char buf[STAT_BUFF];
  int parsed_count = 0;

  while (fgets(buf, sizeof(buf), stat_fp)) {
    // check prefix "cpu"
    if (strncmp(buf, CPU_PREFIX, CPU_PREFIX_LEN) != 0)
      break;

    char label[LABEL_MAX];
    struct cpu_jiffies *target = NULL;

    if (sscanf(buf, LABEL_FMT, label) != 1)
      continue;

    if (strcmp(label, CPU_PREFIX) == 0) {
      target = &jiffies[0]; // TOTAL
    } else if (label[3] && isdigit((unsigned char)label[3])) {
      char *end = NULL;
      long core_num = strtol(label + 3, &end, 10);

      if (*end != '\0' || core_num < 0)
        continue;
      if (core_num + 1 >= max_cores)
        continue;
      target = &jiffies[core_num + 1];
    }

    if (!target)
      continue;

    int scanned =
        sscanf(buf, "%*s %llu %llu %llu %llu %llu %llu %llu", &target->user,
               &target->nice, &target->system, &target->idle, &target->iowait,
               &target->irq, &target->softirq);

    if (scanned != CPU_JIFFIES_FIELD_COUNT)
      continue;

    parsed_count++;
  }

  fclose(stat_fp);
  return (parsed_count > 0) ? SPKT_OK : SPKT_ERR_CPU_PARSE_FAILED;
}

// helper to calculate total jiffies
static inline unsigned long long total_jiffies(const struct cpu_jiffies *j) {
  return j->user + j->nice + j->system + j->idle + j->iowait + j->irq +
         j->softirq;
}

// helper to calculate idle jiffies
static inline unsigned long long idle_jiffies(const struct cpu_jiffies *j) {
  return j->idle + j->iowait;
}

spkt_status_t cpu_calc_usage_pct_batch(const struct cpu_jiffies *old_jiffies,
                                       const struct cpu_jiffies *new_jiffies,
                                       int num_cores, double *out_usage) {
  if (!old_jiffies || !new_jiffies || !out_usage || num_cores <= 0) {
    return SPKT_ERR_INVALID_PARAM;
  }

  for (int i = 1; i <= num_cores; i++) {
    unsigned long long total_new = total_jiffies(&new_jiffies[i]);
    unsigned long long total_old = total_jiffies(&old_jiffies[i]);

    unsigned long long idle_new = idle_jiffies(&new_jiffies[i]);
    unsigned long long idle_old = idle_jiffies(&old_jiffies[i]);

    unsigned long long total_delta = total_new - total_old;
    unsigned long long idle_delta = idle_new - idle_old;

    if (total_delta == 0 || idle_delta > total_delta) {
      out_usage[i - 1] = 0.0;
      continue;
    }

    double idle_ratio = (double)idle_delta / total_delta;
    out_usage[i - 1] = (1.0 - idle_ratio) * 100.0;
  }

  return SPKT_OK;
}
