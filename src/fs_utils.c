/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

#define _POSIX_C_SOURCE 200809L

#include "fs_utils.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

spkt_status_t spkt_mkdir_p(const char *path, mode_t mode) {
    if (!path || *path == '\0') {
        return SPKT_ERR_INVALID_PARAM;
    }

    /* We need a mutable copy of the path */
    char temp_path[PATH_MAX];
    if (strlen(path) >= sizeof(temp_path)) {
        return SPKT_ERR_INVALID_PARAM;
    }
    
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *p = temp_path;
    
    /* Skip leading slash */
    if (*p == '/') {
        p++;
    }

    while (*p) {
        /* Find next slash or end of string */
        while (*p && *p != '/') {
            p++;
        }

        /* Temporarily terminate string here */
        char saved_char = *p;
        *p = '\0';

        /* Try to create directory */
        if (mkdir(temp_path, mode) != 0) {
            if (errno != EEXIST) {
                /* If it fails and it's not because it exists, return error */
                fprintf(stderr, "fs_utils: mkdir failed for %s: %s\n", temp_path, strerror(errno));
                return SPKT_ERR_FS_CREATE;
            }
            
            /* If it exists, check if it is a directory */
            struct stat st;
            if (stat(temp_path, &st) == 0 && !S_ISDIR(st.st_mode)) {
                fprintf(stderr, "fs_utils: path component %s exists but is not a directory\n", temp_path);
                return SPKT_ERR_FS_NOT_DIR;
            }
        }

        /* Restore slash and continue */
        *p = saved_char;
        if (*p) {
            p++;
        }
    }

    return SPKT_OK;
}
