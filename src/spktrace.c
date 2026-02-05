/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#define _POSIX_C_SOURCE 200809L

#include "anomaly_detector.h"
#include "config.h"
#include "proc.h"
#include "ringbuf.h"
#include "snapshot.h"
#include "snapshot_builder.h"
#include "spike_dump.h"
#include "spkt_common.h"

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/* Maximum context snapshots (must match ring buffer capacity) */
#define MAX_CONTEXT_SNAPSHOTS 60

/* Shutdown flag set by signal handler */
static volatile sig_atomic_t shutdown_requested = 0;

/* Config reload flag set by SIGHUP handler */
static volatile sig_atomic_t config_reload_requested = 0;

/* Active config (protected by mutex for SIGHUP reload) */
static spkt_config_t active_config;
static pthread_mutex_t config_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Signal handler for shutdown */
static void signal_handler(int sig) {
  (void)sig; /* Unused parameter */
  shutdown_requested = 1;
}

/* Signal handler for config reload */
static void sighup_handler(int sig) {
  (void)sig; /* Unused parameter */
  config_reload_requested = 1;
}

static int install_signal_handlers(void) {
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGTERM, &sa, NULL) != 0) {
    return -1;
  }
  if (sigaction(SIGINT, &sa, NULL) != 0) {
    return -1;
  }

  /* Install SIGHUP handler for config reload */
  sa.sa_handler = sighup_handler;
  if (sigaction(SIGHUP, &sa, NULL) != 0) {
    return -1;
  }

  return 0;
}

/* Load and validate config */
static spkt_status_t load_config(spkt_config_t *config) {
  spkt_status_t s = config_load(config, NULL);
  if (s != SPKT_OK) {
    return s;
  }

  s = config_validate(config);
  return s;
}

/* Reload config atomically (called from main loop on SIGHUP) */
static void reload_config(void) {
  spkt_config_t new_config;
  spkt_status_t s = load_config(&new_config);
  if (s != SPKT_OK) {
    fprintf(stderr, "spiketrace: config reload failed, keeping current config\n");
    return;
  }

  pthread_mutex_lock(&config_mutex);
  active_config = new_config;
  pthread_mutex_unlock(&config_mutex);

  fprintf(stderr, "spiketrace: config reloaded successfully\n");
}

/* Convert spkt_config_t to anomaly_config_t */
static anomaly_config_t config_to_anomaly_config(const spkt_config_t *config) {
  anomaly_config_t anomaly_config = {0};
  anomaly_config.cpu_delta_threshold_pct = config->cpu_delta_threshold_pct;
  anomaly_config.new_process_threshold_pct = config->new_process_threshold_pct;
  anomaly_config.mem_drop_threshold_kib = config->mem_drop_threshold_kib;
  anomaly_config.mem_pressure_threshold_pct = config->mem_pressure_threshold_pct;
  anomaly_config.swap_spike_threshold_kib = config->swap_spike_threshold_kib;
  anomaly_config.cooldown_ns =
      (uint64_t)(config->cooldown_seconds * 1000000000.0);
  anomaly_config.memory_baseline_alpha = config->memory_baseline_alpha;
  anomaly_config.trigger_scope = config->trigger_scope;
  anomaly_config.aggregate_related_processes = config->aggregate_related_processes;
  return anomaly_config;
}

/* Format scope context string for logging (empty for per_process) */
static void format_scope_context(char *buf, size_t buf_size,
                                 spkt_trigger_scope_t scope, int32_t scope_key) {
  switch (scope) {
  case TRIGGER_SCOPE_PROCESS_GROUP:
    snprintf(buf, buf_size, " (Group %d)", scope_key);
    break;
  case TRIGGER_SCOPE_PARENT:
    snprintf(buf, buf_size, " (Parent %d)", scope_key);
    break;
  case TRIGGER_SCOPE_SYSTEM:
    snprintf(buf, buf_size, " (System)");
    break;
  case TRIGGER_SCOPE_PROCESS:
  default:
    buf[0] = '\0';
    break;
  }
}

