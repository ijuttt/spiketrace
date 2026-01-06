#ifndef MEM_H
#define MEM_H

#include "spkt_common.h"

/* Memory statistics in KiB from proc/meminfo */
struct meminfo {
  unsigned long long total;
  unsigned long long available;
  unsigned long long free;
  unsigned long long active;
  unsigned long long inactive;
  unsigned long long dirty;
  unsigned long long slab;
  unsigned long long swap_total;
  unsigned long long swap_free;
  unsigned long long shmem;
};

/* Reads /proc/meminfo to populate memory snapshot */
spkt_status_t mem_read_kibibytes(struct meminfo *kibibytes);

#endif
