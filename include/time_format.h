#ifndef TIME_FORMAT_H
#define TIME_FORMAT_H

#include <stddef.h>
#include <time.h>

/**
 * Format current wall-clock time as ISO8601 with timezone.
 * Output format: "2026-01-30T17:08:09+07:00"
 *
 * @param buf Output buffer for the formatted string
 * @param buf_size Size of the output buffer (should be >= 32)
 * @return Number of characters written (excluding null terminator), or 0 on
 * error
 */
static inline size_t spkt_format_iso8601(char *buf, size_t buf_size) {
  if (!buf || buf_size < 26) {
    return 0;
  }

  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  if (!tm_info) {
    return 0;
  }

  /* Format: YYYY-MM-DDTHH:MM:SS+HHMM */
  size_t len = strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%S%z", tm_info);

  /* Insert colon in timezone offset for RFC3339 (e.g., +0700 -> +07:00) */
  if (len > 0 && len + 1 < buf_size) {
    /* Check if we have a timezone offset (ends with digits) */
    char *entry = buf + len;
    if (len >= 5 && (entry[-5] == '+' || entry[-5] == '-')) {
      /* Move minutes (last 2 chars) right by 1 */
      entry[1] = '\0';
      entry[0] = entry[-1];
      entry[-1] = entry[-2];
      entry[-2] = ':';
      return len + 1;
    }
  }
  return len;
}

/**
 * Convert nanoseconds to seconds as a double.
 * @param ns Nanoseconds value
 * @return Seconds as floating-point
 */
static inline double spkt_ns_to_seconds(uint64_t ns) {
  return (double)ns / 1000000000.0;
}

/**
 * Convert kibibytes to mebibytes (integer division).
 * @param kib Kibibytes value
 * @return Mebibytes (KiB / 1024)
 */
static inline uint64_t spkt_kib_to_mib(uint64_t kib) { return kib / 1024; }

#endif
