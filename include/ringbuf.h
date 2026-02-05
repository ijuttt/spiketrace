/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "snapshot.h"
#include "spkt_common.h"

#include <pthread.h>
#include <stdbool.h>

/* 60 snapshots = 1 minute at 1Hz */
#define SPKT_RINGBUF_CAPACITY 60

typedef struct {
  spkt_snapshot_t snapshots[SPKT_RINGBUF_CAPACITY];
  size_t head;
  size_t tail;
  size_t count;
  pthread_mutex_t lock;
} ringbuffer_t;

/* Initialize ring buffer */
spkt_status_t ringbuf_init(ringbuffer_t *rb);

/* Cleanup ring buffer resources */
spkt_status_t ringbuf_cleanup(ringbuffer_t *rb);

/* Push snapshot to buffer (overwrites oldest if full) */
spkt_status_t ringbuf_push(ringbuffer_t *rb, const spkt_snapshot_t *snapshot);


/* Get most recent N snapshots */
spkt_status_t ringbuf_get_recent(const ringbuffer_t *rb, spkt_snapshot_t *dest,
                                 size_t n, size_t *out_count);


/* Get current number of stored snapshots */
size_t ringbuf_count(const ringbuffer_t *rb);



#endif
