/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package model

import (
	"encoding/json"
	"fmt"
	"os"
)

// MaxFileSize is the maximum allowed dump file size (16 MiB).
const MaxFileSize = 16 * 1024 * 1024

// LoadDump reads and parses a spike dump JSON file.
func LoadDump(path string) (*SpikeDump, error) {
	info, err := os.Stat(path)
	if err != nil {
		return nil, fmt.Errorf("cannot stat %s: %w", path, err)
	}

	if info.Size() > MaxFileSize {
		return nil, fmt.Errorf("file %s exceeds maximum size (%d bytes)", path, MaxFileSize)
	}

	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("cannot read %s: %w", path, err)
	}

	var dump SpikeDump
	if err := json.Unmarshal(data, &dump); err != nil {
		return nil, fmt.Errorf("cannot parse JSON: %w", err)
	}

	return &dump, nil
}
