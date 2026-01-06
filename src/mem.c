#include "mem.h"
#include "spkt_common.h"
#include <stdio.h>
#include <string.h>

#define PROC_MEMINFO_PATH "/proc/meminfo"
#define MEMINFO_BUFF 512

/* Reads /proc/meminfo to populate memory snapshot */
spkt_status_t mem_read_kibibytes(struct meminfo *kibibytes) {
  if (!kibibytes) {
    return SPKT_ERR_INVALID_PARAM;
  }

  FILE *meminfo_fp = fopen(PROC_MEMINFO_PATH, "r");
  if (!meminfo_fp) {
    return SPKT_ERR_MEM_OPEN_MEMINFO;
  }

  // Zero-initialize to avoid uninitialized fields when parsing /proc/meminfo
  *kibibytes = (struct meminfo){0};

  struct {
    const char *label;
    unsigned long long *dest;
  } lookup[] = {{"MemTotal:", &kibibytes->total},
                {"MemAvailable:", &kibibytes->available},
                {"MemFree:", &kibibytes->free},
                {"Active:", &kibibytes->active},
                {"Inactive:", &kibibytes->inactive},
                {"Dirty:", &kibibytes->dirty},
                {"Slab:", &kibibytes->slab},
                {"SwapTotal:", &kibibytes->swap_total},
                {"SwapFree:", &kibibytes->swap_free},
                {"Shmem:", &kibibytes->shmem}};

  char buf[MEMINFO_BUFF];
  const int num_fields = sizeof(lookup) / sizeof(lookup[0]);

  while (fgets(buf, sizeof(buf), meminfo_fp)) {
    for (int i = 0; i < num_fields; i++) {

      if (strncmp(buf, lookup[i].label, strlen(lookup[i].label)) != 0) {
        continue;
      }

      if (sscanf(buf, "%*s %llu", lookup[i].dest) != 1) {
        fclose(meminfo_fp);
        return SPKT_ERR_MEM_PARSE_FAILED;
      }
      break;
    }
  }

  fclose(meminfo_fp);
  return SPKT_OK;
}
