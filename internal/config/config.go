/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

// Package config provides configuration constants and path discovery for spiketrace-view.
package config

import (
	"os"
	"path/filepath"
)

// -----------------------------------------------------------------------------
// Application Constants
// -----------------------------------------------------------------------------

const (
	// AppName is the application identifier.
	AppName = "spiketrace"

	// DefaultDumpDir is the system-wide dump directory.
	DefaultDumpDir = "/var/lib/spiketrace"

	// DumpFileExtension is the expected extension for dump files.
	DumpFileExtension = ".json"
)

// -----------------------------------------------------------------------------
// Size Constants (Memory/Storage)
// -----------------------------------------------------------------------------

const (
	// KiBToMiB converts kibibytes to mebibytes.
	KiBToMiB = 1024

	// MaxFileSize is the maximum allowed dump file size (16 MiB).
	MaxFileSize = 16 * KiBToMiB * KiBToMiB
)

// -----------------------------------------------------------------------------
// UI Constants
// -----------------------------------------------------------------------------

const (
	// MaxCoreDisplay is the maximum number of CPU cores to display individually.
	MaxCoreDisplay = 16

	// ProcessListRankWidth is the width for process rank display.
	ProcessListRankWidth = 2
)

// -----------------------------------------------------------------------------
// Environment Variables
// -----------------------------------------------------------------------------

const (
	// EnvDataDir overrides the default data directory.
	EnvDataDir = "SPIKETRACE_DATA_DIR"

	// EnvXDGDataHome is the XDG data home environment variable.
	EnvXDGDataHome = "XDG_DATA_HOME"
)

// -----------------------------------------------------------------------------
// Path Resolution
// -----------------------------------------------------------------------------

// GetDataPaths returns an ordered list of directories to search for dump files.
// Priority order:
//  1. $SPIKETRACE_DATA_DIR (if set)
//  2. $XDG_DATA_HOME/spiketrace (or ~/.local/share/spiketrace)
//  3. /var/lib/spiketrace (system default)
func GetDataPaths() []string {
	var paths []string

	// Priority 1: Environment variable override
	if envDir := os.Getenv(EnvDataDir); envDir != "" {
		paths = append(paths, envDir)
	}

	// Priority 2: XDG Data Home
	xdgDataHome := os.Getenv(EnvXDGDataHome)
	if xdgDataHome == "" {
		// Fallback to default XDG location
		if home, err := os.UserHomeDir(); err == nil {
			xdgDataHome = filepath.Join(home, ".local", "share")
		}
	}
	if xdgDataHome != "" {
		paths = append(paths, filepath.Join(xdgDataHome, AppName))
	}

	// Priority 3: System default
	paths = append(paths, DefaultDumpDir)

	return paths
}
