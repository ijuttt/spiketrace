#define _POSIX_C_SOURCE 200809L

#include "anomaly_detector.h"

#include <string.h>

/* ===== INTERNAL: COOLDOWN TABLE ===== */

static cooldown_entry_t *cooldown_find(anomaly_state_t *state, int32_t pid) {
  for (size_t i = 0; i < state->cooldown_count; i++) {
    if (state->cooldowns[i].pid == pid) {
      return &state->cooldowns[i];
    }
  }
  return NULL;
}

static bool cooldown_is_active(anomaly_state_t *state, int32_t pid,
                               uint64_t current_ns, uint64_t cooldown_ns) {
  cooldown_entry_t *entry = cooldown_find(state, pid);
  if (!entry) {
    return false;
  }

  if (current_ns <= entry->last_trigger_ns) {
    return true;
  }

  uint64_t elapsed = current_ns - entry->last_trigger_ns;
  if (elapsed < cooldown_ns) {
    return true;
  }

  /* Expired - remove by swap with last */
  if (state->cooldown_count > 1) {
    *entry = state->cooldowns[state->cooldown_count - 1];
  }
  state->cooldown_count--;
  return false;
}

static void cooldown_record(anomaly_state_t *state, int32_t pid,
                            uint64_t timestamp_ns) {
  cooldown_entry_t *entry = cooldown_find(state, pid);
  if (entry) {
    entry->last_trigger_ns = timestamp_ns;
    return;
  }

  if (state->cooldown_count >= ANOMALY_COOLDOWN_TABLE_SIZE) {
    /* Evict oldest */
    size_t oldest_idx = 0;
    uint64_t oldest_ts = state->cooldowns[0].last_trigger_ns;
    for (size_t i = 1; i < state->cooldown_count; i++) {
      if (state->cooldowns[i].last_trigger_ns < oldest_ts) {
        oldest_ts = state->cooldowns[i].last_trigger_ns;
        oldest_idx = i;
      }
    }
    state->cooldowns[oldest_idx].pid = pid;
    state->cooldowns[oldest_idx].last_trigger_ns = timestamp_ns;
    return;
  }

  state->cooldowns[state->cooldown_count].pid = pid;
  state->cooldowns[state->cooldown_count].last_trigger_ns = timestamp_ns;
  state->cooldown_count++;
}

/* ===== PUBLIC API ===== */

void anomaly_state_init(anomaly_state_t *state) {
  if (!state) {
    return;
  }
  memset(state, 0, sizeof(*state));
}

anomaly_config_t anomaly_default_config(void) {
  return (anomaly_config_t){
      .cpu_delta_threshold_pct = ANOMALY_DEFAULT_CPU_DELTA_THRESHOLD,
      .new_process_threshold_pct = ANOMALY_DEFAULT_NEW_PROCESS_THRESHOLD,
      .mem_drop_threshold_kib = ANOMALY_DEFAULT_MEM_DROP_THRESHOLD_MIB * 1024,
      .mem_pressure_threshold_pct = ANOMALY_DEFAULT_MEM_PRESSURE_THRESHOLD_PCT,
      .swap_spike_threshold_kib = ANOMALY_DEFAULT_SWAP_SPIKE_THRESHOLD_MIB * 1024,
      .cooldown_ns = ANOMALY_DEFAULT_COOLDOWN_NS,
      .memory_baseline_alpha = ANOMALY_MEM_BASELINE_ALPHA,
  };
}

