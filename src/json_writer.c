/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#include "json_writer.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum buffer size to prevent runaway growth (1MB) */
#define SPKT_JSON_MAX_BUFFER_SIZE (1024 * 1024)

/* Try to grow buffer to fit additional data */
static bool json_grow_buffer(spkt_json_writer_t *writer, size_t needed) {
  if (writer->capacity >= SPKT_JSON_MAX_BUFFER_SIZE) {
    return false; /* Hit max limit */
  }

  size_t new_capacity = writer->capacity * 2;
  if (new_capacity < writer->length + needed + 1) {
    new_capacity = writer->length + needed + 1;
  }
  if (new_capacity > SPKT_JSON_MAX_BUFFER_SIZE) {
    new_capacity = SPKT_JSON_MAX_BUFFER_SIZE;
  }

  char *new_buffer = realloc(writer->buffer, new_capacity);
  if (!new_buffer) {
    return false;
  }

  writer->buffer = new_buffer;
  writer->capacity = new_capacity;
  return true;
}

/* Internal helper to append string to buffer with auto-growth */
static spkt_status_t json_append(spkt_json_writer_t *writer, const char *str,
                                 size_t len) {
  if (writer->error) {
    return SPKT_ERR_JSON_OVERFLOW;
  }

  /* Try to grow if needed */
  if (writer->length + len >= writer->capacity) {
    if (!json_grow_buffer(writer, len)) {
      writer->error = true;
      return SPKT_ERR_JSON_OVERFLOW;
    }
  }

  memcpy(writer->buffer + writer->length, str, len);
  writer->length += len;
  writer->buffer[writer->length] = '\0';

  return SPKT_OK;
}

/* Append single character */
static spkt_status_t json_append_char(spkt_json_writer_t *writer, char c) {
  return json_append(writer, &c, 1);
}

/* Append comma if needed before next element */
static spkt_status_t json_maybe_comma(spkt_json_writer_t *writer) {
  if (writer->needs_comma) {
    return json_append_char(writer, ',');
  }
  return SPKT_OK;
}

/* Escape and append string value (handles special JSON chars) */
static spkt_status_t json_append_escaped(spkt_json_writer_t *writer,
                                         const char *str) {
  spkt_status_t status;

  status = json_append_char(writer, '"');
  if (status != SPKT_OK) {
    return status;
  }

  for (const char *p = str; *p != '\0'; p++) {
    char c = *p;
    const char *escape = NULL;

    switch (c) {
    case '"':
      escape = "\\\"";
      break;
    case '\\':
      escape = "\\\\";
      break;
    case '\b':
      escape = "\\b";
      break;
    case '\f':
      escape = "\\f";
      break;
    case '\n':
      escape = "\\n";
      break;
    case '\r':
      escape = "\\r";
      break;
    case '\t':
      escape = "\\t";
      break;
    default:
      /* Control characters: use \u00XX format */
      if ((unsigned char)c < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
        status = json_append(writer, buf, 6);
        if (status != SPKT_OK) {
          return status;
        }
        continue;
      }
      break;
    }

    if (escape) {
      status = json_append(writer, escape, strlen(escape));
    } else {
      status = json_append_char(writer, c);
    }

    if (status != SPKT_OK) {
      return status;
    }
  }

  return json_append_char(writer, '"');
}

spkt_status_t spkt_json_init(spkt_json_writer_t *writer, size_t capacity) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  if (capacity == 0) {
    capacity = SPKT_JSON_DEFAULT_BUFFER_SIZE;
  }

  writer->buffer = malloc(capacity);
  if (!writer->buffer) {
    return SPKT_ERR_JSON_ALLOC;
  }

  writer->capacity = capacity;
  writer->length = 0;
  writer->buffer[0] = '\0';
  writer->needs_comma = false;
  writer->error = false;

  return SPKT_OK;
}

void spkt_json_cleanup(spkt_json_writer_t *writer) {
  if (!writer) {
    return;
  }

  free(writer->buffer);
  writer->buffer = NULL;
  writer->capacity = 0;
  writer->length = 0;
  writer->needs_comma = false;
  writer->error = false;
}


