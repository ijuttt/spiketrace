/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include "spkt_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Minimal JSON writer for spike dump serialization.
 * Uses a pre-allocated buffer with bounds checking.
 * Does NOT support nesting depth tracking or validation.
 * Caller is responsible for correct nesting.
 */

/* Default buffer size: 64 KiB (sufficient for ~10 snapshots) */
#define SPKT_JSON_DEFAULT_BUFFER_SIZE (64 * 1024)

/* Maximum precision for double formatting */
#define SPKT_JSON_DOUBLE_PRECISION 2

typedef struct {
  char *buffer;
  size_t capacity;
  size_t length;
  bool needs_comma; /* Track if next element needs a leading comma */
  bool error;       /* Set if buffer overflow occurred */
} spkt_json_writer_t;

/* Initialize writer with given buffer capacity.
 * Returns SPKT_OK on success.
 * Allocates buffer internally; caller must call spkt_json_cleanup().
 */
spkt_status_t spkt_json_init(spkt_json_writer_t *writer, size_t capacity);

/* Cleanup writer resources */
void spkt_json_cleanup(spkt_json_writer_t *writer);


/* Object/array delimiters */
spkt_status_t spkt_json_begin_object(spkt_json_writer_t *writer);
spkt_status_t spkt_json_end_object(spkt_json_writer_t *writer);
spkt_status_t spkt_json_begin_array(spkt_json_writer_t *writer);
spkt_status_t spkt_json_end_array(spkt_json_writer_t *writer);

/* Write object key (must be followed by a value) */
spkt_status_t spkt_json_key(spkt_json_writer_t *writer, const char *key);

/* Value writers */
spkt_status_t spkt_json_string(spkt_json_writer_t *writer, const char *value);
spkt_status_t spkt_json_int(spkt_json_writer_t *writer, int64_t value);
spkt_status_t spkt_json_uint(spkt_json_writer_t *writer, uint64_t value);
spkt_status_t spkt_json_double(spkt_json_writer_t *writer, double value);
spkt_status_t spkt_json_bool(spkt_json_writer_t *writer, bool value);

/* Get buffer contents (null-terminated) */
const char *spkt_json_get_buffer(const spkt_json_writer_t *writer);

/* Get current length (excluding null terminator) */
size_t spkt_json_get_length(const spkt_json_writer_t *writer);

/* Check if overflow occurred */
bool spkt_json_has_error(const spkt_json_writer_t *writer);

#endif
