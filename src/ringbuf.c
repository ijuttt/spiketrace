#include "ringbuf.h"
#include "spkt_common.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Internal helper to get monotonic timestamp in nanoseconds */
static uint64_t get_monotonic_timestamp(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

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
  rb->overflow_warned = false;

  return SPKT_OK;
}

spkt_status_t ringbuf_cleanup(ringbuffer_t *rb) {
  if (!rb) {
    return SPKT_ERR_NULL_POINTER;
  }

  pthread_mutex_destroy(&rb->lock);
  return SPKT_OK;
}