spkt_status_t spkt_json_begin_object(spkt_json_writer_t *writer) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  spkt_status_t status = json_maybe_comma(writer);
  if (status != SPKT_OK) {
    return status;
  }

  writer->needs_comma = false;
  return json_append_char(writer, '{');
}

spkt_status_t spkt_json_end_object(spkt_json_writer_t *writer) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  writer->needs_comma = true;
  return json_append_char(writer, '}');
}

spkt_status_t spkt_json_begin_array(spkt_json_writer_t *writer) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  spkt_status_t status = json_maybe_comma(writer);
  if (status != SPKT_OK) {
    return status;
  }

  writer->needs_comma = false;
  return json_append_char(writer, '[');
}

spkt_status_t spkt_json_end_array(spkt_json_writer_t *writer) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  writer->needs_comma = true;
  return json_append_char(writer, ']');
}

spkt_status_t spkt_json_key(spkt_json_writer_t *writer, const char *key) {
  if (!writer || !key) {
    return SPKT_ERR_NULL_POINTER;
  }

  spkt_status_t status = json_maybe_comma(writer);
  if (status != SPKT_OK) {
    return status;
  }

  status = json_append_escaped(writer, key);
  if (status != SPKT_OK) {
    return status;
  }

  writer->needs_comma = false;
  return json_append_char(writer, ':');
}

spkt_status_t spkt_json_string(spkt_json_writer_t *writer, const char *value) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  spkt_status_t status = json_maybe_comma(writer);
  if (status != SPKT_OK) {
    return status;
  }

  /* Handle NULL as empty string */
  if (!value) {
    value = "";
  }

  writer->needs_comma = true;
  return json_append_escaped(writer, value);
}

spkt_status_t spkt_json_int(spkt_json_writer_t *writer, int64_t value) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  spkt_status_t status = json_maybe_comma(writer);
  if (status != SPKT_OK) {
    return status;
  }

  char buf[32];
  int len = snprintf(buf, sizeof(buf), "%" PRId64, value);
  if (len < 0 || (size_t)len >= sizeof(buf)) {
    return SPKT_ERR_JSON_OVERFLOW;
  }

  writer->needs_comma = true;
  return json_append(writer, buf, (size_t)len);
}

spkt_status_t spkt_json_uint(spkt_json_writer_t *writer, uint64_t value) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  spkt_status_t status = json_maybe_comma(writer);
  if (status != SPKT_OK) {
    return status;
  }

  char buf[32];
  int len = snprintf(buf, sizeof(buf), "%" PRIu64, value);
  if (len < 0 || (size_t)len >= sizeof(buf)) {
    return SPKT_ERR_JSON_OVERFLOW;
  }

  writer->needs_comma = true;
  return json_append(writer, buf, (size_t)len);
}

spkt_status_t spkt_json_double(spkt_json_writer_t *writer, double value) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  spkt_status_t status = json_maybe_comma(writer);
  if (status != SPKT_OK) {
    return status;
  }

  char buf[32];
  int len = snprintf(buf, sizeof(buf), "%.*f", SPKT_JSON_DOUBLE_PRECISION, value);
  if (len < 0 || (size_t)len >= sizeof(buf)) {
    return SPKT_ERR_JSON_OVERFLOW;
  }

  writer->needs_comma = true;
  return json_append(writer, buf, (size_t)len);
}

spkt_status_t spkt_json_bool(spkt_json_writer_t *writer, bool value) {
  if (!writer) {
    return SPKT_ERR_NULL_POINTER;
  }

  spkt_status_t status = json_maybe_comma(writer);
  if (status != SPKT_OK) {
    return status;
  }

  writer->needs_comma = true;
  if (value) {
    return json_append(writer, "true", 4);
  } else {
    return json_append(writer, "false", 5);
  }
}

const char *spkt_json_get_buffer(const spkt_json_writer_t *writer) {
  if (!writer || !writer->buffer) {
    return "";
  }
  return writer->buffer;
}

size_t spkt_json_get_length(const spkt_json_writer_t *writer) {
  if (!writer) {
    return 0;
  }
  return writer->length;
}

bool spkt_json_has_error(const spkt_json_writer_t *writer) {
  if (!writer) {
    return true;
  }
  return writer->error;
}
