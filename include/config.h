/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#ifndef CONFIG_H
#define CONFIG_H

#include "spkt_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Maximum config file size (64 KiB) */
#define CONFIG_MAX_FILE_SIZE (64 * 1024)

/* Maximum path length for config and output directories */
#define CONFIG_MAX_PATH_LEN 256

/* Config file version */
#define CONFIG_VERSION 1

/* System-wide config path (for daemons running as root)
 * Can be overridden at build time via -DCONFIG_SYSTEM_PATH=\"...\" */
#ifndef CONFIG_SYSTEM_PATH
#define CONFIG_SYSTEM_PATH "/etc/spiketrace/config.toml"
#endif

/* Trigger scope for grouping anomaly cooldowns */
typedef enum {
  TRIGGER_SCOPE_PROCESS = 0,
  TRIGGER_SCOPE_PROCESS_GROUP,
  TRIGGER_SCOPE_PARENT,
  TRIGGER_SCOPE_SYSTEM,
} spkt_trigger_scope_t;

/* Log cleanup policy */
typedef enum {
  LOG_CLEANUP_DISABLED = 0, /* No automatic cleanup */
  LOG_CLEANUP_BY_AGE,       /* Delete logs older than N days */
  LOG_CLEANUP_BY_COUNT,     /* Keep only N most recent logs */
  LOG_CLEANUP_BY_SIZE,      /* Delete when total size exceeds N MiB */
} log_cleanup_policy_t;

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
  bool aggregate_related_processes;

  /* Advanced tuning */
  double memory_baseline_alpha;
  double process_baseline_alpha;

  /* Trigger policy */
  spkt_trigger_scope_t trigger_scope;

  /* Log management configuration */
  bool enable_auto_cleanup;            /* Enable automatic log cleanup */
  log_cleanup_policy_t cleanup_policy; /* Which cleanup policy to use */
  uint32_t log_max_age_days; /* For BY_AGE: delete logs older than N days */
  uint32_t log_max_count;    /* For BY_COUNT: keep only N most recent logs */
  uint32_t log_max_total_size_mib;   /* For BY_SIZE: max total size in MiB */
  uint32_t cleanup_interval_minutes; /* How often to run cleanup check */

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

/* Convert log_cleanup_policy_t to string (for config parsing) */
const char *log_cleanup_policy_to_string(log_cleanup_policy_t policy);

/* Parse string to log_cleanup_policy_t (returns LOG_CLEANUP_DISABLED on error)
 */
log_cleanup_policy_t log_cleanup_policy_from_string(const char *str);

#endif
