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

/* ===== CONFIG PARSING ===== */

static void parse_table_header(toml_parser_t *p, char *section,
                               size_t section_size) {
  section[0] = '\0';
  if (p->token != TOML_TOK_TABLE_START) {
    return;
  }

  toml_next_token(p); /* Skip [ */

  size_t section_len = 0;
  while (p->token == TOML_TOK_STRING || p->token == TOML_TOK_KEY) {
    if (section_len > 0 && section_len < section_size - 1) {
      section[section_len++] = '.';
    }
    size_t copy_len = p->str_len;
    if (copy_len > section_size - section_len - 1) {
      copy_len = section_size - section_len - 1;
    }
    memcpy(&section[section_len], p->str_buf, copy_len);
    section_len += copy_len;
    toml_next_token(p);
  }
  section[section_len] = '\0';
}

static bool parse_key_value(toml_parser_t *p, char *key, size_t key_size,
                            char *section, size_t section_size) {
  key[0] = '\0';
  section[0] = '\0';

  if (p->token == TOML_TOK_TABLE_START) {
    parse_table_header(p, section, section_size);
    toml_next_token(p); /* Consume newline after table */
    return false;       /* Not a key-value pair */
  }

  if (p->token != TOML_TOK_KEY) {
    return false;
  }

  if (p->key_len < key_size) {
    memcpy(key, p->key_buf, p->key_len);
    key[p->key_len] = '\0';
  } else {
    return false; /* Key too long */
  }

  toml_next_token(p); /* Consume key */
  skip_whitespace(p);

  if (p->token != TOML_TOK_EQUAL) {
    return false;
  }

  toml_next_token(p); /* Consume = */
  skip_whitespace(p);

  return true;
}

