// Package components provides reusable TUI components.
package components

import (
	"strings"

	"github.com/charmbracelet/bubbles/key"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/ijuttt/spiketrace/internal/ui/styles"
)

// -----------------------------------------------------------------------------
// Confirm Dialog Component
// -----------------------------------------------------------------------------

// ConfirmAction represents what action the dialog is confirming.
type ConfirmAction int

const (
	ConfirmNone ConfirmAction = iota
	ConfirmDeleteSingle
	ConfirmDeleteAll
)

// ConfirmResult is sent when user responds to confirmation dialog.
type ConfirmResult struct {
	Action    ConfirmAction
	Confirmed bool
	Data      string // Additional data (e.g., file path for single delete)
}

// ConfirmDialog is a modal confirmation dialog component.
type ConfirmDialog struct {
	visible bool
	action  ConfirmAction
	message string
	data    string
	width   int
	height  int
}

// NewConfirmDialog creates a new confirmation dialog.
func NewConfirmDialog() ConfirmDialog {
	return ConfirmDialog{
		visible: false,
	}
}

// Show displays the confirmation dialog with the given message.
func (c *ConfirmDialog) Show(action ConfirmAction, message string, data string) {
	c.visible = true
	c.action = action
	c.message = message
	c.data = data
}

// Hide hides the confirmation dialog.
func (c *ConfirmDialog) Hide() {
	c.visible = false
	c.action = ConfirmNone
	c.message = ""
	c.data = ""
}

// IsVisible returns whether the dialog is currently visible.
func (c *ConfirmDialog) IsVisible() bool {
	return c.visible
}

// Action returns the current action being confirmed.
func (c *ConfirmDialog) Action() ConfirmAction {
	return c.action
}

// Data returns the additional data associated with the action.
func (c *ConfirmDialog) Data() string {
	return c.data
}

// SetSize sets the dialog dimensions.
func (c *ConfirmDialog) SetSize(width, height int) {
	c.width = width
	c.height = height
}

// Key bindings for the dialog
var (
	confirmKey = key.NewBinding(
		key.WithKeys("y", "Y"),
		key.WithHelp("y", "confirm"),
	)
	cancelKey = key.NewBinding(
		key.WithKeys("n", "N", "esc"),
		key.WithHelp("n/esc", "cancel"),
	)
)

// Update handles input for the confirmation dialog.
func (c *ConfirmDialog) Update(msg tea.Msg) (ConfirmResult, bool) {
	if !c.visible {
		return ConfirmResult{}, false
	}

	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch {
		case key.Matches(msg, confirmKey):
			result := ConfirmResult{
				Action:    c.action,
				Confirmed: true,
				Data:      c.data,
			}
			c.Hide()
			return result, true

		case key.Matches(msg, cancelKey):
			result := ConfirmResult{
				Action:    c.action,
				Confirmed: false,
				Data:      c.data,
			}
			c.Hide()
			return result, true
		}
	}

	return ConfirmResult{}, false
}

// View renders the confirmation dialog.
func (c ConfirmDialog) View() string {
	if !c.visible {
		return ""
	}

	// Dialog styling
	dialogStyle := lipgloss.NewStyle().
		Border(lipgloss.RoundedBorder()).
		BorderForeground(styles.ColorWarning).
		Padding(1, 2).
		Width(40)

	titleStyle := lipgloss.NewStyle().
		Foreground(styles.ColorWarning).
		Bold(true)

	messageStyle := lipgloss.NewStyle().
		Foreground(styles.ColorText).
		MarginTop(1).
		MarginBottom(1)

	hintStyle := lipgloss.NewStyle().
		Foreground(styles.ColorMuted)

	// Build dialog content
	var b strings.Builder
	b.WriteString(titleStyle.Render("⚠️  Confirmation"))
	b.WriteString("\n")
	b.WriteString(messageStyle.Render(c.message))
	b.WriteString("\n\n")
	b.WriteString(hintStyle.Render("[y] Yes    [n/esc] Cancel"))

	dialog := dialogStyle.Render(b.String())

	// Center the dialog
	if c.width > 0 && c.height > 0 {
		return lipgloss.Place(
			c.width, c.height,
			lipgloss.Center, lipgloss.Center,
			dialog,
		)
	}

	return dialog
}