int main(void) {
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cores <= 0 || num_cores > MAX_CORES) {
    fprintf(stderr, "spiketrace: invalid core count\n");
    return 1;
  }

  if (install_signal_handlers() != 0) {
    fprintf(stderr, "spiketrace: failed to install signal handlers\n");
    return 1;
  }

  /* Load initial config */
  if (load_config(&active_config) != SPKT_OK) {
    fprintf(stderr, "spiketrace: config load failed, using defaults\n");
    config_init_defaults(&active_config);
  }

  snapshot_builder_t builder;
  if (snapshot_builder_init(&builder, num_cores) != SPKT_OK) {
    fprintf(stderr, "spiketrace: snapshot builder init failed\n");
    return 1;
  }

  /* Set process baseline alpha from config */
  pthread_mutex_lock(&config_mutex);
  snapshot_builder_set_baseline_alpha(&builder,
                                     active_config.process_baseline_alpha);
  snapshot_builder_set_top_processes_limit(&builder,
                                           active_config.top_processes_stored);
  pthread_mutex_unlock(&config_mutex);

  ringbuffer_t rb;
  if (ringbuf_init(&rb) != SPKT_OK) {
    fprintf(stderr, "spiketrace: ring buffer init failed\n");
    snapshot_builder_cleanup(&builder);
    return 1;
  }

  anomaly_state_t anomaly_state;
  anomaly_state_init(&anomaly_state);

  spike_dump_ctx_t dump_ctx;
  bool dumps_enabled = false;
  pthread_mutex_lock(&config_mutex);
  if (strlen(active_config.output_directory) > 0) {
    dumps_enabled =
        (spike_dump_init(&dump_ctx, active_config.output_directory) == SPKT_OK);
  } else {
    dumps_enabled = (spike_dump_init(&dump_ctx, NULL) == SPKT_OK);
  }
  pthread_mutex_unlock(&config_mutex);

  if (!dumps_enabled) {
    fprintf(stderr, "spiketrace: spike dumps disabled (init failed)\n");
  }

  fprintf(stderr, "spiketrace: started (pid=%d)\n", getpid());

  /* Allocate buffer for context snapshots (max size) */
  spkt_snapshot_t *dump_snaps = malloc(MAX_CONTEXT_SNAPSHOTS * sizeof(spkt_snapshot_t));
  if (!dump_snaps) {
    fprintf(stderr, "spiketrace: out of memory\n");
    if (dumps_enabled) {
      spike_dump_cleanup(&dump_ctx);
    }
    snapshot_builder_cleanup(&builder);
    ringbuf_cleanup(&rb);
    return 1;
  }

  while (!shutdown_requested) {
    /* Check for config reload */
    if (config_reload_requested) {
      config_reload_requested = 0;
      reload_config();
      /* Reset anomaly state on reload */
      anomaly_state_init(&anomaly_state);
      /* Update process baseline alpha */
      pthread_mutex_lock(&config_mutex);
      snapshot_builder_set_baseline_alpha(&builder,
                                         active_config.process_baseline_alpha);
      snapshot_builder_set_top_processes_limit(&builder,
                                              active_config.top_processes_stored);
      pthread_mutex_unlock(&config_mutex);
    }

    /* Get current config values (protected by mutex) */
    pthread_mutex_lock(&config_mutex);
    double sampling_interval = active_config.sampling_interval_seconds;
    uint32_t context_size = active_config.context_snapshots_per_dump;
    anomaly_config_t anomaly_config = config_to_anomaly_config(&active_config);
    bool enable_cpu = active_config.enable_cpu_detection;
    bool enable_memory = active_config.enable_memory_detection;
    bool enable_swap = active_config.enable_swap_detection;
    pthread_mutex_unlock(&config_mutex);

    /* Sleep for configured interval */
    if (sampling_interval >= 1.0) {
      sleep((unsigned int)sampling_interval);
    } else {
      /* Sub-second sleep using nanosleep */
      struct timespec ts;
      ts.tv_sec = (time_t)sampling_interval;
      ts.tv_nsec = (long)((sampling_interval - (double)ts.tv_sec) * 1000000000.0);
      nanosleep(&ts, NULL);
    }

    if (shutdown_requested) {
      break;
    }

    spkt_snapshot_t snap;
    if (snapshot_builder_collect(&builder, &snap) != SPKT_OK) {
      continue;
    }

    /* ringbuf drops oldest when full (circular buffer) */
    ringbuf_push(&rb, &snap);

    /* ===== ANOMALY DETECTION (CPU + MEMORY) ===== */
    size_t sample_count = 0;
    const proc_sample_t *samples =
        snapshot_builder_get_proc_samples(&builder, &sample_count);

    anomaly_result_t result = {0};
    if (enable_cpu || enable_memory || enable_swap) {
      /* Only evaluate if at least one detection type is enabled */
      result = anomaly_evaluate(&anomaly_config, &anomaly_state, samples,
                                sample_count, &snap.mem,
                                snap.timestamp_monotonic_ns);

      /* Filter by enabled detection types */
      if (result.has_anomaly) {
        bool should_report = false;
        if (enable_cpu &&
            (result.type == ANOMALY_TYPE_CPU_DELTA ||
             result.type == ANOMALY_TYPE_CPU_NEW_PROC)) {
          should_report = true;
        }
        if (enable_memory &&
            (result.type == ANOMALY_TYPE_MEM_DROP ||
             result.type == ANOMALY_TYPE_MEM_PRESSURE)) {
          should_report = true;
        }
        if (enable_swap && result.type == ANOMALY_TYPE_SWAP_SPIKE) {
          should_report = true;
        }

        if (!should_report) {
          result.has_anomaly = false;
        }
      }
    }

    if (anomaly_should_dump(&result)) {
      /* Format scope context for logging */
      char scope_ctx[32] = "";
      format_scope_context(scope_ctx, sizeof(scope_ctx),
                           result.trigger_scope, result.scope_key);

      /* Log anomaly type */
      switch (result.type) {
      case ANOMALY_TYPE_CPU_DELTA:
        fprintf(stderr,
                "spiketrace: [ANOMALY] CPU DELTA%s: [%d] %s  "
                "CPU: %.1f%% (baseline: %.1f%%, delta: +%.1f%%)\n",
                scope_ctx, result.spike_pid, result.spike_comm, result.spike_cpu_pct,
                result.spike_baseline_pct, result.spike_delta);
        break;
      case ANOMALY_TYPE_CPU_NEW_PROC:
        fprintf(stderr,
                "spiketrace: [ANOMALY] NEW PROCESS%s: [%d] %s  CPU: %.1f%%\n",
                scope_ctx, result.spike_pid, result.spike_comm, result.spike_cpu_pct);
        break;
      case ANOMALY_TYPE_MEM_DROP:
        fprintf(stderr,
                "spiketrace: [ANOMALY] MEM DROP%s by [%d] %s: available: %lu MiB "
                "(baseline: %lu MiB, delta: %ld MiB)\n",
                scope_ctx, result.spike_pid, result.spike_comm,
                (unsigned long)(result.mem_available_kib / 1024),
                (unsigned long)(result.mem_baseline_kib / 1024),
                (long)(result.mem_delta_kib / 1024));
        break;
      case ANOMALY_TYPE_MEM_PRESSURE:
        fprintf(stderr,
                "spiketrace: [ANOMALY] MEM PRESSURE%s: [%d] %s top RSS, %.1f%% "
                "used (available: %lu MiB)\n",
                scope_ctx, result.spike_pid, result.spike_comm,
                result.mem_used_pct,
                (unsigned long)(result.mem_available_kib / 1024));
        break;
      case ANOMALY_TYPE_SWAP_SPIKE:
        fprintf(stderr,
                "spiketrace: [ANOMALY] SWAP SPIKE%s by [%d] %s: used: %lu MiB "
                "(baseline: %lu MiB, delta: +%ld MiB)\n",
                scope_ctx, result.spike_pid, result.spike_comm,
                (unsigned long)(result.swap_used_kib / 1024),
                (unsigned long)(result.swap_baseline_kib / 1024),
                (long)(result.swap_delta_kib / 1024));
        break;
      default:
        break;
      }

      if (dumps_enabled) {
        /* Extract recent snapshots and write dump */
        size_t dump_count = 0;
        uint32_t actual_context_size = context_size;
        if (actual_context_size > MAX_CONTEXT_SNAPSHOTS) {
          actual_context_size = MAX_CONTEXT_SNAPSHOTS;
        }
        ringbuf_get_recent(&rb, dump_snaps, actual_context_size, &dump_count);

        if (dump_count > 0) {
          spkt_status_t dump_status =
              spike_dump_write(&dump_ctx, dump_snaps, dump_count, &result,
                               snap.timestamp_monotonic_ns);
          if (dump_status != SPKT_OK) {
            fprintf(stderr, "spiketrace: dump write failed (error %d)\n",
                    dump_status);
          }
        }
      }
    }
  }

  fprintf(stderr, "spiketrace: shutting down\n");

  free(dump_snaps);
  if (dumps_enabled) {
    spike_dump_cleanup(&dump_ctx);
  }
  snapshot_builder_cleanup(&builder);
  ringbuf_cleanup(&rb);

  fprintf(stderr, "spiketrace: stopped\n");
  return 0;
}
