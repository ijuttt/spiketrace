#ifndef SPKT_COMMON_H
#define SPKT_COMMON_H

/* Return status codes grouped by module */
typedef enum {
  SPKT_OK = 0,

  // CPU module errors
  SPKT_ERR_CPU_OPEN_PROC = -100,
  SPKT_ERR_CPU_PARSE_FAILED = -101,

  // Generic errors
  SPKT_ERR_INVALID_PARAM = -1,
  SPKT_ERR_NULL_POINTER = -2,
} spkt_status_t;

#endif
