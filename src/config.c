/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "spkt_common.h"

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ===== BUILT-IN DEFAULTS ===== */

/* Import spike_dump.h for SPIKE_DUMP_DEFAULT_DIR (allows build-time override) */
#include "spike_dump.h"

/* These match the current compile-time constants */
#define DEFAULT_CPU_DELTA_THRESHOLD_PCT 10.0
#define DEFAULT_NEW_PROCESS_THRESHOLD_PCT 5.0
#define DEFAULT_MEM_DROP_THRESHOLD_MIB 512
#define DEFAULT_MEM_PRESSURE_THRESHOLD_PCT 90.0
#define DEFAULT_SWAP_SPIKE_THRESHOLD_MIB 256
#define DEFAULT_COOLDOWN_SECONDS 5.0
#define DEFAULT_SAMPLING_INTERVAL_SECONDS 1.0
#define DEFAULT_RING_BUFFER_CAPACITY 60
#define DEFAULT_CONTEXT_SNAPSHOTS_PER_DUMP 10
#define DEFAULT_MAX_PROCESSES_TRACKED 512
#define DEFAULT_TOP_PROCESSES_STORED 10
#define DEFAULT_MEMORY_BASELINE_ALPHA 0.2
#define DEFAULT_PROCESS_BASELINE_ALPHA 0.3

/* Use header-defined default (supports build-time override via -DSPIKE_DUMP_DEFAULT_DIR) */
#define DEFAULT_OUTPUT_DIRECTORY SPIKE_DUMP_DEFAULT_DIR

/* ===== LOG MANAGEMENT DEFAULTS ===== */
#define DEFAULT_ENABLE_AUTO_CLEANUP false
#define DEFAULT_LOG_CLEANUP_POLICY LOG_CLEANUP_DISABLED
#define DEFAULT_LOG_MAX_AGE_DAYS 30
#define DEFAULT_LOG_MAX_COUNT 100
#define DEFAULT_LOG_MAX_TOTAL_SIZE_MIB 512
#define DEFAULT_CLEANUP_INTERVAL_MINUTES 60

/* ===== VALIDATION BOUNDS ===== */

#define MIN_CPU_DELTA_THRESHOLD_PCT 0.1
#define MAX_CPU_DELTA_THRESHOLD_PCT 100.0
#define MIN_NEW_PROCESS_THRESHOLD_PCT 0.1
#define MAX_NEW_PROCESS_THRESHOLD_PCT 100.0
#define MIN_MEM_DROP_THRESHOLD_MIB 1
#define MAX_MEM_DROP_THRESHOLD_MIB (1024 * 1024) /* 1 TiB */
#define MIN_MEM_PRESSURE_THRESHOLD_PCT 50.0
#define MAX_MEM_PRESSURE_THRESHOLD_PCT 100.0
#define MIN_SWAP_SPIKE_THRESHOLD_MIB 1
#define MAX_SWAP_SPIKE_THRESHOLD_MIB (1024 * 1024) /* 1 TiB */
#define MIN_COOLDOWN_SECONDS 0.1
#define MAX_COOLDOWN_SECONDS 300.0
#define MIN_SAMPLING_INTERVAL_SECONDS 0.1
#define MAX_SAMPLING_INTERVAL_SECONDS 10.0
#define MIN_RING_BUFFER_CAPACITY 10
#define MAX_RING_BUFFER_CAPACITY 600
#define MIN_CONTEXT_SNAPSHOTS_PER_DUMP 1
#define MAX_CONTEXT_SNAPSHOTS_PER_DUMP 60
#define MIN_MAX_PROCESSES_TRACKED 10
#define MAX_MAX_PROCESSES_TRACKED 1024
#define MIN_TOP_PROCESSES_STORED 1
#define MAX_TOP_PROCESSES_STORED 50
#define MIN_BASELINE_ALPHA 0.01
#define MAX_BASELINE_ALPHA 0.9
#define MIN_JSON_BUFFER_SIZE_KIB 16
#define MAX_JSON_BUFFER_SIZE_KIB 1024

/* Log management validation bounds */
#define MIN_LOG_MAX_AGE_DAYS 1
#define MAX_LOG_MAX_AGE_DAYS 365
#define MIN_LOG_MAX_COUNT 1
#define MAX_LOG_MAX_COUNT 10000
#define MIN_LOG_MAX_TOTAL_SIZE_MIB 1
#define MAX_LOG_MAX_TOTAL_SIZE_MIB (100 * 1024) /* 100 GiB */
#define MIN_CLEANUP_INTERVAL_MINUTES 1
#define MAX_CLEANUP_INTERVAL_MINUTES (24 * 60) /* 24 hours */

