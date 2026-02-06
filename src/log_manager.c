#define _POSIX_C_SOURCE 200809L

#include "log_manager.h"
#include "time_utils.h"
#include "fs_utils.h" /* [NEW] */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

/* ===== CONSTANTS ===== */

/* Nanoseconds per minute for interval calculation */
#define NS_PER_MINUTE (60ULL * 1000000000ULL)

/* Seconds per day for age calculation */
#define SECONDS_PER_DAY (24 * 60 * 60)

/* ===== INTERNAL HELPERS ===== */

/* Safe file deletion with exclusive locking to prevent race conditions */
static spkt_status_t safe_delete_file(const char *filepath) {
  if (!filepath) {
    return SPKT_ERR_NULL_POINTER;
  }

  /* Open file to acquire lock */
  int fd = open(filepath, O_RDONLY);
  if (fd < 0) {
    if (errno == ENOENT) {
      return SPKT_OK; /* File already deleted, not an error */
    }
    return SPKT_ERR_LOG_DIR_ACCESS;
  }

  /* Try to acquire exclusive lock (non-blocking)
   * If file is being written, this will fail immediately */
  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    close(fd);
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return SPKT_ERR_LOG_FILE_IN_USE;
    }
    return SPKT_ERR_LOG_DELETE_FAILED;
  }

  /* Delete file while holding exclusive lock */
  int result = unlink(filepath);
  spkt_status_t status = (result == 0) ? SPKT_OK : SPKT_ERR_LOG_DELETE_FAILED;

  /* Release lock and close */
  flock(fd, LOCK_UN);
  close(fd);

  return status;
}

/* Comparison function for sorting by modification time (oldest first) */
static int compare_by_mtime_asc(const void *a, const void *b) {
  const log_file_info_t *fa = (const log_file_info_t *)a;
  const log_file_info_t *fb = (const log_file_info_t *)b;

  if (!fa->valid && !fb->valid)
    return 0;
  if (!fa->valid)
    return 1;
  if (!fb->valid)
    return -1;

  if (fa->mtime < fb->mtime)
    return -1;
  if (fa->mtime > fb->mtime)
    return 1;
  return 0;
}

/* Comparison function for sorting by modification time (newest first) */
static int compare_by_mtime_desc(const void *a, const void *b) {
  return -compare_by_mtime_asc(a, b);
}

/* Scan directory for log files matching pattern */
static spkt_status_t scan_log_files(const char *directory, const char *pattern,
                                    log_file_info_t *files, size_t max_files,
                                    size_t *out_count) {
  if (!directory || !files || !out_count) {
    return SPKT_ERR_NULL_POINTER;
  }

  *out_count = 0;

  DIR *dir = opendir(directory);
  if (!dir) {
    fprintf(stderr, "log_manager: cannot open directory '%s': %s\n", directory,
            strerror(errno));
    return SPKT_ERR_LOG_DIR_ACCESS;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL && *out_count < max_files) {
    /* Skip . and .. entries */
    if (entry->d_name[0] == '.') {
      continue;
    }

    /* Match pattern (or all spike_*.json files if pattern is NULL) */
    const char *match_pattern = pattern ? pattern : LOG_FILE_PATTERN;
    if (fnmatch(match_pattern, entry->d_name, 0) != 0) {
      continue;
    }

    /* Build full path */
    log_file_info_t *info = &files[*out_count];
    int written = snprintf(info->filepath, sizeof(info->filepath), "%s/%s",
                           directory, entry->d_name);
    if (written < 0 || (size_t)written >= sizeof(info->filepath)) {
      continue; /* Path too long, skip */
    }

    /* Get file stats */
    struct stat st;
    if (stat(info->filepath, &st) != 0) {
      continue; /* Cannot stat, skip */
    }

    /* Only regular files */
    if (!S_ISREG(st.st_mode)) {
      continue;
    }

    info->mtime = st.st_mtime;
    info->size_bytes = (uint64_t)st.st_size;
    info->valid = true;
    (*out_count)++;
  }

  closedir(dir);
  return SPKT_OK;
}

