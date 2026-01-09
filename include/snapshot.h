#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <stdint.h>

#define MAX_CORES 64
#define MAX_PROCS 10

/* CPU usage snapshot data */
typedef struct {
  double global_usage_pct;
  double per_core_usage_pct[MAX_CORES];
  uint16_t valid_core_count;
} spkt_cpu_snapshot_t;

/* Memory usage snapshot data (values in KiB) */
typedef struct {
  uint64_t total_ram_kib;
  uint64_t available_ram_kib;
  uint64_t free_ram_kib;
  uint64_t active_ram_kib;
  uint64_t inactive_ram_kib;
  uint64_t dirty_ram_kib;
  uint64_t slab_ram_kib;
  uint64_t swap_total_ram_kib;
  uint64_t swap_free_ram_kib;
  uint64_t shmem_ram_kib;
} spkt_mem_snapshot_t;

/* Single process entry in snapshot */
typedef struct {
  int32_t pid;
  char comm[16];
  double cpu_usage_pct;
  uint64_t rss_kib;
} spkt_proc_entry_t;

/* Top processes snapshot data */
typedef struct {
  spkt_proc_entry_t entries[MAX_PROCS];
  uint32_t valid_entry_count;
} spkt_proc_snapshot_t;

/* Complete system snapshot at a point in time */
typedef struct {
  uint64_t timestamp_monotonic_ns;

  spkt_cpu_snapshot_t cpu;
  spkt_mem_snapshot_t mem;
  spkt_proc_snapshot_t procs;

} spkt_snapshot_t;

#endif