/* Evaluate CPU anomalies (per-process) */
static void evaluate_cpu(const anomaly_config_t *config,
                         anomaly_state_t *state,
                         const proc_sample_t *samples,
                         size_t sample_count,
                         uint64_t current_ns,
                         anomaly_result_t *out) {
  double max_spike = 0.0;
  const proc_sample_t *worst = NULL;
  bool is_new = false;

  for (size_t i = 0; i < sample_count; i++) {
    const proc_sample_t *s = &samples[i];
    if (!s->valid) {
      continue;
    }

    if (cooldown_is_active(state, s->pid, current_ns, config->cooldown_ns)) {
      continue;
    }

    /* New process spike */
    if (s->sample_count <= 2 &&
        s->cpu_pct >= config->new_process_threshold_pct) {
      if (s->cpu_pct > max_spike) {
        max_spike = s->cpu_pct;
        worst = s;
        is_new = true;
      }
    }

    /* Delta spike */
    if (s->sample_count > 2) {
      double delta = s->cpu_pct - s->baseline_cpu_pct;
      if (delta >= config->cpu_delta_threshold_pct && delta > max_spike) {
        max_spike = delta;
        worst = s;
        is_new = false;
      }
    }
  }

  if (worst) {
    out->has_anomaly = true;
    out->type = is_new ? ANOMALY_TYPE_CPU_NEW_PROC : ANOMALY_TYPE_CPU_DELTA;
    out->spike_pid = worst->pid;
    strncpy(out->spike_comm, worst->comm, sizeof(out->spike_comm) - 1);
    out->spike_comm[sizeof(out->spike_comm) - 1] = '\0';
    out->spike_cpu_pct = worst->cpu_pct;
    out->spike_baseline_pct = worst->baseline_cpu_pct;
    out->spike_delta = worst->cpu_pct - worst->baseline_cpu_pct;
    out->is_new_process_spike = is_new;

    cooldown_record(state, worst->pid, current_ns);
  }
}

/* Evaluate memory anomalies */
static void evaluate_mem(const anomaly_config_t *config,
                         anomaly_state_t *state,
                         const spkt_mem_snapshot_t *mem,
                         const proc_sample_t *proc_samples,
                         size_t proc_sample_count,
                         uint64_t current_ns,
                         anomaly_result_t *out) {
  if (!mem || mem->total_ram_kib == 0) {
    return;
  }

  uint64_t available = mem->available_ram_kib;
  uint64_t total = mem->total_ram_kib;
  double used_pct = 100.0 * (double)(total - available) / (double)total;

  /* Update baseline (EMA) */
  if (!state->mem_baseline_initialized) {
    state->mem_baseline_kib = available;
    state->mem_baseline_initialized = true;
    return; /* First sample - no detection */
  }

  uint64_t baseline = state->mem_baseline_kib;

  /* Check cooldown */
  if (state->last_mem_trigger_ns > 0 && current_ns > state->last_mem_trigger_ns) {
    uint64_t elapsed = current_ns - state->last_mem_trigger_ns;
    if (elapsed < config->cooldown_ns) {
      /* Still in cooldown - just update baseline */
      state->mem_baseline_kib = (uint64_t)(
          config->memory_baseline_alpha * (double)available +
          (1.0 - config->memory_baseline_alpha) * (double)baseline);
      return;
    }
  }

  /* Check for sudden drop */
  int64_t delta = (int64_t)available - (int64_t)baseline;
  bool is_drop = (delta < 0) &&
                 ((uint64_t)(-delta) >= config->mem_drop_threshold_kib);

  /* Check for pressure threshold */
  bool is_pressure = (used_pct >= config->mem_pressure_threshold_pct);

  if (is_drop || is_pressure) {
    out->has_anomaly = true;
    out->type = is_drop ? ANOMALY_TYPE_MEM_DROP : ANOMALY_TYPE_MEM_PRESSURE;
    out->mem_available_kib = available;
    out->mem_baseline_kib = baseline;
    out->mem_delta_kib = delta;
    out->mem_used_pct = used_pct;

    /* Find top RSS consumer to attribute the memory spike */
    if (proc_samples && proc_sample_count > 0) {
      const proc_sample_t *top_rss = NULL;
      uint64_t max_rss = 0;

      for (size_t i = 0; i < proc_sample_count; i++) {
        const proc_sample_t *s = &proc_samples[i];
        if (s->valid && s->rss_kib > max_rss) {
          max_rss = s->rss_kib;
          top_rss = s;
        }
      }

      if (top_rss) {
        out->spike_pid = top_rss->pid;
        strncpy(out->spike_comm, top_rss->comm, sizeof(out->spike_comm) - 1);
        out->spike_comm[sizeof(out->spike_comm) - 1] = '\0';
      }
    }

    state->last_mem_trigger_ns = current_ns;
  }

  /* Update baseline */
  state->mem_baseline_kib = (uint64_t)(
      config->memory_baseline_alpha * (double)available +
      (1.0 - config->memory_baseline_alpha) * (double)baseline);
}

