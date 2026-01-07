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
