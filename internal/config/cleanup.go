// Package config provides configuration constants and cleanup operations for spiketrace.
package config

import (
	"fmt"
	"os"
	"path/filepath"
)

// -----------------------------------------------------------------------------
// Cleanup Operations
// -----------------------------------------------------------------------------

// CleanupResult holds the result of a cleanup operation.
type CleanupResult struct {
	DeletedCount int
	FailedCount  int
	Errors       []error
}

// CleanupAllDumps deletes all dump files across all configured data paths.
// This is typically called during shutdown or manual cleanup.
func CleanupAllDumps() CleanupResult {
	result := CleanupResult{}
	dataPaths := GetDataPaths()

	for _, dir := range dataPaths {
		files, err := findDumpFiles(dir)
		if err != nil {
			continue
		}

		for _, file := range files {
			if err := os.Remove(file.path); err != nil {
				result.FailedCount++
				result.Errors = append(result.Errors,
					fmt.Errorf("%s: %w", filepath.Base(file.path), err))
			} else {
				result.DeletedCount++
			}
		}
	}

	return result
}

// CleanupDumpsInDirectory deletes all dump files in a specific directory.
func CleanupDumpsInDirectory(dir string) CleanupResult {
	result := CleanupResult{}

	files, err := findDumpFiles(dir)
	if err != nil {
		result.Errors = append(result.Errors, err)
		return result
	}

	for _, file := range files {
		if err := os.Remove(file.path); err != nil {
			result.FailedCount++
			result.Errors = append(result.Errors,
				fmt.Errorf("%s: %w", filepath.Base(file.path), err))
		} else {
			result.DeletedCount++
		}
	}

	return result
}
