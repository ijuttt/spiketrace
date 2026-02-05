/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

// Package components provides reusable TUI components.
package components

import (
	"fmt"
	"path/filepath"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/key"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/jegesmk/spiketrace/internal/processor"
	"github.com/jegesmk/spiketrace/internal/ui/styles"
)

// -----------------------------------------------------------------------------
// Key Bindings (local to avoid import cycle)
// -----------------------------------------------------------------------------

var (
	keyUp = key.NewBinding(
		key.WithKeys("up", "k"),
	)
	keyDown = key.NewBinding(
		key.WithKeys("down", "j"),
	)
)

// -----------------------------------------------------------------------------
// Explorer Component
// -----------------------------------------------------------------------------

// Explorer is a file browser component.
type Explorer struct {
	files     []processor.FileInfo
	cursor    int
	width     int
	height    int
	focused   bool
	title     string
	emptyText string
}

// NewExplorer creates a new file explorer.
func NewExplorer() Explorer {
	return Explorer{
		files:     []processor.FileInfo{},
		cursor:    0,
		title:     "📂 File Explorer",
		emptyText: "No files found",
	}
}

// SetFiles updates the file list.
func (e *Explorer) SetFiles(files []processor.FileInfo) {
	e.files = files
	if e.cursor >= len(files) {
		e.cursor = max(0, len(files)-1)
	}
}

// SetSize updates the component dimensions.
func (e *Explorer) SetSize(width, height int) {
	e.width = width
	e.height = height
}

// SetFocused sets the focus state.
func (e *Explorer) SetFocused(focused bool) {
	e.focused = focused
}

// SelectedFile returns the currently selected file info.
func (e *Explorer) SelectedFile() *processor.FileInfo {
	if e.cursor >= 0 && e.cursor < len(e.files) {
		return &e.files[e.cursor]
	}
	return nil
}

// Selected returns the currently selected file path (for compatibility).
func (e *Explorer) Selected() string {
	if f := e.SelectedFile(); f != nil {
		return f.Path
	}
	return ""
}

// Cursor returns the current cursor position.
func (e *Explorer) Cursor() int {
	return e.cursor
}

// FileCount returns total number of files.
func (e *Explorer) FileCount() int {
	return len(e.files)
}

// Update handles input for the explorer.
func (e *Explorer) Update(msg tea.Msg) tea.Cmd {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch {
		case key.Matches(msg, keyUp):
			if e.cursor > 0 {
				e.cursor--
			}
		case key.Matches(msg, keyDown):
			if e.cursor < len(e.files)-1 {
				e.cursor++
			}
		}
	}
	return nil
}

// View renders the explorer.
func (e Explorer) View() string {
	var b strings.Builder

	// Title with file count
	titleText := fmt.Sprintf("%s (%d)", e.title, len(e.files))
	title := styles.PanelTitleStyle.Render(titleText)
	b.WriteString(title)
	b.WriteString("\n\n")

	if len(e.files) == 0 {
		b.WriteString(styles.DimItemStyle.Render(e.emptyText))
		return e.applyPanelStyle(b.String())
	}

	// Calculate visible range
	visibleHeight := e.height - 5 // Account for title and padding
	if visibleHeight < 1 {
		visibleHeight = 5
	}

	start := 0
	if e.cursor >= visibleHeight {
		start = e.cursor - visibleHeight + 1
	}
	end := min(start+visibleHeight, len(e.files))

	// Render file list
	for i := start; i < end; i++ {
		file := e.files[i]
		filename := filepath.Base(file.Path)

		// Truncate filename if too long
		maxLen := e.width - 6
		if maxLen < 10 {
			maxLen = 10
		}
		if len(filename) > maxLen {
			filename = filename[:maxLen-3] + "..."
		}

		var item string
		if i == e.cursor {
			prefix := "▸ "
			item = styles.SelectedItemStyle.Render(prefix + filename)
		} else {
			prefix := "  "
			item = styles.NormalItemStyle.Render(prefix + filename)
		}

		b.WriteString(item)
		if i < end-1 {
			b.WriteString("\n")
		}
	}

	// Scroll indicator
	b.WriteString("\n")
	indicator := fmt.Sprintf(" [%d/%d]", e.cursor+1, len(e.files))
	b.WriteString(styles.DimItemStyle.Render(indicator))

	return e.applyPanelStyle(b.String())
}

// applyPanelStyle applies the appropriate panel style.
func (e Explorer) applyPanelStyle(content string) string {
	style := styles.BasePanelStyle
	if e.focused {
		style = styles.ActivePanelStyle
	}

	return style.
		Width(e.width).
		Height(e.height).
		Render(content)
}

// -----------------------------------------------------------------------------
// Time Formatting Helpers
// -----------------------------------------------------------------------------

// FormatDateTime formats a time for detailed display.
func FormatDateTime(t time.Time) string {
	now := time.Now()
	diff := now.Sub(t)

	if diff < 24*time.Hour && t.Day() == now.Day() {
		return "Today at " + t.Format("15:04:05")
	} else if diff < 48*time.Hour && t.Day() == now.Add(-24*time.Hour).Day() {
		return "Yesterday at " + t.Format("15:04:05")
	}
	return t.Format("02 Jan 2006, 15:04:05")
}

// FormatFileSize formats bytes as human-readable size.
func FormatFileSize(bytes int64) string {
	const (
		KB = 1024
		MB = KB * 1024
		GB = MB * 1024
	)

	switch {
	case bytes >= GB:
		return fmt.Sprintf("%.1f GB", float64(bytes)/float64(GB))
	case bytes >= MB:
		return fmt.Sprintf("%.1f MB", float64(bytes)/float64(MB))
	case bytes >= KB:
		return fmt.Sprintf("%.1f KB", float64(bytes)/float64(KB))
	default:
		return fmt.Sprintf("%d B", bytes)
	}
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}
