// Package processor provides async file loading and parsing for the TUI.
package processor

import (
	"fmt"
	"os"
	"path/filepath"

	tea "github.com/charmbracelet/bubbletea"
)

// -----------------------------------------------------------------------------
// Delete Messages
// -----------------------------------------------------------------------------

// DeleteResultMsg is sent when a file deletion operation completes.
type DeleteResultMsg struct {
	Path    string
	Success bool
	Err     error
}

// DeleteAllResultMsg is sent when bulk deletion operation completes.
type DeleteAllResultMsg struct {
	DeletedCount int
	FailedCount  int
	Errors       []error
}

// -----------------------------------------------------------------------------
// Delete Commands
// -----------------------------------------------------------------------------

// DeleteDumpCmd creates a command to delete a single dump file.
func DeleteDumpCmd(path string) tea.Cmd {
	return func() tea.Msg {
		// Validate path exists and is a file
		info, err := os.Stat(path)
		if err != nil {
			return DeleteResultMsg{
				Path:    path,
				Success: false,
				Err:     fmt.Errorf("file not found: %w", err),
			}
		}

		if info.IsDir() {
			return DeleteResultMsg{
				Path:    path,
				Success: false,
				Err:     fmt.Errorf("cannot delete directory: %s", path),
			}
		}

		// Perform deletion
		if err := os.Remove(path); err != nil {
			return DeleteResultMsg{
				Path:    path,
				Success: false,
				Err:     fmt.Errorf("failed to delete: %w", err),
			}
		}

		return DeleteResultMsg{
			Path:    path,
			Success: true,
			Err:     nil,
		}
	}
}

// DeleteAllDumpsCmd creates a command to delete all dump files in the given paths.
func DeleteAllDumpsCmd(paths []string, extension string) tea.Cmd {
	return func() tea.Msg {
		var deleted, failed int
		var errors []error

		for _, dir := range paths {
			files, err := listFilesWithInfo(dir, extension)
			if err != nil {
				continue
			}

			for _, file := range files {
				if err := os.Remove(file.Path); err != nil {
					failed++
					errors = append(errors, fmt.Errorf("%s: %w", filepath.Base(file.Path), err))
				} else {
					deleted++
				}
			}
		}

		return DeleteAllResultMsg{
			DeletedCount: deleted,
			FailedCount:  failed,
			Errors:       errors,
		}
	}
}

// -----------------------------------------------------------------------------
// Standalone Delete Functions (for scripts/cleanup)
// -----------------------------------------------------------------------------

// DeleteFile deletes a single file at the given path.
// Returns nil on success, error otherwise.
func DeleteFile(path string) error {
	info, err := os.Stat(path)
	if err != nil {
		return fmt.Errorf("file not found: %w", err)
	}

	if info.IsDir() {
		return fmt.Errorf("cannot delete directory: %s", path)
	}

	return os.Remove(path)
}

// DeleteAllInDirectory deletes all files with the given extension in a directory.
// Returns count of deleted files and any errors encountered.
func DeleteAllInDirectory(dir, extension string) (int, []error) {
	var deleted int
	var errors []error

	files, err := listFilesWithInfo(dir, extension)
	if err != nil {
		return 0, []error{err}
	}

	for _, file := range files {
		if err := os.Remove(file.Path); err != nil {
			errors = append(errors, fmt.Errorf("%s: %w", filepath.Base(file.Path), err))
		} else {
			deleted++
		}
	}

	return deleted, errors
}
