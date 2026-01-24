#define _POSIX_C_SOURCE 200809L

#include "anomaly_detector.h"
#include "proc.h"
#include "ringbuf.h"
#include "snapshot.h"
#include "snapshot_builder.h"
#include "spike_dump.h"
#include "spkt_common.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

/* Number of recent snapshots to include in spike dump (pre-spike context) */
#define SPIKE_DUMP_CONTEXT_SIZE 10

/* Shutdown flag set by signal handler */
static volatile sig_atomic_t shutdown_requested = 0;

static void signal_handler(int sig) {
  (void)sig; /* Unused parameter */
  shutdown_requested = 1;
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
  return 0;
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

  snapshot_builder_t builder;
  if (snapshot_builder_init(&builder, num_cores) != SPKT_OK) {
    fprintf(stderr, "spiketrace: snapshot builder init failed\n");
    return 1;
  }

  ringbuffer_t rb;
  if (ringbuf_init(&rb) != SPKT_OK) {
    fprintf(stderr, "spiketrace: ring buffer init failed\n");
    snapshot_builder_cleanup(&builder);
    return 1;
  }

  anomaly_config_t anomaly_config = anomaly_default_config();
  anomaly_state_t anomaly_state;
  anomaly_state_init(&anomaly_state);

  spike_dump_ctx_t dump_ctx;
  bool dumps_enabled = (spike_dump_init(&dump_ctx, NULL) == SPKT_OK);
  if (!dumps_enabled) {
    fprintf(stderr, "spiketrace: spike dumps disabled (init failed)\n");
  }

  fprintf(stderr, "spiketrace: started (pid=%d)\n", getpid());

  while (!shutdown_requested) {
    sleep(1);

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

    anomaly_result_t result =
        anomaly_evaluate(&anomaly_config, &anomaly_state, samples,
                         sample_count, &snap.mem, snap.timestamp_monotonic_ns);

    if (anomaly_should_dump(&result)) {
      /* Log anomaly type */
      switch (result.type) {
      case ANOMALY_TYPE_CPU_DELTA:
        fprintf(stderr,
                "spiketrace: [ANOMALY] CPU DELTA: [%d] %s  "
                "CPU: %.1f%% (baseline: %.1f%%, delta: +%.1f%%)\n",
                result.spike_pid, result.spike_comm, result.spike_cpu_pct,
                result.spike_baseline_pct, result.spike_delta);
        break;
      case ANOMALY_TYPE_CPU_NEW_PROC:
        fprintf(stderr,
                "spiketrace: [ANOMALY] NEW PROCESS: [%d] %s  CPU: %.1f%%\n",
                result.spike_pid, result.spike_comm, result.spike_cpu_pct);
        break;
      case ANOMALY_TYPE_MEM_DROP:
        fprintf(stderr,
                "spiketrace: [ANOMALY] MEM DROP by [%d] %s: available: %lu MiB "
                "(baseline: %lu MiB, delta: %ld MiB)\n",
                result.spike_pid, result.spike_comm,
                (unsigned long)(result.mem_available_kib / 1024),
                (unsigned long)(result.mem_baseline_kib / 1024),
                (long)(result.mem_delta_kib / 1024));
        break;
      case ANOMALY_TYPE_MEM_PRESSURE:
        fprintf(stderr,
                "spiketrace: [ANOMALY] MEM PRESSURE: [%d] %s top RSS, %.1f%% "
                "used (available: %lu MiB)\n",
                result.spike_pid, result.spike_comm,
                result.mem_used_pct,
                (unsigned long)(result.mem_available_kib / 1024));
        break;
      case ANOMALY_TYPE_SWAP_SPIKE:
        fprintf(stderr,
                "spiketrace: [ANOMALY] SWAP SPIKE by [%d] %s: used: %lu MiB "
                "(baseline: %lu MiB, delta: +%ld MiB)\n",
                result.spike_pid, result.spike_comm,
                (unsigned long)(result.swap_used_kib / 1024),
                (unsigned long)(result.swap_baseline_kib / 1024),
                (long)(result.swap_delta_kib / 1024));
        break;
      default:
        break;
      }

      if (dumps_enabled) {
        /* Extract recent snapshots and write dump */
        spkt_snapshot_t dump_snaps[SPIKE_DUMP_CONTEXT_SIZE];
        size_t dump_count = 0;
        ringbuf_get_recent(&rb, dump_snaps, SPIKE_DUMP_CONTEXT_SIZE,
                           &dump_count);

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

  if (dumps_enabled) {
    spike_dump_cleanup(&dump_ctx);
  }
  snapshot_builder_cleanup(&builder);
  ringbuf_cleanup(&rb);

  fprintf(stderr, "spiketrace: stopped\n");
  return 0;
}
