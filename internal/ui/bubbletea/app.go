/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

// Package bubbletea provides the main TUI application using Bubble Tea.
package bubbletea

import (
	"fmt"
	"path/filepath"
	"strings"

	"github.com/charmbracelet/bubbles/key"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/ijuttt/spiketrace/internal/config"
	"github.com/ijuttt/spiketrace/internal/processor"
	"github.com/ijuttt/spiketrace/internal/ui/components"
	"github.com/ijuttt/spiketrace/internal/ui/styles"
)

// Panel identifiers
const (
	PanelExplorer = iota
	PanelTrigger
	PanelViewer
	PanelCount
)

// Panel width ratios
const (
	explorerRatio = 0.20
	triggerRatio  = 0.35
	viewerRatio   = 0.45
)

// App is the main application model.
type App struct {
	// Components
	explorer      components.Explorer
	trigger       components.Trigger
	viewer        components.Viewer
	confirmDialog components.ConfirmDialog

	// State
	activePanel int
	currentFile string
	loading     bool
	statusMsg   string
	errMsg      string

	// Layout
	width  int
	height int

	// Key bindings
	keys KeyMap
}

// NewApp creates a new application instance.
func NewApp() App {
	return App{
		explorer:      components.NewExplorer(),
		trigger:       components.NewTrigger(),
		viewer:        components.NewViewer(),
		confirmDialog: components.NewConfirmDialog(),
		activePanel:   PanelExplorer,
		keys:          DefaultKeyMap(),
		statusMsg:     "Loading files...",
	}
}

// Init initializes the application.
func (a App) Init() tea.Cmd {
	return processor.RefreshFilesCmd(config.GetDataPaths(), ".json")
}

// Update handles messages and updates the model.
func (a App) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	var cmds []tea.Cmd

	// Handle confirmation dialog first if visible
	if a.confirmDialog.IsVisible() {
		if result, handled := a.confirmDialog.Update(msg); handled {
			if result.Confirmed {
				switch result.Action {
				case components.ConfirmDeleteSingle:
					a.statusMsg = "Menghapus file..."
					return a, processor.DeleteDumpCmd(result.Data)
				case components.ConfirmDeleteAll:
					a.statusMsg = "Menghapus semua file..."
					return a, processor.DeleteAllDumpsCmd(
						config.GetDataPaths(),
						config.DumpFileExtension,
					)
				}
			} else {
				a.statusMsg = "Penghapusan dibatalkan"
			}
			return a, nil
		}
		return a, nil // Block other input while dialog is visible
	}

	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		a.width = msg.Width
		a.height = msg.Height
		a.confirmDialog.SetSize(msg.Width, msg.Height)
		a.updateComponentSizes()

	case tea.KeyMsg:
		// Global keys
		switch {
		case key.Matches(msg, a.keys.Quit):
			return a, tea.Quit

		case key.Matches(msg, a.keys.Tab):
			a.activePanel = (a.activePanel + 1) % PanelCount
			a.updateFocus()

		case key.Matches(msg, a.keys.Reload):
			a.loading = true
			a.statusMsg = "Reloading files..."
			cmds = append(cmds, processor.RefreshFilesCmd(config.GetDataPaths(), ".json"))

		case key.Matches(msg, a.keys.Enter):
			if a.activePanel == PanelExplorer || a.activePanel == PanelTrigger {
				if path := a.explorer.Selected(); path != "" {
					a.loading = true
					a.statusMsg = "Loading file..."
					cmds = append(cmds, processor.LoadDumpCmd(path))
				}
			}

		case key.Matches(msg, a.keys.Delete):
			// Delete selected file (with confirmation)
			if a.activePanel == PanelExplorer {
				selected := a.explorer.Selected()
				if selected != "" {
					filename := filepath.Base(selected)
					a.confirmDialog.Show(
						components.ConfirmDeleteSingle,
						fmt.Sprintf("Hapus file '%s'?", filename),
						selected,
					)
				}
			}

		case key.Matches(msg, a.keys.DeleteAll):
			// Delete all files (with confirmation)
			if a.activePanel == PanelExplorer && a.explorer.FileCount() > 0 {
				a.confirmDialog.Show(
					components.ConfirmDeleteAll,
					fmt.Sprintf("Hapus SEMUA %d file log?", a.explorer.FileCount()),
					"",
				)
			}

		default:
			// Forward to active panel
			switch a.activePanel {
			case PanelExplorer:
				cmd := a.explorer.Update(msg)
				if cmd != nil {
					cmds = append(cmds, cmd)
				}
				// Update trigger's selected file info when cursor moves
				a.trigger.SetSelectedFile(a.explorer.SelectedFile())
			case PanelTrigger:
				cmd := a.trigger.Update(msg)
				if cmd != nil {
					cmds = append(cmds, cmd)
				}
				// Update viewer when snapshot changes
				a.viewer.SetState(a.trigger.Dump(), a.trigger.SnapshotIdx())
			}
		}

	case processor.FileListMsg:
		a.loading = false
		if msg.Err != nil {
			a.errMsg = msg.Err.Error()
		} else {
			a.explorer.SetFiles(msg.Files)
			a.statusMsg = fmt.Sprintf("Found %d files (sorted by date)", len(msg.Files))
			a.errMsg = ""
			// Update trigger with first file's info
			a.trigger.SetSelectedFile(a.explorer.SelectedFile())
		}

	case processor.LoadResultMsg:
		a.loading = false
		if msg.Err != nil {
			a.errMsg = msg.Err.Error()
			a.statusMsg = "Failed to load file"
		} else {
			a.currentFile = msg.Path
			a.trigger.SetDump(msg.Dump)
			a.viewer.SetState(msg.Dump, 0)
			a.statusMsg = fmt.Sprintf("✓ Loaded: %d snapshots", len(msg.Dump.Snapshots))
			a.errMsg = ""
		}

	case processor.DeleteResultMsg:
		if msg.Success {
			a.statusMsg = fmt.Sprintf("✓ File dihapus: %s", filepath.Base(msg.Path))
			a.errMsg = ""
			// Clear current file if it was deleted
			if a.currentFile == msg.Path {
				a.currentFile = ""
				a.trigger.SetDump(nil)
				a.viewer.SetState(nil, 0)
			}
			// Refresh file list
			return a, processor.RefreshFilesCmd(
				config.GetDataPaths(),
				config.DumpFileExtension,
			)
		} else {
			a.errMsg = msg.Err.Error()
			a.statusMsg = "Gagal menghapus file"
		}

	case processor.DeleteAllResultMsg:
		a.currentFile = ""
		a.trigger.SetDump(nil)
		a.viewer.SetSnapshot(nil)
		if msg.FailedCount > 0 {
			a.statusMsg = fmt.Sprintf("Dihapus %d file, %d gagal", msg.DeletedCount, msg.FailedCount)
		} else {
			a.statusMsg = fmt.Sprintf("✓ Semua %d file dihapus", msg.DeletedCount)
		}
		a.errMsg = ""
		// Refresh file list
		return a, processor.RefreshFilesCmd(
			config.GetDataPaths(),
			config.DumpFileExtension,
		)
	}

	return a, tea.Batch(cmds...)
}

