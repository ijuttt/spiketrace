/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

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

  // Process collector errors
  SPKT_ERR_PROC_OPEN_DIR = -400,
  SPKT_ERR_PROC_PARSE_FAILED = -401,

  // JSON writer errors
  SPKT_ERR_JSON_OVERFLOW = -500,
  SPKT_ERR_JSON_ALLOC = -501,

  // Spike dump errors
  SPKT_ERR_DUMP_OPEN_FAILED = -600,
  SPKT_ERR_DUMP_WRITE_FAILED = -601,
  SPKT_ERR_DUMP_RENAME_FAILED = -602,

  // Generic errors
  SPKT_ERR_INVALID_PARAM = -1,
  SPKT_ERR_NULL_POINTER = -2,
} spkt_status_t;

#endif
