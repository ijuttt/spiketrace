#define _POSIX_C_SOURCE 200809L

#include "ringbuf.h"
#include "spkt_common.h"
#include "time_utils.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

spkt_status_t ringbuf_init(ringbuffer_t *rb) {
  if (!rb) {
    return SPKT_ERR_NULL_POINTER;
  }

  if (pthread_mutex_init(&rb->lock, NULL) != 0) {
    fprintf(stderr, "ringbuf: Failed to initialize mutex\n");
    return SPKT_ERR_INVALID_PARAM;
  }

  memset(rb->snapshots, 0, sizeof(rb->snapshots));
  rb->head = 0;
  rb->tail = 0;
  rb->count = 0;

  return SPKT_OK;
}

spkt_status_t ringbuf_cleanup(ringbuffer_t *rb) {
  if (!rb) {
    return SPKT_ERR_NULL_POINTER;
  }

  pthread_mutex_destroy(&rb->lock);
  return SPKT_OK;
}

spkt_status_t ringbuf_push(ringbuffer_t *rb, const spkt_snapshot_t *snapshot) {
  if (!rb || !snapshot) {
    return SPKT_ERR_NULL_POINTER;
  }

  pthread_mutex_lock(&rb->lock);

  /* Ensure timestamp is set if not provided */
  spkt_snapshot_t snapshot_copy = *snapshot;
  if (snapshot_copy.timestamp_monotonic_ns == 0) {
    snapshot_copy.timestamp_monotonic_ns = spkt_get_monotonic_ns();
  }

  rb->snapshots[rb->tail] = snapshot_copy;

  /* Circular buffer: advance head when full (overwrite oldest) */
  if (rb->count == SPKT_RINGBUF_CAPACITY) {
    rb->head = (rb->head + 1) % SPKT_RINGBUF_CAPACITY;
  } else {
    rb->count++;
  }

  rb->tail = (rb->tail + 1) % SPKT_RINGBUF_CAPACITY;

  pthread_mutex_unlock(&rb->lock);
  return SPKT_OK;
}


spkt_status_t ringbuf_get_recent(const ringbuffer_t *rb, spkt_snapshot_t *dest,
                                 size_t n, size_t *out_count) {
  if (!rb || !dest || !out_count) {
    return SPKT_ERR_NULL_POINTER;
  }

  pthread_mutex_lock((pthread_mutex_t *)&rb->lock);

  size_t available = rb->count;
  size_t count_to_copy = (n < available) ? n : available;
  *out_count = count_to_copy;

  if (count_to_copy == 0) {
    pthread_mutex_unlock((pthread_mutex_t *)&rb->lock);
    return SPKT_OK;
  }

  // Copy in reverse chronological order (newest first)
  for (size_t i = 0; i < count_to_copy; i++) {
    // Calculate index: tail-1 is newest, tail-2 is second newest, etc.
    size_t idx =
        (rb->tail + SPKT_RINGBUF_CAPACITY - i - 1) % SPKT_RINGBUF_CAPACITY;
    dest[i] = rb->snapshots[idx];
  }

  pthread_mutex_unlock((pthread_mutex_t *)&rb->lock);
  return SPKT_OK;
}


size_t ringbuf_count(const ringbuffer_t *rb) {
  if (!rb)
    return 0;

  pthread_mutex_lock((pthread_mutex_t *)&rb->lock);
  size_t count = rb->count;
  pthread_mutex_unlock((pthread_mutex_t *)&rb->lock);

  return count;
}