/* Apply config value */
static void apply_config_value(spkt_config_t *config, const char *section,
                               const char *key, toml_parser_t *p) {
  if (strcmp(section, "anomaly_detection") == 0) {
    if (strcmp(key, "cpu_delta_threshold_pct") == 0 &&
        p->token == TOML_TOK_FLOAT) {
      config->cpu_delta_threshold_pct = p->float_val;
    } else if (strcmp(key, "new_process_threshold_pct") == 0 &&
               p->token == TOML_TOK_FLOAT) {
      config->new_process_threshold_pct = p->float_val;
    } else if (strcmp(key, "mem_drop_threshold_mib") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->mem_drop_threshold_kib =
          (uint64_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val) *
          1024;
    } else if (strcmp(key, "mem_pressure_threshold_pct") == 0 &&
               p->token == TOML_TOK_FLOAT) {
      config->mem_pressure_threshold_pct = p->float_val;
    } else if (strcmp(key, "swap_spike_threshold_mib") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->swap_spike_threshold_kib =
          (uint64_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val) *
          1024;
    } else if (strcmp(key, "cooldown_seconds") == 0 &&
               p->token == TOML_TOK_FLOAT) {
      config->cooldown_seconds = p->float_val;
    }
  } else if (strcmp(section, "sampling") == 0) {
    if (strcmp(key, "sampling_interval_seconds") == 0 &&
        p->token == TOML_TOK_FLOAT) {
      config->sampling_interval_seconds = p->float_val;
    } else if (strcmp(key, "ring_buffer_capacity") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->ring_buffer_capacity =
          (uint32_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val);
    } else if (strcmp(key, "context_snapshots_per_dump") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->context_snapshots_per_dump =
          (uint32_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val);
    }
  } else if (strcmp(section, "process_collection") == 0) {
    if (strcmp(key, "max_processes_tracked") == 0 &&
        (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->max_processes_tracked =
          (uint32_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val);
    } else if (strcmp(key, "top_processes_stored") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->top_processes_stored =
          (uint32_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val);
    }
  } else if (strcmp(section, "output") == 0) {
    if (strcmp(key, "output_directory") == 0 && p->token == TOML_TOK_STRING) {
      size_t copy_len = p->str_len;
      if (copy_len >= sizeof(config->output_directory)) {
        copy_len = sizeof(config->output_directory) - 1;
      }
      memcpy(config->output_directory, p->str_buf, copy_len);
      config->output_directory[copy_len] = '\0';
    }
  } else if (strcmp(section, "features") == 0) {
    if (strcmp(key, "enable_cpu_detection") == 0 &&
        p->token == TOML_TOK_BOOLEAN) {
      config->enable_cpu_detection = p->bool_val;
    } else if (strcmp(key, "enable_memory_detection") == 0 &&
               p->token == TOML_TOK_BOOLEAN) {
      config->enable_memory_detection = p->bool_val;
    } else if (strcmp(key, "enable_swap_detection") == 0 &&
               p->token == TOML_TOK_BOOLEAN) {
      config->enable_swap_detection = p->bool_val;
    } else if (strcmp(key, "aggregate_related_processes") == 0 &&
               p->token == TOML_TOK_BOOLEAN) {
      config->aggregate_related_processes = p->bool_val;
    }
  } else if (strcmp(section, "advanced") == 0) {
    if (strcmp(key, "memory_baseline_alpha") == 0 &&
        p->token == TOML_TOK_FLOAT) {
      config->memory_baseline_alpha = p->float_val;
    } else if (strcmp(key, "process_baseline_alpha") == 0 &&
               p->token == TOML_TOK_FLOAT) {
      config->process_baseline_alpha = p->float_val;
    }
  } else if (strcmp(section, "trigger") == 0) {
    if (strcmp(key, "scope") == 0 && p->token == TOML_TOK_STRING) {
      if (strcmp(p->str_buf, "per_process") == 0) {
        config->trigger_scope = TRIGGER_SCOPE_PROCESS;
      } else if (strcmp(p->str_buf, "process_group") == 0) {
        config->trigger_scope = TRIGGER_SCOPE_PROCESS_GROUP;
      } else if (strcmp(p->str_buf, "parent") == 0) {
        config->trigger_scope = TRIGGER_SCOPE_PARENT;
      } else if (strcmp(p->str_buf, "system") == 0) {
        config->trigger_scope = TRIGGER_SCOPE_SYSTEM;
      }
    }
  } else if (strcmp(section, "log_management") == 0) {
    if (strcmp(key, "enable_auto_cleanup") == 0 &&
        p->token == TOML_TOK_BOOLEAN) {
      config->enable_auto_cleanup = p->bool_val;
    } else if (strcmp(key, "cleanup_policy") == 0 &&
               p->token == TOML_TOK_STRING) {
      config->cleanup_policy = log_cleanup_policy_from_string(p->str_buf);
    } else if (strcmp(key, "log_max_age_days") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->log_max_age_days =
          (uint32_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val);
    } else if (strcmp(key, "log_max_count") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->log_max_count =
          (uint32_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val);
    } else if (strcmp(key, "log_max_total_size_mib") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->log_max_total_size_mib =
          (uint32_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val);
    } else if (strcmp(key, "cleanup_interval_minutes") == 0 &&
               (p->token == TOML_TOK_INTEGER || p->token == TOML_TOK_FLOAT)) {
      config->cleanup_interval_minutes =
          (uint32_t)(p->token == TOML_TOK_INTEGER ? p->int_val : p->float_val);
    }
  }
}

/* ===== PATH SECURITY ===== */

/* Check if path is absolute and contains no parent directory traversal */
static bool is_safe_absolute_path(const char *path) {
  if (!path || path[0] != '/') {
    return false; /* Must be absolute */
  }

  /* Check for parent directory traversal */
  if (strstr(path, "/../") != NULL || strstr(path, "../") != NULL) {
    return false;
  }

  /* Check for symlink components (basic check) */
  /* Full symlink resolution would require realpath, but we validate on use */

  return true;
}

/* ===== PUBLIC API ===== */

void config_init_defaults(spkt_config_t *config) {
  if (!config) {
    return;
  }

  memset(config, 0, sizeof(*config));

  config->cpu_delta_threshold_pct = DEFAULT_CPU_DELTA_THRESHOLD_PCT;
  config->new_process_threshold_pct = DEFAULT_NEW_PROCESS_THRESHOLD_PCT;
  config->mem_drop_threshold_kib = DEFAULT_MEM_DROP_THRESHOLD_MIB * 1024;
  config->mem_pressure_threshold_pct = DEFAULT_MEM_PRESSURE_THRESHOLD_PCT;
  config->swap_spike_threshold_kib = DEFAULT_SWAP_SPIKE_THRESHOLD_MIB * 1024;
  config->cooldown_seconds = DEFAULT_COOLDOWN_SECONDS;

  config->sampling_interval_seconds = DEFAULT_SAMPLING_INTERVAL_SECONDS;
  config->ring_buffer_capacity = DEFAULT_RING_BUFFER_CAPACITY;
  config->context_snapshots_per_dump = DEFAULT_CONTEXT_SNAPSHOTS_PER_DUMP;

  config->max_processes_tracked = DEFAULT_MAX_PROCESSES_TRACKED;
  config->top_processes_stored = DEFAULT_TOP_PROCESSES_STORED;

  strncpy(config->output_directory, DEFAULT_OUTPUT_DIRECTORY,
          sizeof(config->output_directory) - 1);
  config->output_directory[sizeof(config->output_directory) - 1] = '\0';

  config->enable_cpu_detection = true;
  config->enable_memory_detection = true;
  config->enable_swap_detection = true;
  config->aggregate_related_processes = false;

  config->memory_baseline_alpha = DEFAULT_MEMORY_BASELINE_ALPHA;
  config->process_baseline_alpha = DEFAULT_PROCESS_BASELINE_ALPHA;

  config->trigger_scope = TRIGGER_SCOPE_PROCESS;

  /* Log management defaults */
  config->enable_auto_cleanup = DEFAULT_ENABLE_AUTO_CLEANUP;
  config->cleanup_policy = DEFAULT_LOG_CLEANUP_POLICY;
  config->log_max_age_days = DEFAULT_LOG_MAX_AGE_DAYS;
  config->log_max_count = DEFAULT_LOG_MAX_COUNT;
  config->log_max_total_size_mib = DEFAULT_LOG_MAX_TOTAL_SIZE_MIB;
  config->cleanup_interval_minutes = DEFAULT_CLEANUP_INTERVAL_MINUTES;

  config->loaded = false;
}

spkt_status_t config_get_default_path(char *path, size_t path_size) {
  if (!path || path_size == 0) {
    return SPKT_ERR_NULL_POINTER;
  }

  const char *home = getenv("HOME");
  if (!home) {
    /* Try getpwuid as fallback */
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
      home = pw->pw_dir;
    }
  }

  if (!home) {
    return SPKT_ERR_INVALID_PARAM; /* Cannot determine home directory */
  }

  int written =
      snprintf(path, path_size, "%s/.config/spiketrace/config.toml", home);
  if (written < 0 || (size_t)written >= path_size) {
    return SPKT_ERR_INVALID_PARAM;
  }

  return SPKT_OK;
}

