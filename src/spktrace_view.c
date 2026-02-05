/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#define _POSIX_C_SOURCE 200809L

#include "json_reader.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_SIZE (16 * 1024 * 1024) /* 16 MiB max */
#define MAX_PROCS 10

/* Parsed trigger data */
typedef struct {
  char type[32];
  int32_t pid;
  char comm[16];
  double cpu_pct;
  double baseline_pct;
  double delta_pct;
  uint64_t mem_available_kib;
  uint64_t mem_baseline_kib;
  int64_t mem_delta_kib;
  double mem_used_pct;
  uint64_t swap_used_kib;
  uint64_t swap_baseline_kib;
  int64_t swap_delta_kib;
} trigger_t;

/* Parsed process entry */
typedef struct {
  int32_t pid;
  char comm[16];
  double cpu_pct;
  uint64_t rss_kib;
} proc_entry_t;

/* Print usage */
static void print_usage(const char *prog) {
  fprintf(stderr, "Usage: %s <spike_dump.json>\n", prog);
  fprintf(stderr, "\nDisplays a human-readable summary of a spiketrace dump.\n");
}

/* Read entire file */
static char *read_file(const char *path, size_t *out_len) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    fprintf(stderr, "Error: cannot open '%s': %s\n", path, strerror(errno));
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  long fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (fsize < 0 || (size_t)fsize > MAX_FILE_SIZE) {
    fprintf(stderr, "Error: file too large or invalid\n");
    fclose(fp);
    return NULL;
  }

  char *buf = malloc((size_t)fsize + 1);
  if (!buf) {
    fprintf(stderr, "Error: out of memory\n");
    fclose(fp);
    return NULL;
  }

  size_t nread = fread(buf, 1, (size_t)fsize, fp);
  fclose(fp);

  buf[nread] = '\0';
  *out_len = nread;
  return buf;
}

/* Parse trigger object */
static void parse_trigger(json_reader_t *r, trigger_t *t) {
  memset(t, 0, sizeof(*t));

  while (json_reader_next(r) != JSON_TOK_OBJECT_END &&
         r->token != JSON_TOK_EOF) {
    if (r->token == JSON_TOK_KEY) {
      if (json_reader_key_equals(r, "type")) {
        json_reader_next(r);
        strncpy(t->type, json_reader_get_string(r), sizeof(t->type) - 1);
      } else if (json_reader_key_equals(r, "pid")) {
        json_reader_next(r);
        t->pid = (int32_t)json_reader_get_int(r);
      } else if (json_reader_key_equals(r, "comm")) {
        json_reader_next(r);
        strncpy(t->comm, json_reader_get_string(r), sizeof(t->comm) - 1);
      } else if (json_reader_key_equals(r, "cpu_pct")) {
        json_reader_next(r);
        t->cpu_pct = json_reader_get_double(r);
      } else if (json_reader_key_equals(r, "baseline_pct")) {
        json_reader_next(r);
        t->baseline_pct = json_reader_get_double(r);
      } else if (json_reader_key_equals(r, "delta_pct")) {
        json_reader_next(r);
        t->delta_pct = json_reader_get_double(r);
      } else if (json_reader_key_equals(r, "mem_available_kib")) {
        json_reader_next(r);
        t->mem_available_kib = json_reader_get_uint(r);
      } else if (json_reader_key_equals(r, "mem_baseline_kib")) {
        json_reader_next(r);
        t->mem_baseline_kib = json_reader_get_uint(r);
      } else if (json_reader_key_equals(r, "mem_delta_kib")) {
        json_reader_next(r);
        t->mem_delta_kib = json_reader_get_int(r);
      } else if (json_reader_key_equals(r, "mem_used_pct")) {
        json_reader_next(r);
        t->mem_used_pct = json_reader_get_double(r);
      } else if (json_reader_key_equals(r, "swap_used_kib")) {
        json_reader_next(r);
        t->swap_used_kib = json_reader_get_uint(r);
      } else if (json_reader_key_equals(r, "swap_baseline_kib")) {
        json_reader_next(r);
        t->swap_baseline_kib = json_reader_get_uint(r);
      } else if (json_reader_key_equals(r, "swap_delta_kib")) {
        json_reader_next(r);
        t->swap_delta_kib = json_reader_get_int(r);
      }
    }
  }
}

/* Parse single process entry */
static void parse_proc_entry(json_reader_t *r, proc_entry_t *p) {
  memset(p, 0, sizeof(*p));

  while (json_reader_next(r) != JSON_TOK_OBJECT_END &&
         r->token != JSON_TOK_EOF) {
    if (r->token == JSON_TOK_KEY) {
      if (json_reader_key_equals(r, "pid")) {
        json_reader_next(r);
        p->pid = (int32_t)json_reader_get_int(r);
      } else if (json_reader_key_equals(r, "comm")) {
        json_reader_next(r);
        strncpy(p->comm, json_reader_get_string(r), sizeof(p->comm) - 1);
      } else if (json_reader_key_equals(r, "cpu_pct")) {
        json_reader_next(r);
        p->cpu_pct = json_reader_get_double(r);
      } else if (json_reader_key_equals(r, "rss_kib")) {
        json_reader_next(r);
        p->rss_kib = json_reader_get_uint(r);
      }
    }
  }
}

/* Parse procs array */
static size_t parse_procs_array(json_reader_t *r, proc_entry_t *procs,
                                size_t max) {
  size_t count = 0;

  while (json_reader_next(r) != JSON_TOK_ARRAY_END &&
         r->token != JSON_TOK_EOF) {
    if (r->token == JSON_TOK_OBJECT_START && count < max) {
      parse_proc_entry(r, &procs[count]);
      count++;
    }
  }

  return count;
}

