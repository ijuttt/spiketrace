#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <time.h>
#include <stdint.h>

static inline uint64_t spkt_get_monotonic_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#endif