bool config_file_exists(const char *path) {
  if (!path) {
    return false;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    return false;
  }

  return S_ISREG(st.st_mode);
}

spkt_status_t config_load(spkt_config_t *config, const char *config_path) {
  if (!config) {
    return SPKT_ERR_NULL_POINTER;
  }

  /* Initialize with defaults first */
  config_init_defaults(config);

  /* Determine config file path */
  char file_path[CONFIG_MAX_PATH_LEN];
  bool found = false;

  if (config_path) {
    /* Explicit path provided */
    if (strlen(config_path) >= sizeof(file_path)) {
      fprintf(stderr, "config: config path too long\n");
      return SPKT_OK; /* Use defaults */
    }
    strncpy(file_path, config_path, sizeof(file_path) - 1);
    file_path[sizeof(file_path) - 1] = '\0';
    found = config_file_exists(file_path);
  } else {
    /* Check system-wide config first (for daemon/root execution) */
    if (config_file_exists(CONFIG_SYSTEM_PATH)) {
      strncpy(file_path, CONFIG_SYSTEM_PATH, sizeof(file_path) - 1);
      file_path[sizeof(file_path) - 1] = '\0';
      found = true;
    } else {
      /* Fall back to user config */
      spkt_status_t s = config_get_default_path(file_path, sizeof(file_path));
      if (s == SPKT_OK) {
        found = config_file_exists(file_path);
      }
    }
  }

  /* Check if file exists */
  if (!found) {
    /* Missing config is normal - use defaults */
    return SPKT_OK;
  }

  /* Open and read file */
  FILE *fp = fopen(file_path, "r");
  if (!fp) {
    fprintf(stderr, "config: cannot open %s: %s\n", file_path, strerror(errno));
    return SPKT_OK; /* Use defaults on error */
  }

  /* Get file size */
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    fprintf(stderr, "config: cannot seek in %s\n", file_path);
    return SPKT_OK;
  }

  long file_size = ftell(fp);
  if (file_size < 0 || (size_t)file_size > CONFIG_MAX_FILE_SIZE) {
    fclose(fp);
    fprintf(stderr, "config: file %s too large or invalid\n", file_path);
    return SPKT_OK;
  }

  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    fprintf(stderr, "config: cannot rewind %s\n", file_path);
    return SPKT_OK;
  }

  /* Allocate buffer */
  char *file_data = malloc((size_t)file_size + 1);
  if (!file_data) {
    fclose(fp);
    fprintf(stderr, "config: out of memory reading %s\n", file_path);
    return SPKT_OK;
  }

  size_t read_bytes = fread(file_data, 1, (size_t)file_size, fp);
  fclose(fp);

  if (read_bytes != (size_t)file_size) {
    free(file_data);
    fprintf(stderr, "config: read error in %s\n", file_path);
    return SPKT_OK;
  }

  file_data[read_bytes] = '\0';

  /* Parse TOML */
  toml_parser_t parser = {0};
  parser.data = file_data;
  parser.len = read_bytes;
  parser.pos = 0;
  parser.line = 1;
  parser.col = 1;

  char current_section[128] = "";
  toml_next_token(&parser);

  while (parser.token != TOML_TOK_EOF && parser.token != TOML_TOK_ERROR) {
    char key[128];
    char section[128];

    if (parser.token == TOML_TOK_TABLE_START) {
      parse_table_header(&parser, current_section, sizeof(current_section));
      toml_next_token(&parser);
      continue;
    }

    if (parse_key_value(&parser, key, sizeof(key), section, sizeof(section))) {
      /* Got key-value pair */
      toml_next_token(&parser); /* Get value token */
      apply_config_value(config, current_section, key, &parser);
      toml_next_token(&parser); /* Consume value and advance */
    } else {
      toml_next_token(&parser);
    }
  }

  free(file_data);

  if (parser.token == TOML_TOK_ERROR) {
    fprintf(stderr, "config: parse error in %s at line %d, col %d\n", file_path,
            parser.line, parser.col);
    /* Continue with defaults */
  }

  config->loaded = true;
  return SPKT_OK;
}