/* Print trigger summary */
static void print_trigger(const trigger_t *t) {
  printf("\n=== SPIKE TRIGGER ===\n");
  printf("Type: %s\n", t->type);

  if (strcmp(t->type, "cpu_delta") == 0 ||
      strcmp(t->type, "cpu_new_process") == 0) {
    printf("Process: [%d] %s\n", t->pid, t->comm);
    printf("CPU: %.1f%% (baseline: %.1f%%, delta: +%.1f%%)\n", t->cpu_pct,
           t->baseline_pct, t->delta_pct);
  } else if (strcmp(t->type, "mem_drop") == 0) {
    printf("Process: [%d] %s (top RSS)\n", t->pid, t->comm);
    printf("Available: %lu MiB (baseline: %lu MiB, delta: %ld MiB)\n",
           (unsigned long)(t->mem_available_kib / 1024),
           (unsigned long)(t->mem_baseline_kib / 1024),
           (long)(t->mem_delta_kib / 1024));
  } else if (strcmp(t->type, "mem_pressure") == 0) {
    printf("Process: [%d] %s (top RSS)\n", t->pid, t->comm);
    printf("RAM used: %.1f%% (available: %lu MiB)\n", t->mem_used_pct,
           (unsigned long)(t->mem_available_kib / 1024));
  } else if (strcmp(t->type, "swap_spike") == 0) {
    printf("Process: [%d] %s (top RSS)\n", t->pid, t->comm);
    printf("Swap used: %lu MiB (baseline: %lu MiB, delta: +%ld MiB)\n",
           (unsigned long)(t->swap_used_kib / 1024),
           (unsigned long)(t->swap_baseline_kib / 1024),
           (long)(t->swap_delta_kib / 1024));
  }
}

/* Print process list */
static void print_procs(const char *title, const proc_entry_t *procs,
                        size_t count, bool show_rss) {
  printf("\n=== %s ===\n", title);
  for (size_t i = 0; i < count; i++) {
    if (show_rss) {
      printf("%2zu. [%5d] %-15s %6lu MiB  (CPU: %.1f%%)\n", i + 1, procs[i].pid,
             procs[i].comm, (unsigned long)(procs[i].rss_kib / 1024),
             procs[i].cpu_pct);
    } else {
      printf("%2zu. [%5d] %-15s %6.1f%%  (RSS: %lu MiB)\n", i + 1, procs[i].pid,
             procs[i].comm, procs[i].cpu_pct,
             (unsigned long)(procs[i].rss_kib / 1024));
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char *path = argv[1];

  /* Read file */
  size_t len;
  char *data = read_file(path, &len);
  if (!data) {
    return 1;
  }

  /* Parse JSON */
  json_reader_t reader;
  json_reader_init(&reader, data, len);

  trigger_t trigger = {0};
  proc_entry_t cpu_procs[MAX_PROCS] = {0};
  proc_entry_t rss_procs[MAX_PROCS] = {0};
  size_t cpu_count = 0;
  size_t rss_count = 0;
  uint64_t timestamp_ns = 0;
  bool got_first_snapshot = false;

  while (json_reader_next(&reader) != JSON_TOK_EOF) {
    if (reader.token == JSON_TOK_KEY) {
      if (json_reader_key_equals(&reader, "dump_timestamp_ns")) {
        json_reader_next(&reader);
        timestamp_ns = json_reader_get_uint(&reader);
      } else if (json_reader_key_equals(&reader, "trigger")) {
        json_reader_next(&reader);
        if (reader.token == JSON_TOK_OBJECT_START) {
          parse_trigger(&reader, &trigger);
        }
      } else if (json_reader_key_equals(&reader, "snapshots")) {
        json_reader_next(&reader);
        if (reader.token == JSON_TOK_ARRAY_START) {
          /* Parse first snapshot only for top processes */
          while (json_reader_next(&reader) != JSON_TOK_ARRAY_END &&
                 reader.token != JSON_TOK_EOF) {
            if (reader.token == JSON_TOK_OBJECT_START && !got_first_snapshot) {
              /* Parse first snapshot */
              while (json_reader_next(&reader) != JSON_TOK_OBJECT_END &&
                     reader.token != JSON_TOK_EOF) {
                if (reader.token == JSON_TOK_KEY) {
                  if (json_reader_key_equals(&reader, "procs")) {
                    json_reader_next(&reader);
                    if (reader.token == JSON_TOK_ARRAY_START) {
                      cpu_count = parse_procs_array(&reader, cpu_procs, MAX_PROCS);
                    }
                  } else if (json_reader_key_equals(&reader, "top_rss_procs")) {
                    json_reader_next(&reader);
                    if (reader.token == JSON_TOK_ARRAY_START) {
                      rss_count = parse_procs_array(&reader, rss_procs, MAX_PROCS);
                    }
                  }
                }
              }
              got_first_snapshot = true;
            }
          }
        }
      }
    }
  }

  /* Print results */
  printf("Spike Dump: %s\n", path);
  printf("Timestamp (monotonic): %lu ns\n", (unsigned long)timestamp_ns);

  print_trigger(&trigger);

  if (cpu_count > 0) {
    print_procs("TOP PROCESSES BY CPU", cpu_procs, cpu_count, false);
  }

  if (rss_count > 0) {
    print_procs("TOP PROCESSES BY RSS", rss_procs, rss_count, true);
  }

  free(data);
  return 0;
}
