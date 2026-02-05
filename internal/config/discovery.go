package config

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"time"
)

// -----------------------------------------------------------------------------
// Dump File Discovery
// -----------------------------------------------------------------------------

// DiscoverLatestDump searches configured paths for the most recent dump file.
// Returns the path to the newest .json file found, or an error if none exist.
func DiscoverLatestDump() (string, error) {
	dataPaths := GetDataPaths()

	var candidates []fileCandidate

	for _, dir := range dataPaths {
		files, err := findDumpFiles(dir)
		if err != nil {
			// Directory might not exist; continue searching
			continue
		}
		candidates = append(candidates, files...)
	}

	if len(candidates) == 0 {
		return "", fmt.Errorf("no dump files found in paths: %v", dataPaths)
	}

	// Sort by modification time (newest first)
	sort.Slice(candidates, func(i, j int) bool {
		return candidates[i].modTime.After(candidates[j].modTime)
	})

	return candidates[0].path, nil
}

// fileCandidate represents a discovered dump file with its metadata.
type fileCandidate struct {
	path    string
	modTime time.Time
}

// findDumpFiles returns all .json files in the given directory.
func findDumpFiles(dir string) ([]fileCandidate, error) {
	info, err := os.Stat(dir)
	if err != nil {
		return nil, err
	}
	if !info.IsDir() {
		return nil, fmt.Errorf("%s is not a directory", dir)
	}

	entries, err := os.ReadDir(dir)
	if err != nil {
		return nil, err
	}

	var candidates []fileCandidate
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}

		if filepath.Ext(entry.Name()) != DumpFileExtension {
			continue
		}

		fullPath := filepath.Join(dir, entry.Name())
		fileInfo, err := entry.Info()
		if err != nil {
			continue
		}

		candidates = append(candidates, fileCandidate{
			path:    fullPath,
			modTime: fileInfo.ModTime(),
		})
	}

	return candidates, nil
}

// ListAvailableDumps returns a list of all discovered dump files across all paths.
// Useful for future "file picker" UI enhancement.
func ListAvailableDumps() ([]string, error) {
	dataPaths := GetDataPaths()

	var allDumps []string
	for _, dir := range dataPaths {
		files, err := findDumpFiles(dir)
		if err != nil {
			continue
		}
		for _, f := range files {
			allDumps = append(allDumps, f.path)
		}
	}

	if len(allDumps) == 0 {
		return nil, fmt.Errorf("no dump files found")
	}

	return allDumps, nil
}