/* Evaluate swap anomalies */
static void evaluate_swap(const anomaly_config_t *config,
                          anomaly_state_t *state,
                          const spkt_mem_snapshot_t *mem,
                          const proc_sample_t *proc_samples,
                          size_t proc_sample_count,
                          uint64_t current_ns,
                          anomaly_result_t *out) {
  if (!mem || mem->swap_total_ram_kib == 0) {
    return;
  }

  uint64_t swap_used = mem->swap_total_ram_kib - mem->swap_free_ram_kib;

  /* Initialize baseline */
  if (!state->swap_baseline_initialized) {
    state->swap_baseline_kib = swap_used;
    state->swap_baseline_initialized = true;
    return;
  }

  uint64_t baseline = state->swap_baseline_kib;

  /* Check cooldown */
  if (state->last_swap_trigger_ns > 0 && current_ns > state->last_swap_trigger_ns) {
    uint64_t elapsed = current_ns - state->last_swap_trigger_ns;
    if (elapsed < config->cooldown_ns) {
      state->swap_baseline_kib = (uint64_t)(
          config->memory_baseline_alpha * (double)swap_used +
          (1.0 - config->memory_baseline_alpha) * (double)baseline);
      return;
    }
  }

  /* Detect sudden swap usage increase */
  int64_t delta = (int64_t)swap_used - (int64_t)baseline;
  bool is_spike = (delta > 0) &&
                  ((uint64_t)delta >= config->swap_spike_threshold_kib);

  if (is_spike) {
    out->has_anomaly = true;
    out->type = ANOMALY_TYPE_SWAP_SPIKE;
    out->swap_used_kib = swap_used;
    out->swap_baseline_kib = baseline;
    out->swap_delta_kib = delta;

    /* Attribute to top RSS consumer */
    if (proc_samples && proc_sample_count > 0) {
      const proc_sample_t *top_rss = NULL;
      uint64_t max_rss = 0;
      for (size_t i = 0; i < proc_sample_count; i++) {
        const proc_sample_t *s = &proc_samples[i];
        if (s->valid && s->rss_kib > max_rss) {
          max_rss = s->rss_kib;
          top_rss = s;
        }
      }
      if (top_rss) {
        out->spike_pid = top_rss->pid;
        strncpy(out->spike_comm, top_rss->comm, sizeof(out->spike_comm) - 1);
        out->spike_comm[sizeof(out->spike_comm) - 1] = '\0';
      }
    }

    state->last_swap_trigger_ns = current_ns;
  }

  /* Update baseline */
  state->swap_baseline_kib = (uint64_t)(
      config->memory_baseline_alpha * (double)swap_used +
      (1.0 - config->memory_baseline_alpha) * (double)baseline);
}

anomaly_result_t anomaly_evaluate(const anomaly_config_t *config,
                                  anomaly_state_t *state,
                                  const proc_sample_t *proc_samples,
                                  size_t proc_sample_count,
                                  const spkt_mem_snapshot_t *mem,
                                  uint64_t current_timestamp_ns) {
  anomaly_result_t result = {0};

  if (!config || !state) {
    return result;
  }

  /* Check CPU first (takes priority) */
  if (proc_samples && proc_sample_count > 0) {
    evaluate_cpu(config, state, proc_samples, proc_sample_count,
                 current_timestamp_ns, &result);
  }

  /* If no CPU anomaly, check memory */
  if (!result.has_anomaly && mem) {
    evaluate_mem(config, state, mem, proc_samples, proc_sample_count,
                 current_timestamp_ns, &result);
  }

  /* If no memory anomaly, check swap */
  if (!result.has_anomaly && mem) {
    evaluate_swap(config, state, mem, proc_samples, proc_sample_count,
                  current_timestamp_ns, &result);
  }

  return result;
}

/* Legacy wrapper */
anomaly_result_t anomaly_evaluate_procs(const anomaly_config_t *config,
                                        anomaly_state_t *state,
                                        const proc_sample_t *samples,
                                        size_t sample_count,
                                        uint64_t current_timestamp_ns) {
  return anomaly_evaluate(config, state, samples, sample_count, NULL,
                          current_timestamp_ns);
}

bool anomaly_should_dump(const anomaly_result_t *result) {
  if (!result) {
    return false;
  }
  return result->has_anomaly;
}