/* Delete files by age policy */
static size_t cleanup_by_age(log_file_info_t *files, size_t count,
                             uint32_t max_age_days) {
  time_t now = time(NULL);
  time_t max_age_seconds = (time_t)max_age_days * SECONDS_PER_DAY;
  size_t deleted = 0;

  for (size_t i = 0; i < count; i++) {
    if (!files[i].valid)
      continue;

    time_t age = now - files[i].mtime;
    if (age > max_age_seconds) {
      spkt_status_t s = safe_delete_file(files[i].filepath);
      if (s == SPKT_OK) {
        fprintf(stderr, "log_manager: deleted old log: %s (age: %ld days)\n",
                files[i].filepath, (long)(age / SECONDS_PER_DAY));
        files[i].valid = false;
        deleted++;
      } else if (s == SPKT_ERR_LOG_FILE_IN_USE) {
        fprintf(stderr, "log_manager: skipping in-use file: %s\n",
                files[i].filepath);
      }
    }
  }

  return deleted;
}

/* Delete files by count policy (keep N newest) */
static size_t cleanup_by_count(log_file_info_t *files, size_t count,
                               uint32_t max_count) {
  if (count <= max_count) {
    return 0; /* Nothing to delete */
  }

  /* Sort by mtime descending (newest first) */
  qsort(files, count, sizeof(log_file_info_t), compare_by_mtime_desc);

  size_t deleted = 0;
  size_t to_delete = count - max_count;

  /* Delete oldest files (at the end of sorted array) */
  for (size_t i = 0; i < to_delete; i++) {
    size_t idx = count - 1 - i;
    if (!files[idx].valid)
      continue;

    spkt_status_t s = safe_delete_file(files[idx].filepath);
    if (s == SPKT_OK) {
      fprintf(stderr, "log_manager: deleted excess log: %s\n",
              files[idx].filepath);
      files[idx].valid = false;
      deleted++;
    } else if (s == SPKT_ERR_LOG_FILE_IN_USE) {
      fprintf(stderr, "log_manager: skipping in-use file: %s\n",
              files[idx].filepath);
    }
  }

  return deleted;
}

/* Delete files by size policy (keep total under N MiB) */
static size_t cleanup_by_size(log_file_info_t *files, size_t count,
                              uint32_t max_total_size_mib) {
  /* Calculate current total size */
  uint64_t total_bytes = 0;
  for (size_t i = 0; i < count; i++) {
    if (files[i].valid) {
      total_bytes += files[i].size_bytes;
    }
  }

  uint64_t max_bytes = (uint64_t)max_total_size_mib * 1024 * 1024;
  if (total_bytes <= max_bytes) {
    return 0; /* Under limit */
  }

  /* Sort by mtime ascending (oldest first) */
  qsort(files, count, sizeof(log_file_info_t), compare_by_mtime_asc);

  size_t deleted = 0;
  uint64_t bytes_to_free = total_bytes - max_bytes;
  uint64_t bytes_freed = 0;

  for (size_t i = 0; i < count && bytes_freed < bytes_to_free; i++) {
    if (!files[i].valid)
      continue;

    spkt_status_t s = safe_delete_file(files[i].filepath);
    if (s == SPKT_OK) {
      fprintf(stderr, "log_manager: deleted for size: %s (%lu bytes)\n",
              files[i].filepath, (unsigned long)files[i].size_bytes);
      bytes_freed += files[i].size_bytes;
      files[i].valid = false;
      deleted++;
    } else if (s == SPKT_ERR_LOG_FILE_IN_USE) {
      fprintf(stderr, "log_manager: skipping in-use file: %s\n",
              files[i].filepath);
    }
  }

  return deleted;
}

/* ===== PUBLIC API ===== */

