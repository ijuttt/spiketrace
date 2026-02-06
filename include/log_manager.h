#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include "config.h"
#include "spkt_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/*
 * Log Manager Module
 *
 * Provides safe log file cleanup operations with:
 * - Race condition prevention via file locking
 * - Atomic operations to avoid partial deletes
 * - Configurable retention policies
 */

/* Maximum log files to track in a single cleanup operation */
#define LOG_MANAGER_MAX_FILES 1024

/* Log file pattern for spike dumps */
#define LOG_FILE_PATTERN "spike_*.json"

/* Log file info for sorting/filtering */
typedef struct {
  char filepath[256];
  time_t mtime;        /* Modification time */
  uint64_t size_bytes; /* File size */
  bool valid;
} log_file_info_t;

/* Log manager context */
typedef struct {
  char log_directory[256];
  uint64_t last_cleanup_ns; /* Monotonic timestamp of last cleanup */
  bool initialized;
} log_manager_ctx_t;

/* Initialize log manager with output directory
 * Returns SPKT_OK on success, error code on failure.
 */
spkt_status_t log_manager_init(log_manager_ctx_t *ctx, const char *log_dir);

/* Cleanup log manager context */
void log_manager_cleanup(log_manager_ctx_t *ctx);

/* Manual deletion of specific log file(s)
 * pattern can be:
 *   - Absolute path to single file
 *   - Glob pattern like "spike_2024-01-*.json"
 *   - "all" to delete all .json log files
 * Returns number of files deleted via out_deleted_count
 */
spkt_status_t log_manager_delete_manual(log_manager_ctx_t *ctx,
                                        const char *pattern,
                                        size_t *out_deleted_count);

/* Automatic cleanup based on configured policy
 * Should be called periodically from main loop
 * current_ns: Current monotonic timestamp
 * Returns SPKT_OK even if no files deleted (not an error)
 */
spkt_status_t log_manager_auto_cleanup(log_manager_ctx_t *ctx,
                                       const spkt_config_t *config,
                                       uint64_t current_ns,
                                       size_t *out_deleted_count);

/* Force immediate cleanup (ignores interval, useful for manual trigger)
 * Applies the configured cleanup policy immediately
 */
spkt_status_t log_manager_run_cleanup(log_manager_ctx_t *ctx,
                                      const spkt_config_t *config,
                                      size_t *out_deleted_count);

/* Get statistics about log files in the output directory */
spkt_status_t log_manager_get_stats(log_manager_ctx_t *ctx,
                                    size_t *out_file_count,
                                    uint64_t *out_total_size_bytes);

#endif
