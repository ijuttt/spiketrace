#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include "proc.h"
#include "snapshot.h"

#include <stdbool.h>
#include <stdint.h>

/* ===== CONFIGURATION ===== */

/* CPU anomaly thresholds */
#define ANOMALY_DEFAULT_CPU_DELTA_THRESHOLD 10.0      /* % jump from baseline */
#define ANOMALY_DEFAULT_NEW_PROCESS_THRESHOLD 5.0     /* % for new process */

/* Memory anomaly thresholds */
#define ANOMALY_DEFAULT_MEM_DROP_THRESHOLD_MIB 512    /* MiB sudden drop */
#define ANOMALY_DEFAULT_MEM_PRESSURE_THRESHOLD_PCT 90 /* % used triggers alert */

/* Swap anomaly threshold */
#define ANOMALY_DEFAULT_SWAP_SPIKE_THRESHOLD_MIB 256  /* MiB sudden swap usage */

/* Cooldown */
#define ANOMALY_DEFAULT_COOLDOWN_NS (5ULL * 1000000000ULL) /* 5 seconds */

/* Per-PID cooldown table size */
#define ANOMALY_COOLDOWN_TABLE_SIZE 64

/* Memory baseline smoothing factor (lower = more stable) */
#define ANOMALY_MEM_BASELINE_ALPHA 0.2

/* ===== TYPES ===== */

/* Anomaly type enum */
typedef enum {
  ANOMALY_TYPE_NONE = 0,
  ANOMALY_TYPE_CPU_DELTA,      /* Process CPU jumped from baseline */
  ANOMALY_TYPE_CPU_NEW_PROC,   /* New process with high CPU */
  ANOMALY_TYPE_MEM_DROP,       /* Sudden drop in available RAM */
  ANOMALY_TYPE_MEM_PRESSURE,   /* Available RAM below threshold */
  ANOMALY_TYPE_SWAP_SPIKE,     /* Sudden increase in swap usage */
} anomaly_type_t;

/* Configuration */
typedef struct {
  /* CPU thresholds */
  double cpu_delta_threshold_pct;
  double new_process_threshold_pct;

  /* Memory thresholds */
  uint64_t mem_drop_threshold_kib;
  double mem_pressure_threshold_pct;

  /* Swap threshold */
  uint64_t swap_spike_threshold_kib;

  /* Cooldown */
  uint64_t cooldown_ns;
} anomaly_config_t;

/* Result of anomaly evaluation */
typedef struct {
  bool has_anomaly;
  anomaly_type_t type;

  /* CPU anomaly details (valid when type is CPU_*) */
  int32_t spike_pid;
  char spike_comm[16];
  double spike_cpu_pct;
  double spike_baseline_pct;
  double spike_delta;
  bool is_new_process_spike;

  /* Memory anomaly details (valid when type is MEM_* or SWAP_*) */
  uint64_t mem_available_kib;
  uint64_t mem_baseline_kib;
  int64_t mem_delta_kib;
  double mem_used_pct;

  /* Swap anomaly details (valid when type is SWAP_*) */
  uint64_t swap_used_kib;
  uint64_t swap_baseline_kib;
  int64_t swap_delta_kib;
} anomaly_result_t;

/* Single entry in per-PID cooldown table */
typedef struct {
  int32_t pid;
  uint64_t last_trigger_ns;
} cooldown_entry_t;

/* Detector state */
typedef struct {
  /* Per-PID cooldown tracking */
  cooldown_entry_t cooldowns[ANOMALY_COOLDOWN_TABLE_SIZE];
  size_t cooldown_count;

  /* Memory baseline (EMA) */
  uint64_t mem_baseline_kib;
  bool mem_baseline_initialized;
  uint64_t last_mem_trigger_ns;

  /* Swap baseline (EMA) */
  uint64_t swap_baseline_kib;
  bool swap_baseline_initialized;
  uint64_t last_swap_trigger_ns;
} anomaly_state_t;

/* ===== FUNCTIONS ===== */

/* Initialize detector state */
void anomaly_state_init(anomaly_state_t *state);

/* Get default configuration */
anomaly_config_t anomaly_default_config(void);

/* Evaluate for anomalies (CPU + memory)
 * Returns the most severe anomaly found (CPU takes priority if both)
 */
anomaly_result_t anomaly_evaluate(const anomaly_config_t *config,
                                  anomaly_state_t *state,
                                  const proc_sample_t *proc_samples,
                                  size_t proc_sample_count,
                                  const spkt_mem_snapshot_t *mem,
                                  uint64_t current_timestamp_ns);

/* Check if any anomaly was detected */
bool anomaly_should_dump(const anomaly_result_t *result);

/* Legacy wrapper for backward compatibility */
anomaly_result_t anomaly_evaluate_procs(const anomaly_config_t *config,
                                        anomaly_state_t *state,
                                        const proc_sample_t *samples,
                                        size_t sample_count,
                                        uint64_t current_timestamp_ns);

#endif
