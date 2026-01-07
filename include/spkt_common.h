#ifndef SPKT_COMMON_H
#define SPKT_COMMON_H

/* Return status codes grouped by module */
typedef enum {
  SPKT_OK = 0,

  // CPU module errors
  SPKT_ERR_CPU_OPEN_PROC = -100,
  SPKT_ERR_CPU_PARSE_FAILED = -101,

  // Memory module errors
  SPKT_ERR_MEM_OPEN_MEMINFO = -200,
  SPKT_ERR_MEM_PARSE_FAILED = -201,

  // Ring buffer errors
  SPKT_ERR_RINGBUF_FULL = -300,
  SPKT_ERR_RINGBUF_EMPTY = -301,
  SPKT_ERR_RINGBUF_LOCK_FAILED = -302,

  // Generic errors
  SPKT_ERR_INVALID_PARAM = -1,
  SPKT_ERR_NULL_POINTER = -2,
} spkt_status_t;

#endif
