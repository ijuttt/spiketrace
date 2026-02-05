package processor

import (
	"os"
	"path/filepath"
	"sort"
	"time"
)

// FileInfo holds file metadata for display.
type FileInfo struct {
	Path    string
	Name    string
	Size    int64
	ModTime time.Time
}

// listFilesWithInfo returns files with metadata, sorted by modification time (newest first).
func listFilesWithInfo(dir, extension string) ([]FileInfo, error) {
	info, err := os.Stat(dir)
	if err != nil {
		return nil, err
	}
	if !info.IsDir() {
		return nil, nil
	}

	entries, err := os.ReadDir(dir)
	if err != nil {
		return nil, err
	}

	var files []FileInfo
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		if filepath.Ext(entry.Name()) != extension {
			continue
		}

		fileInfo, err := entry.Info()
		if err != nil {
			continue
		}

		files = append(files, FileInfo{
			Path:    filepath.Join(dir, entry.Name()),
			Name:    entry.Name(),
			Size:    fileInfo.Size(),
			ModTime: fileInfo.ModTime(),
		})
	}

	// Sort by modification time (newest first)
	sort.Slice(files, func(i, j int) bool {
		return files[i].ModTime.After(files[j].ModTime)
	})

	return files, nil
}

// listFiles returns all files with the given extension in the directory (legacy).
func listFiles(dir, extension string) ([]string, error) {
	files, err := listFilesWithInfo(dir, extension)
	if err != nil {
		return nil, err
	}

	var paths []string
	for _, f := range files {
		paths = append(paths, f.Path)
	}
	return paths, nil
}
