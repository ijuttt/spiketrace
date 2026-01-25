#ifndef CONFIG_H
#define CONFIG_H

#include "spkt_common.h"

#include <stdbool.h>
#include <stdint.h>

/* Maximum config file size (64 KiB) */
#define CONFIG_MAX_FILE_SIZE (64 * 1024)

/* Maximum path length for config and output directories */
#define CONFIG_MAX_PATH_LEN 256

/* Config file version */
#define CONFIG_VERSION 1

/* Configuration structure holding all user-configurable values */
typedef struct {
  /* Anomaly detection thresholds */
  double cpu_delta_threshold_pct;
  double new_process_threshold_pct;
  uint64_t mem_drop_threshold_kib;
  double mem_pressure_threshold_pct;
  uint64_t swap_spike_threshold_kib;
  double cooldown_seconds;

  /* Sampling configuration */
  double sampling_interval_seconds;
  uint32_t ring_buffer_capacity;
  uint32_t context_snapshots_per_dump;

  /* Process collection */
  uint32_t max_processes_tracked;
  uint32_t top_processes_stored;

  /* Output configuration */
  char output_directory[CONFIG_MAX_PATH_LEN];

  /* Feature toggles */
  bool enable_cpu_detection;
  bool enable_memory_detection;
  bool enable_swap_detection;

  /* Advanced tuning */
  double memory_baseline_alpha;
  double process_baseline_alpha;

  /* Internal: config loaded flag */
  bool loaded;
} spkt_config_t;

/* Initialize config with built-in defaults */
void config_init_defaults(spkt_config_t *config);

/* Load config from file path (NULL = use default location) */
spkt_status_t config_load(spkt_config_t *config, const char *config_path);

/* Validate config values and clamp to safe ranges */
spkt_status_t config_validate(spkt_config_t *config);

/* Get default config file path (~/.config/spiketrace/config.toml) */
spkt_status_t config_get_default_path(char *path, size_t path_size);

/* Check if config file exists at path */
bool config_file_exists(const char *path);

#endif