spkt_status_t log_manager_init(log_manager_ctx_t *ctx, const char *log_dir) {
  if (!ctx) {
    return SPKT_ERR_NULL_POINTER;
  }

  memset(ctx, 0, sizeof(*ctx));

  if (!log_dir || strlen(log_dir) == 0) {
    return SPKT_ERR_INVALID_PARAM;
  }

  if (strlen(log_dir) >= sizeof(ctx->log_directory)) {
    return SPKT_ERR_INVALID_PARAM;
  }

  strncpy(ctx->log_directory, log_dir, sizeof(ctx->log_directory) - 1);
  ctx->log_directory[sizeof(ctx->log_directory) - 1] = '\0';

  /* Remove trailing slash if present */
  size_t len = strlen(ctx->log_directory);
  if (len > 0 && ctx->log_directory[len - 1] == '/') {
    ctx->log_directory[len - 1] = '\0';
  }

  /* Ensure directory exists */
  spkt_mkdir_p(ctx->log_directory, 0755);

  /* Verify directory exists and is accessible */
  if (access(ctx->log_directory, R_OK | W_OK) != 0) {
    fprintf(stderr, "log_manager: directory '%s' not accessible: %s\n",
            ctx->log_directory, strerror(errno));
    return SPKT_ERR_LOG_DIR_ACCESS;
  }

  ctx->last_cleanup_ns = 0;
  ctx->initialized = true;

  return SPKT_OK;
}

void log_manager_cleanup(log_manager_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
}

spkt_status_t log_manager_delete_manual(log_manager_ctx_t *ctx,
                                        const char *pattern,
                                        size_t *out_deleted_count) {
  if (!ctx || !ctx->initialized) {
    return SPKT_ERR_INVALID_PARAM;
  }

  if (!pattern) {
    return SPKT_ERR_NULL_POINTER;
  }

  size_t deleted = 0;

  /* Handle "all" keyword */
  const char *use_pattern = pattern;
  if (strcmp(pattern, "all") == 0) {
    use_pattern = "*.json";
  }

  /* Check if pattern is absolute path (single file) */
  if (pattern[0] == '/') {
    spkt_status_t s = safe_delete_file(pattern);
    if (s == SPKT_OK) {
      fprintf(stderr, "log_manager: manually deleted: %s\n", pattern);
      deleted = 1;
    } else if (s == SPKT_ERR_LOG_FILE_IN_USE) {
      fprintf(stderr, "log_manager: file in use, cannot delete: %s\n", pattern);
    }
    if (out_deleted_count)
      *out_deleted_count = deleted;
    return s;
  }

  /* Pattern matching within log directory */
  log_file_info_t *files =
      malloc(LOG_MANAGER_MAX_FILES * sizeof(log_file_info_t));
  if (!files) {
    return SPKT_ERR_OUT_OF_MEMORY;
  }

  size_t file_count = 0;
  spkt_status_t s = scan_log_files(ctx->log_directory, use_pattern, files,
                                   LOG_MANAGER_MAX_FILES, &file_count);
  if (s != SPKT_OK) {
    free(files);
    return s;
  }

  for (size_t i = 0; i < file_count; i++) {
    if (!files[i].valid)
      continue;

    spkt_status_t del_s = safe_delete_file(files[i].filepath);
    if (del_s == SPKT_OK) {
      fprintf(stderr, "log_manager: manually deleted: %s\n", files[i].filepath);
      deleted++;
    } else if (del_s == SPKT_ERR_LOG_FILE_IN_USE) {
      fprintf(stderr, "log_manager: skipping in-use: %s\n", files[i].filepath);
    }
  }

  free(files);

  if (out_deleted_count)
    *out_deleted_count = deleted;
  return SPKT_OK;
}

