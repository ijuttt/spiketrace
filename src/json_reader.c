/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#define _POSIX_C_SOURCE 200809L

#include "json_reader.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Skip whitespace */
static void skip_ws(json_reader_t *r) {
  while (r->pos < r->len) {
    char c = r->data[r->pos];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      r->pos++;
    } else {
      break;
    }
  }
}

/* Parse string (starting after opening quote) */
static bool parse_string(json_reader_t *r, char *out, size_t max_len) {
  size_t out_pos = 0;

  while (r->pos < r->len && out_pos < max_len - 1) {
    char c = r->data[r->pos++];

    if (c == '"') {
      out[out_pos] = '\0';
      return true;
    }

    if (c == '\\' && r->pos < r->len) {
      char esc = r->data[r->pos++];
      switch (esc) {
      case '"':
        out[out_pos++] = '"';
        break;
      case '\\':
        out[out_pos++] = '\\';
        break;
      case 'n':
        out[out_pos++] = '\n';
        break;
      case 'r':
        out[out_pos++] = '\r';
        break;
      case 't':
        out[out_pos++] = '\t';
        break;
      default:
        out[out_pos++] = esc;
        break;
      }
    } else {
      out[out_pos++] = c;
    }
  }

  out[out_pos] = '\0';
  return false;
}

/* Parse number */
static bool parse_number(json_reader_t *r) {
  size_t start = r->pos;

  /* Optional minus */
  if (r->pos < r->len && r->data[r->pos] == '-') {
    r->pos++;
  }

  /* Digits */
  while (r->pos < r->len && isdigit((unsigned char)r->data[r->pos])) {
    r->pos++;
  }

  /* Optional decimal */
  if (r->pos < r->len && r->data[r->pos] == '.') {
    r->pos++;
    while (r->pos < r->len && isdigit((unsigned char)r->data[r->pos])) {
      r->pos++;
    }
  }

  /* Optional exponent */
  if (r->pos < r->len && (r->data[r->pos] == 'e' || r->data[r->pos] == 'E')) {
    r->pos++;
    if (r->pos < r->len && (r->data[r->pos] == '+' || r->data[r->pos] == '-')) {
      r->pos++;
    }
    while (r->pos < r->len && isdigit((unsigned char)r->data[r->pos])) {
      r->pos++;
    }
  }

  if (r->pos > start) {
    char buf[64];
    size_t len = r->pos - start;
    if (len >= sizeof(buf)) {
      len = sizeof(buf) - 1;
    }
    memcpy(buf, r->data + start, len);
    buf[len] = '\0';
    r->num_val = strtod(buf, NULL);
    return true;
  }

  return false;
}

void json_reader_init(json_reader_t *r, const char *data, size_t len) {
  memset(r, 0, sizeof(*r));
  r->data = data;
  r->len = len;
  r->token = JSON_TOK_NONE;
}

json_token_type_t json_reader_next(json_reader_t *r) {
  skip_ws(r);

  if (r->pos >= r->len) {
    r->token = JSON_TOK_EOF;
    return r->token;
  }

  char c = r->data[r->pos];

  switch (c) {
  case '{':
    r->pos++;
    r->depth++;
    r->token = JSON_TOK_OBJECT_START;
    return r->token;

  case '}':
    r->pos++;
    r->depth--;
    r->token = JSON_TOK_OBJECT_END;
    return r->token;

  case '[':
    r->pos++;
    r->depth++;
    r->in_array = true;
    r->token = JSON_TOK_ARRAY_START;
    return r->token;

  case ']':
    r->pos++;
    r->depth--;
    r->in_array = false;
    r->token = JSON_TOK_ARRAY_END;
    return r->token;

  case ',':
  case ':':
    r->pos++;
    return json_reader_next(r);

  case '"':
    r->pos++;
    if (!parse_string(r, r->str_val, sizeof(r->str_val))) {
      r->token = JSON_TOK_ERROR;
      return r->token;
    }

    skip_ws(r);
    if (r->pos < r->len && r->data[r->pos] == ':') {
      strncpy(r->key, r->str_val, sizeof(r->key) - 1);
      r->key[sizeof(r->key) - 1] = '\0';
      r->token = JSON_TOK_KEY;
    } else {
      r->token = JSON_TOK_STRING;
    }
    return r->token;

  case 't':
    if (r->pos + 4 <= r->len && strncmp(r->data + r->pos, "true", 4) == 0) {
      r->pos += 4;
      r->bool_val = true;
      r->token = JSON_TOK_BOOL;
      return r->token;
    }
    r->token = JSON_TOK_ERROR;
    return r->token;

  case 'f':
    if (r->pos + 5 <= r->len && strncmp(r->data + r->pos, "false", 5) == 0) {
      r->pos += 5;
      r->bool_val = false;
      r->token = JSON_TOK_BOOL;
      return r->token;
    }
    r->token = JSON_TOK_ERROR;
    return r->token;

  case 'n':
    if (r->pos + 4 <= r->len && strncmp(r->data + r->pos, "null", 4) == 0) {
      r->pos += 4;
      r->token = JSON_TOK_NULL;
      return r->token;
    }
    r->token = JSON_TOK_ERROR;
    return r->token;

  default:
    if (c == '-' || isdigit((unsigned char)c)) {
      if (parse_number(r)) {
        r->token = JSON_TOK_NUMBER;
        return r->token;
      }
    }
    r->token = JSON_TOK_ERROR;
    return r->token;
  }
}

bool json_reader_skip(json_reader_t *r) {
  int start_depth = r->depth;

  switch (r->token) {
  case JSON_TOK_OBJECT_START:
  case JSON_TOK_ARRAY_START:
    while (json_reader_next(r) != JSON_TOK_EOF &&
           json_reader_next(r) != JSON_TOK_ERROR) {
      if (r->depth < start_depth) {
        return true;
      }
    }
    return false;

  case JSON_TOK_STRING:
  case JSON_TOK_NUMBER:
  case JSON_TOK_BOOL:
  case JSON_TOK_NULL:
    return true;

  default:
    return false;
  }
}

const char *json_reader_get_string(const json_reader_t *r) {
  return r->str_val;
}

int64_t json_reader_get_int(const json_reader_t *r) {
  return (int64_t)r->num_val;
}

uint64_t json_reader_get_uint(const json_reader_t *r) {
  return (uint64_t)r->num_val;
}

double json_reader_get_double(const json_reader_t *r) {
  return r->num_val;
}

bool json_reader_get_bool(const json_reader_t *r) {
  return r->bool_val;
}

bool json_reader_key_equals(const json_reader_t *r, const char *key) {
  return strcmp(r->key, key) == 0;
}