/* ===== TOML PARSER STATE ===== */

typedef enum {
  TOML_TOK_NONE = 0,
  TOML_TOK_KEY,
  TOML_TOK_STRING,
  TOML_TOK_INTEGER,
  TOML_TOK_FLOAT,
  TOML_TOK_BOOLEAN,
  TOML_TOK_TABLE_START, /* [section] */
  TOML_TOK_EQUAL,
  TOML_TOK_NEWLINE,
  TOML_TOK_EOF,
  TOML_TOK_ERROR,
} toml_token_type_t;

typedef struct {
  const char *data;
  size_t len;
  size_t pos;
  toml_token_type_t token;
  char key_buf[128];
  size_t key_len;
  char str_buf[CONFIG_MAX_PATH_LEN];
  size_t str_len;
  int64_t int_val;
  double float_val;
  bool bool_val;
  int line;
  int col;
} toml_parser_t;

/* ===== TOML PARSER HELPERS ===== */

static bool is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\r'; }

static bool is_newline(char c) { return c == '\n'; }

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static void skip_whitespace(toml_parser_t *p) {
  while (p->pos < p->len && is_whitespace(p->data[p->pos])) {
    p->pos++;
    p->col++;
  }
}

static void skip_comment(toml_parser_t *p) {
  if (p->pos < p->len && p->data[p->pos] == '#') {
    while (p->pos < p->len && !is_newline(p->data[p->pos])) {
      p->pos++;
    }
  }
}

static bool peek_char(toml_parser_t *p, char *out) {
  if (p->pos >= p->len) {
    return false;
  }
  *out = p->data[p->pos];
  return true;
}

static bool consume_char(toml_parser_t *p, char expected) {
  if (p->pos >= p->len || p->data[p->pos] != expected) {
    return false;
  }
  p->pos++;
  if (expected == '\n') {
    p->line++;
    p->col = 1;
  } else {
    p->col++;
  }
  return true;
}

/* Parse string value (basic, unquoted or quoted) */
static bool parse_string(toml_parser_t *p) {
  p->str_len = 0;
  p->str_buf[0] = '\0';

  if (p->pos >= p->len) {
    return false;
  }

  if (p->data[p->pos] == '"') {
    /* Quoted string */
    p->pos++;
    while (p->pos < p->len && p->data[p->pos] != '"') {
      if (p->data[p->pos] == '\\' && p->pos + 1 < p->len) {
        /* Simple escape handling */
        p->pos++;
        char esc = p->data[p->pos];
        if (esc == 'n') {
          esc = '\n';
        } else if (esc == 't') {
          esc = '\t';
        } else if (esc == 'r') {
          esc = '\r';
        } else if (esc == '\\') {
          esc = '\\';
        } else if (esc == '"') {
          esc = '"';
        }
        if (p->str_len < sizeof(p->str_buf) - 1) {
          p->str_buf[p->str_len++] = esc;
        }
        p->pos++;
      } else {
        if (p->str_len < sizeof(p->str_buf) - 1) {
          p->str_buf[p->str_len++] = p->data[p->pos];
        }
        p->pos++;
      }
    }
    if (p->pos >= p->len || p->data[p->pos] != '"') {
      return false; /* Unterminated string */
    }
    p->pos++;
  } else {
    /* Unquoted string (key or bare string) */
    while (p->pos < p->len && !is_whitespace(p->data[p->pos]) &&
           p->data[p->pos] != '=' && p->data[p->pos] != ']' &&
           p->data[p->pos] != '#' && !is_newline(p->data[p->pos])) {
      if (p->str_len < sizeof(p->str_buf) - 1) {
        p->str_buf[p->str_len++] = p->data[p->pos];
      }
      p->pos++;
    }
  }

  p->str_buf[p->str_len] = '\0';
  return true;
}

