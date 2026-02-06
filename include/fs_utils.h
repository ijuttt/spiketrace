/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#ifndef FS_UTILS_H
#define FS_UTILS_H

#include "spkt_common.h"
#include <sys/types.h>

/**
 * @brief Recursively create directories (mkdir -p)
 *
 * @param path The path to create
 * @param mode Permission mode for new directories (e.g., 0755)
 * @return SPKT_OK on success, error code otherwise
 */
spkt_status_t spkt_mkdir_p(const char *path, mode_t mode);

#endif