spkt_status_t config_validate(spkt_config_t *config) {
  if (!config) {
    return SPKT_ERR_NULL_POINTER;
  }

  bool had_warnings = false;

  /* Validate anomaly detection thresholds */
  if (config->cpu_delta_threshold_pct < MIN_CPU_DELTA_THRESHOLD_PCT ||
      config->cpu_delta_threshold_pct > MAX_CPU_DELTA_THRESHOLD_PCT) {
    fprintf(stderr,
            "config: cpu_delta_threshold_pct out of range (%.1f), clamping to "
            "%.1f\n",
            config->cpu_delta_threshold_pct, DEFAULT_CPU_DELTA_THRESHOLD_PCT);
    config->cpu_delta_threshold_pct = DEFAULT_CPU_DELTA_THRESHOLD_PCT;
    had_warnings = true;
  }

  if (config->new_process_threshold_pct < MIN_NEW_PROCESS_THRESHOLD_PCT ||
      config->new_process_threshold_pct > MAX_NEW_PROCESS_THRESHOLD_PCT) {
    fprintf(
        stderr,
        "config: new_process_threshold_pct out of range (%.1f), clamping to "
        "%.1f\n",
        config->new_process_threshold_pct, DEFAULT_NEW_PROCESS_THRESHOLD_PCT);
    config->new_process_threshold_pct = DEFAULT_NEW_PROCESS_THRESHOLD_PCT;
    had_warnings = true;
  }

  if (config->mem_drop_threshold_kib < MIN_MEM_DROP_THRESHOLD_MIB * 1024 ||
      config->mem_drop_threshold_kib > MAX_MEM_DROP_THRESHOLD_MIB * 1024) {
    fprintf(stderr,
            "config: mem_drop_threshold_mib out of range, clamping to %u\n",
            DEFAULT_MEM_DROP_THRESHOLD_MIB);
    config->mem_drop_threshold_kib = DEFAULT_MEM_DROP_THRESHOLD_MIB * 1024;
    had_warnings = true;
  }

  if (config->mem_pressure_threshold_pct < MIN_MEM_PRESSURE_THRESHOLD_PCT ||
      config->mem_pressure_threshold_pct > MAX_MEM_PRESSURE_THRESHOLD_PCT) {
    fprintf(stderr,
            "config: mem_pressure_threshold_pct out of range (%.1f), clamping "
            "to %.1f\n",
            config->mem_pressure_threshold_pct,
            DEFAULT_MEM_PRESSURE_THRESHOLD_PCT);
    config->mem_pressure_threshold_pct = DEFAULT_MEM_PRESSURE_THRESHOLD_PCT;
    had_warnings = true;
  }

  if (config->swap_spike_threshold_kib < MIN_SWAP_SPIKE_THRESHOLD_MIB * 1024 ||
      config->swap_spike_threshold_kib > MAX_SWAP_SPIKE_THRESHOLD_MIB * 1024) {
    fprintf(stderr,
            "config: swap_spike_threshold_mib out of range, clamping to %u\n",
            DEFAULT_SWAP_SPIKE_THRESHOLD_MIB);
    config->swap_spike_threshold_kib = DEFAULT_SWAP_SPIKE_THRESHOLD_MIB * 1024;
    had_warnings = true;
  }

  if (config->cooldown_seconds < MIN_COOLDOWN_SECONDS ||
      config->cooldown_seconds > MAX_COOLDOWN_SECONDS) {
    fprintf(stderr,
            "config: cooldown_seconds out of range (%.1f), clamping to %.1f\n",
            config->cooldown_seconds, DEFAULT_COOLDOWN_SECONDS);
    config->cooldown_seconds = DEFAULT_COOLDOWN_SECONDS;
    had_warnings = true;
  }

  /* Validate sampling configuration */
  if (config->sampling_interval_seconds < MIN_SAMPLING_INTERVAL_SECONDS ||
      config->sampling_interval_seconds > MAX_SAMPLING_INTERVAL_SECONDS) {
    fprintf(
        stderr,
        "config: sampling_interval_seconds out of range (%.1f), clamping to "
        "%.1f\n",
        config->sampling_interval_seconds, DEFAULT_SAMPLING_INTERVAL_SECONDS);
    config->sampling_interval_seconds = DEFAULT_SAMPLING_INTERVAL_SECONDS;
    had_warnings = true;
  }

  if (config->ring_buffer_capacity < MIN_RING_BUFFER_CAPACITY ||
      config->ring_buffer_capacity > MAX_RING_BUFFER_CAPACITY) {
    fprintf(stderr,
            "config: ring_buffer_capacity out of range (%u), clamping to %u\n",
            config->ring_buffer_capacity, DEFAULT_RING_BUFFER_CAPACITY);
    config->ring_buffer_capacity = DEFAULT_RING_BUFFER_CAPACITY;
    had_warnings = true;
  }

  if (config->context_snapshots_per_dump < MIN_CONTEXT_SNAPSHOTS_PER_DUMP ||
      config->context_snapshots_per_dump > MAX_CONTEXT_SNAPSHOTS_PER_DUMP) {
    fprintf(stderr,
            "config: context_snapshots_per_dump out of range (%u), clamping to "
            "%u\n",
            config->context_snapshots_per_dump,
            DEFAULT_CONTEXT_SNAPSHOTS_PER_DUMP);
    config->context_snapshots_per_dump = DEFAULT_CONTEXT_SNAPSHOTS_PER_DUMP;
    had_warnings = true;
  }

  /* Cross-validation: context_snapshots_per_dump <= ring_buffer_capacity */
  if (config->context_snapshots_per_dump > config->ring_buffer_capacity) {
    fprintf(stderr,
            "config: context_snapshots_per_dump (%u) > ring_buffer_capacity "
            "(%u), clamping to %u\n",
            config->context_snapshots_per_dump, config->ring_buffer_capacity,
            config->ring_buffer_capacity);
    config->context_snapshots_per_dump = config->ring_buffer_capacity;
    had_warnings = true;
  }

  /* Validate process collection */
  if (config->max_processes_tracked < MIN_MAX_PROCESSES_TRACKED ||
      config->max_processes_tracked > MAX_MAX_PROCESSES_TRACKED) {
    fprintf(stderr,
            "config: max_processes_tracked out of range (%u), clamping to %u\n",
            config->max_processes_tracked, DEFAULT_MAX_PROCESSES_TRACKED);
    config->max_processes_tracked = DEFAULT_MAX_PROCESSES_TRACKED;
    had_warnings = true;
  }

  if (config->top_processes_stored < MIN_TOP_PROCESSES_STORED ||
      config->top_processes_stored > MAX_TOP_PROCESSES_STORED) {
    fprintf(stderr,
            "config: top_processes_stored out of range (%u), clamping to %u\n",
            config->top_processes_stored, DEFAULT_TOP_PROCESSES_STORED);
    config->top_processes_stored = DEFAULT_TOP_PROCESSES_STORED;
    had_warnings = true;
  }

  /* Cross-validation: top_processes_stored <= max_processes_tracked */
  if (config->top_processes_stored > config->max_processes_tracked) {
    fprintf(stderr,
            "config: top_processes_stored (%u) > max_processes_tracked (%u), "
            "clamping to %u\n",
            config->top_processes_stored, config->max_processes_tracked,
            config->max_processes_tracked);
    config->top_processes_stored = config->max_processes_tracked;
    had_warnings = true;
  }

  /* Validate output directory */
  if (strlen(config->output_directory) > 0) {
    if (!is_safe_absolute_path(config->output_directory)) {
      fprintf(
          stderr,
          "config: output_directory must be absolute path, using default\n");
      strncpy(config->output_directory, DEFAULT_OUTPUT_DIRECTORY,
              sizeof(config->output_directory) - 1);
      config->output_directory[sizeof(config->output_directory) - 1] = '\0';
      had_warnings = true;
    }
  }

  /* Validate feature toggles - at least one must be enabled */
  if (!config->enable_cpu_detection && !config->enable_memory_detection &&
      !config->enable_swap_detection) {
    fprintf(
        stderr,
        "config: at least one detection type must be enabled, enabling all\n");
    config->enable_cpu_detection = true;
    config->enable_memory_detection = true;
    config->enable_swap_detection = true;
    had_warnings = true;
  }

  /* Validate advanced tuning */
  if (config->memory_baseline_alpha < MIN_BASELINE_ALPHA ||
      config->memory_baseline_alpha > MAX_BASELINE_ALPHA) {
    fprintf(stderr,
            "config: memory_baseline_alpha out of range (%.3f), clamping to "
            "%.3f\n",
            config->memory_baseline_alpha, DEFAULT_MEMORY_BASELINE_ALPHA);
    config->memory_baseline_alpha = DEFAULT_MEMORY_BASELINE_ALPHA;
    had_warnings = true;
  }

  if (config->process_baseline_alpha < MIN_BASELINE_ALPHA ||
      config->process_baseline_alpha > MAX_BASELINE_ALPHA) {
    fprintf(stderr,
            "config: process_baseline_alpha out of range (%.3f), clamping to "
            "%.3f\n",
            config->process_baseline_alpha, DEFAULT_PROCESS_BASELINE_ALPHA);
    config->process_baseline_alpha = DEFAULT_PROCESS_BASELINE_ALPHA;
    had_warnings = true;
  }

  /* Validate log management settings */
  if (config->log_max_age_days < MIN_LOG_MAX_AGE_DAYS ||
      config->log_max_age_days > MAX_LOG_MAX_AGE_DAYS) {
    fprintf(stderr,
            "config: log_max_age_days out of range (%u), clamping to %u\n",
            config->log_max_age_days, DEFAULT_LOG_MAX_AGE_DAYS);
    config->log_max_age_days = DEFAULT_LOG_MAX_AGE_DAYS;
    had_warnings = true;
  }

  if (config->log_max_count < MIN_LOG_MAX_COUNT ||
      config->log_max_count > MAX_LOG_MAX_COUNT) {
    fprintf(stderr, "config: log_max_count out of range (%u), clamping to %u\n",
            config->log_max_count, DEFAULT_LOG_MAX_COUNT);
    config->log_max_count = DEFAULT_LOG_MAX_COUNT;
    had_warnings = true;
  }

  if (config->log_max_total_size_mib < MIN_LOG_MAX_TOTAL_SIZE_MIB ||
      config->log_max_total_size_mib > MAX_LOG_MAX_TOTAL_SIZE_MIB) {
    fprintf(
        stderr,
        "config: log_max_total_size_mib out of range (%u), clamping to %u\n",
        config->log_max_total_size_mib, DEFAULT_LOG_MAX_TOTAL_SIZE_MIB);
    config->log_max_total_size_mib = DEFAULT_LOG_MAX_TOTAL_SIZE_MIB;
    had_warnings = true;
  }

  if (config->cleanup_interval_minutes < MIN_CLEANUP_INTERVAL_MINUTES ||
      config->cleanup_interval_minutes > MAX_CLEANUP_INTERVAL_MINUTES) {
    fprintf(
        stderr,
        "config: cleanup_interval_minutes out of range (%u), clamping to %u\n",
        config->cleanup_interval_minutes, DEFAULT_CLEANUP_INTERVAL_MINUTES);
    config->cleanup_interval_minutes = DEFAULT_CLEANUP_INTERVAL_MINUTES;
    had_warnings = true;
  }

  /* Validate numeric values are finite */
  if (config->cpu_delta_threshold_pct != config->cpu_delta_threshold_pct ||
      config->new_process_threshold_pct != config->new_process_threshold_pct ||
      config->mem_pressure_threshold_pct !=
          config->mem_pressure_threshold_pct ||
      config->cooldown_seconds != config->cooldown_seconds ||
      config->sampling_interval_seconds != config->sampling_interval_seconds ||
      config->memory_baseline_alpha != config->memory_baseline_alpha ||
      config->process_baseline_alpha != config->process_baseline_alpha) {
    fprintf(stderr, "config: NaN detected in numeric values, using defaults\n");
    config_init_defaults(config);
    return SPKT_ERR_INVALID_PARAM;
  }

  return had_warnings ? SPKT_OK : SPKT_OK;
}

/* ===== LOG CLEANUP POLICY HELPERS ===== */

const char *log_cleanup_policy_to_string(log_cleanup_policy_t policy) {
  switch (policy) {
  case LOG_CLEANUP_DISABLED:
    return "disabled";
  case LOG_CLEANUP_BY_AGE:
    return "by_age";
  case LOG_CLEANUP_BY_COUNT:
    return "by_count";
  case LOG_CLEANUP_BY_SIZE:
    return "by_size";
  default:
    return "disabled";
  }
}

log_cleanup_policy_t log_cleanup_policy_from_string(const char *str) {
  if (!str) {
    return LOG_CLEANUP_DISABLED;
  }
  if (strcmp(str, "by_age") == 0) {
    return LOG_CLEANUP_BY_AGE;
  }
  if (strcmp(str, "by_count") == 0) {
    return LOG_CLEANUP_BY_COUNT;
  }
  if (strcmp(str, "by_size") == 0) {
    return LOG_CLEANUP_BY_SIZE;
  }
  return LOG_CLEANUP_DISABLED;
}