// View renders the application.
func (a App) View() string {
	if a.width == 0 {
		return "Initializing..."
	}

	var b strings.Builder

	// Header
	header := a.renderHeader()
	b.WriteString(header)
	b.WriteString("\n")

	// Panels
	panels := lipgloss.JoinHorizontal(
		lipgloss.Top,
		a.explorer.View(),
		a.trigger.View(),
		a.viewer.View(),
	)
	b.WriteString(panels)
	b.WriteString("\n")

	// Status bar
	statusBar := a.renderStatusBar()
	b.WriteString(statusBar)

	// Overlay confirmation dialog if visible
	if a.confirmDialog.IsVisible() {
		return a.confirmDialog.View()
	}

	return b.String()
}

// updateComponentSizes recalculates component dimensions.
func (a *App) updateComponentSizes() {
	// Reserve space for header and status bar
	contentHeight := a.height - 4

	explorerWidth := int(float64(a.width) * explorerRatio)
	triggerWidth := int(float64(a.width) * triggerRatio)
	viewerWidth := a.width - explorerWidth - triggerWidth - 6 // Account for borders

	a.explorer.SetSize(explorerWidth, contentHeight)
	a.trigger.SetSize(triggerWidth, contentHeight)
	a.viewer.SetSize(viewerWidth, contentHeight)

	a.updateFocus()
}

// updateFocus sets focus states on components.
func (a *App) updateFocus() {
	a.explorer.SetFocused(a.activePanel == PanelExplorer)
	a.trigger.SetFocused(a.activePanel == PanelTrigger)
	a.viewer.SetFocused(a.activePanel == PanelViewer)
}

// renderHeader renders the application header.
func (a App) renderHeader() string {
	title := styles.PanelTitleStyle.Render("⚡ Spiketrace Viewer")
	return title
}

// renderStatusBar renders the status bar.
func (a App) renderStatusBar() string {
	var left, right string

	if a.errMsg != "" {
		left = styles.ErrorStyle.Render(a.errMsg)
	} else if a.loading {
		left = styles.LoadingStyle.Render(a.statusMsg)
	} else {
		left = styles.DimItemStyle.Render(a.statusMsg)
	}

	// Keybinding hints
	hints := []string{
		styles.HelpKeyStyle.Render("Tab") + styles.HelpDescStyle.Render(":panel"),
		styles.HelpKeyStyle.Render("↑↓") + styles.HelpDescStyle.Render(":nav"),
		styles.HelpKeyStyle.Render("Enter") + styles.HelpDescStyle.Render(":load"),
		styles.HelpKeyStyle.Render("h/l") + styles.HelpDescStyle.Render(":snap"),
		styles.HelpKeyStyle.Render("m") + styles.HelpDescStyle.Render(":mem"),
		styles.HelpKeyStyle.Render("d") + styles.HelpDescStyle.Render(":del"),
		styles.HelpKeyStyle.Render("D") + styles.HelpDescStyle.Render(":delAll"),
		styles.HelpKeyStyle.Render("r") + styles.HelpDescStyle.Render(":refresh"),
		styles.HelpKeyStyle.Render("q") + styles.HelpDescStyle.Render(":quit"),
	}
	right = strings.Join(hints, "  ")

	// Pad to fill width
	padding := a.width - lipgloss.Width(left) - lipgloss.Width(right) - 2
	if padding < 0 {
		padding = 0
	}

	return styles.StatusBarStyle.
		Width(a.width).
		Render(left + strings.Repeat(" ", padding) + right)
}