/* Parse integer */
static bool parse_integer(toml_parser_t *p) {
  bool negative = false;
  if (p->pos < p->len && p->data[p->pos] == '-') {
    negative = true;
    p->pos++;
  }

  int64_t val = 0;
  bool has_digit = false;

  while (p->pos < p->len && is_digit(p->data[p->pos])) {
    has_digit = true;
    int64_t digit = p->data[p->pos] - '0';
    if (val > (INT64_MAX - digit) / 10) {
      return false; /* Overflow */
    }
    val = val * 10 + digit;
    p->pos++;
  }

  if (!has_digit) {
    return false;
  }

  p->int_val = negative ? -val : val;
  return true;
}

/* Parse float */
static bool parse_float(toml_parser_t *p) {
  char num_buf[64];
  size_t num_len = 0;
  bool has_dot = false;

  if (p->pos < p->len && p->data[p->pos] == '-') {
    if (num_len < sizeof(num_buf) - 1) {
      num_buf[num_len++] = '-';
    }
    p->pos++;
  }

  while (p->pos < p->len &&
         (is_digit(p->data[p->pos]) || p->data[p->pos] == '.')) {
    if (p->data[p->pos] == '.') {
      if (has_dot) {
        break; /* Second dot */
      }
      has_dot = true;
    }
    if (num_len < sizeof(num_buf) - 1) {
      num_buf[num_len++] = p->data[p->pos];
    }
    p->pos++;
  }

  if (num_len == 0 || (num_len == 1 && num_buf[0] == '-')) {
    return false;
  }

  num_buf[num_len] = '\0';
  char *endptr;
  p->float_val = strtod(num_buf, &endptr);
  if (*endptr != '\0' && *endptr != '\n' && *endptr != ' ' && *endptr != '\t' &&
      *endptr != '#') {
    return false;
  }

  return true;
}

/* Parse boolean */
static bool parse_boolean(toml_parser_t *p) {
  if (p->pos + 4 <= p->len && strncmp(&p->data[p->pos], "true", 4) == 0) {
    p->pos += 4;
    p->bool_val = true;
    return true;
  }
  if (p->pos + 5 <= p->len && strncmp(&p->data[p->pos], "false", 5) == 0) {
    p->pos += 5;
    p->bool_val = false;
    return true;
  }
  return false;
}

/* Get next token */
static toml_token_type_t toml_next_token(toml_parser_t *p) {
  skip_whitespace(p);

  if (p->pos >= p->len) {
    p->token = TOML_TOK_EOF;
    return p->token;
  }

  char c = p->data[p->pos];

  if (is_newline(c)) {
    consume_char(p, '\n');
    p->token = TOML_TOK_NEWLINE;
    return p->token;
  }

  if (c == '#') {
    skip_comment(p);
    return toml_next_token(p); /* Recurse after comment */
  }

  if (c == '[') {
    p->pos++;
    p->col++;
    p->token = TOML_TOK_TABLE_START;
    return p->token;
  }

  if (c == ']') {
    p->pos++;
    p->col++;
    return toml_next_token(p); /* Skip closing bracket */
  }

  if (c == '=') {
    consume_char(p, '=');
    p->token = TOML_TOK_EQUAL;
    return p->token;
  }

  /* Try boolean first */
  if (c == 't' || c == 'f') {
    size_t saved_pos = p->pos;
    if (parse_boolean(p)) {
      p->token = TOML_TOK_BOOLEAN;
      return p->token;
    }
    p->pos = saved_pos;
  }

  /* Try integer/float */
  if (is_digit(c) || c == '-') {
    size_t saved_pos = p->pos;
    if (parse_float(p)) {
      p->token = TOML_TOK_FLOAT;
      return p->token;
    }
    p->pos = saved_pos;
    if (parse_integer(p)) {
      p->token = TOML_TOK_INTEGER;
      return p->token;
    }
    p->pos = saved_pos;
  }

  /* Try string/key */
  if (is_alpha(c) || c == '"') {
    if (parse_string(p)) {
      /* Check if it's a key (before =) or value */
      skip_whitespace(p);
      if (peek_char(p, &c) && c == '=') {
        /* It's a key */
        if (p->str_len < sizeof(p->key_buf)) {
          memcpy(p->key_buf, p->str_buf, p->str_len);
          p->key_buf[p->str_len] = '\0';
          p->key_len = p->str_len;
        }
        p->token = TOML_TOK_KEY;
        return p->token;
      } else {
        /* It's a string value */
        p->token = TOML_TOK_STRING;
        return p->token;
      }
    }
  }

  p->token = TOML_TOK_ERROR;
  return p->token;
}
