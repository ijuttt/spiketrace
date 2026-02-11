/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#ifndef SPIKE_DUMP_H
#define SPIKE_DUMP_H

#include "anomaly_detector.h"
#include "snapshot.h"
#include "spkt_common.h"

#include <stddef.h>
#include <sys/types.h>

/*
 * Spike dump module: orchestrates persistence of spike snapshots to JSON.
 * Writes are atomic (temp file + rename) to prevent partial/corrupt files.
 */

/* Schema version for forward compatibility */
#define SPIKE_DUMP_SCHEMA_VERSION 5

/* Maximum snapshots to include in a dump (pre-spike context + current) */
#define SPIKE_DUMP_MAX_SNAPSHOTS 10

/* Default output directory
 * Can be overridden at build time via -DSPIKE_DUMP_DEFAULT_DIR=\"...\" */
#ifndef SPIKE_DUMP_DEFAULT_DIR
#define SPIKE_DUMP_DEFAULT_DIR "/var/lib/spiketrace"
#endif

/* Maximum path length for dump files */
#define SPIKE_DUMP_PATH_MAX 256

/* Group name for dump file ownership */
#define SPIKE_DUMP_GROUP "spiketrace"

typedef struct {
  char output_dir[SPIKE_DUMP_PATH_MAX];
  uint64_t dump_count; /* Number of dumps written (for unique filenames) */
  gid_t spike_gid;     /* GID for 'spiketrace' group (0 if not found) */
} spike_dump_ctx_t;

/* Initialize dump context with output directory.
 * If dir is NULL, uses SPIKE_DUMP_DEFAULT_DIR.
 * Does NOT create the directory; caller must ensure it exists.
 */
spkt_status_t spike_dump_init(spike_dump_ctx_t *ctx, const char *dir);

/* Write spike dump to JSON file.
 * snapshots: array of recent snapshots (newest first, from ringbuf_get_recent)
 * snapshot_count: number of snapshots in array
 * anomaly: the anomaly result that triggered the dump
 * timestamp_ns: current monotonic timestamp
 *
 * Returns SPKT_OK on success; errors are non-fatal (monitoring continues).
 */
spkt_status_t spike_dump_write(spike_dump_ctx_t *ctx,
                               const spkt_snapshot_t *snapshots,
                               size_t snapshot_count,
                               const anomaly_result_t *anomaly,
                               uint64_t timestamp_ns);

/* Cleanup dump context (currently no-op, future-proof) */
void spike_dump_cleanup(spike_dump_ctx_t *ctx);

#endif