spkt_status_t log_manager_auto_cleanup(log_manager_ctx_t *ctx,
                                       const spkt_config_t *config,
                                       uint64_t current_ns,
                                       size_t *out_deleted_count) {
  if (!ctx || !ctx->initialized || !config) {
    return SPKT_ERR_INVALID_PARAM;
  }

  if (out_deleted_count)
    *out_deleted_count = 0;

  /* Check if auto cleanup is enabled */
  if (!config->enable_auto_cleanup) {
    return SPKT_OK;
  }

  /* Check if cleanup policy is disabled */
  if (config->cleanup_policy == LOG_CLEANUP_DISABLED) {
    return SPKT_OK;
  }

  /* Check cleanup interval */
  uint64_t interval_ns =
      (uint64_t)config->cleanup_interval_minutes * NS_PER_MINUTE;
  if (ctx->last_cleanup_ns > 0 && current_ns > ctx->last_cleanup_ns) {
    uint64_t elapsed = current_ns - ctx->last_cleanup_ns;
    if (elapsed < interval_ns) {
      return SPKT_OK; /* Not time yet */
    }
  }

  /* Run cleanup */
  size_t deleted = 0;
  spkt_status_t s = log_manager_run_cleanup(ctx, config, &deleted);

  /* Update last cleanup time */
  ctx->last_cleanup_ns = current_ns;

  if (out_deleted_count)
    *out_deleted_count = deleted;
  return s;
}

spkt_status_t log_manager_run_cleanup(log_manager_ctx_t *ctx,
                                      const spkt_config_t *config,
                                      size_t *out_deleted_count) {
  if (!ctx || !ctx->initialized || !config) {
    return SPKT_ERR_INVALID_PARAM;
  }

  size_t deleted = 0;

  /* Scan for log files */
  log_file_info_t *files =
      malloc(LOG_MANAGER_MAX_FILES * sizeof(log_file_info_t));
  if (!files) {
    return SPKT_ERR_OUT_OF_MEMORY;
  }

  size_t file_count = 0;
  spkt_status_t s = scan_log_files(ctx->log_directory, NULL, files,
                                   LOG_MANAGER_MAX_FILES, &file_count);
  if (s != SPKT_OK) {
    free(files);
    return s;
  }

  if (file_count == 0) {
    free(files);
    if (out_deleted_count)
      *out_deleted_count = 0;
    return SPKT_OK;
  }

  /* Apply cleanup policy */
  switch (config->cleanup_policy) {
  case LOG_CLEANUP_BY_AGE:
    deleted = cleanup_by_age(files, file_count, config->log_max_age_days);
    break;

  case LOG_CLEANUP_BY_COUNT:
    deleted = cleanup_by_count(files, file_count, config->log_max_count);
    break;

  case LOG_CLEANUP_BY_SIZE:
    deleted =
        cleanup_by_size(files, file_count, config->log_max_total_size_mib);
    break;

  case LOG_CLEANUP_DISABLED:
  default:
    break;
  }

  free(files);

  if (out_deleted_count)
    *out_deleted_count = deleted;
  return SPKT_OK;
}

spkt_status_t log_manager_get_stats(log_manager_ctx_t *ctx,
                                    size_t *out_file_count,
                                    uint64_t *out_total_size_bytes) {
  if (!ctx || !ctx->initialized) {
    return SPKT_ERR_INVALID_PARAM;
  }

  /* Scan for log files */
  log_file_info_t *files =
      malloc(LOG_MANAGER_MAX_FILES * sizeof(log_file_info_t));
  if (!files) {
    return SPKT_ERR_OUT_OF_MEMORY;
  }

  size_t file_count = 0;
  spkt_status_t s = scan_log_files(ctx->log_directory, NULL, files,
                                   LOG_MANAGER_MAX_FILES, &file_count);
  if (s != SPKT_OK) {
    free(files);
    return s;
  }

  uint64_t total_size = 0;
  for (size_t i = 0; i < file_count; i++) {
    if (files[i].valid) {
      total_size += files[i].size_bytes;
    }
  }

  free(files);

  if (out_file_count)
    *out_file_count = file_count;
  if (out_total_size_bytes)
    *out_total_size_bytes = total_size;

  return SPKT_OK;
}
