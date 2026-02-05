// Package processor provides async file loading and parsing for the TUI.
package processor

import (
	tea "github.com/charmbracelet/bubbletea"
	"github.com/jegesmk/spiketrace/internal/model"
)

// -----------------------------------------------------------------------------
// Messages
// -----------------------------------------------------------------------------

// LoadResultMsg is sent when a dump file has been loaded.
type LoadResultMsg struct {
	Path string
	Dump *model.SpikeDump
	Err  error
}

// FileListMsg is sent when the file list has been refreshed.
type FileListMsg struct {
	Files []FileInfo
	Err   error
}

// -----------------------------------------------------------------------------
// Commands
// -----------------------------------------------------------------------------

// LoadDumpCmd creates a command to load a dump file asynchronously.
func LoadDumpCmd(path string) tea.Cmd {
	return func() tea.Msg {
		dump, err := model.LoadDump(path)
		return LoadResultMsg{
			Path: path,
			Dump: dump,
			Err:  err,
		}
	}
}

// RefreshFilesCmd creates a command to refresh the file list.
func RefreshFilesCmd(paths []string, extension string) tea.Cmd {
	return func() tea.Msg {
		var allFiles []FileInfo
		for _, dir := range paths {
			files, err := listFilesWithInfo(dir, extension)
			if err != nil {
				continue
			}
			allFiles = append(allFiles, files...)
		}
		return FileListMsg{Files: allFiles}
	}
}
