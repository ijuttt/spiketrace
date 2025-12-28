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
    perror("fopen");
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
    } else if (isdigit(label[3])) {
      int core = atoi(label + 3);
      if (core + 1 >= max_cores)
        continue;
      s = &curr[core + 1];
    } else {
      continue;
    }

    if (!s)
      continue;
    int scanned = sscanf(buf, "%*s %llu %llu %llu %llu %llu %llu %llu",
                         &s->user, &s->nice, &s->system, &s->idle, &s->iowait,
                         &s->irq, &s->softirq);
    if (scanned != 7)
      continue;
    cores_found++;
  }

  fclose(stat_fp);
  return (cores_found > 0) ? SPKT_OK : SPKT_ERR_GET_CPU_STAT;
}
