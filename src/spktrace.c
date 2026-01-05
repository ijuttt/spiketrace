#include "cpu.h"
#include "spkt_common.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  int max_cores = num_cores + 1; // +1 for aggregate total

  struct cpu_jiffies jiffies[max_cores];

  if (cpu_read_jiffies(jiffies, max_cores) != SPKT_OK) {
    fprintf(stderr, "Failed to read CPU jiffies from /proc/stat\n");
    return 1;
  }

  // Print results for verification
  for (int i = 0; i < max_cores; i++) {
    printf("[%d] %s\n", i, (i == 0) ? "TOTAL" : "CORE");
    printf("  user=%llu nice=%llu system=%llu idle=%llu\n", jiffies[i].user,
           jiffies[i].nice, jiffies[i].system, jiffies[i].idle);
    printf("  iowait=%llu irq=%llu softirq=%llu\n", jiffies[i].iowait,
           jiffies[i].irq, jiffies[i].softirq);
  }

  return 0;
}
