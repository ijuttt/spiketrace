// Package styles provides Lipgloss styles for the TUI.
package styles

import "github.com/charmbracelet/lipgloss"

// -----------------------------------------------------------------------------
// Color Palette
// -----------------------------------------------------------------------------

var (
	// Primary colors
	ColorPrimary   = lipgloss.Color("62")  // Soft blue
	ColorSecondary = lipgloss.Color("241") // Gray
	ColorAccent    = lipgloss.Color("205") // Pink/magenta
	ColorSuccess   = lipgloss.Color("42")  // Green
	ColorWarning   = lipgloss.Color("214") // Orange
	ColorDanger    = lipgloss.Color("196") // Red
	ColorMuted     = lipgloss.Color("240") // Dark gray

	// Text colors
	ColorText     = lipgloss.Color("252") // Light gray
	ColorTextDim  = lipgloss.Color("245") // Dimmed text
	ColorTextBold = lipgloss.Color("255") // White
)

// -----------------------------------------------------------------------------
// Panel Styles
// -----------------------------------------------------------------------------

var (
	// BasePanelStyle is the foundation style for all panels.
	BasePanelStyle = lipgloss.NewStyle().
			Border(lipgloss.RoundedBorder()).
			BorderForeground(ColorSecondary).
			Padding(0, 1)

	// ActivePanelStyle is used for the currently focused panel.
	ActivePanelStyle = BasePanelStyle.
				BorderForeground(ColorPrimary)

	// PanelTitleStyle styles panel titles.
	PanelTitleStyle = lipgloss.NewStyle().
			Bold(true).
			Foreground(ColorAccent).
			Padding(0, 1)
)

// -----------------------------------------------------------------------------
// List Item Styles
// -----------------------------------------------------------------------------

var (
	// SelectedItemStyle is for the currently selected list item.
	SelectedItemStyle = lipgloss.NewStyle().
				Foreground(ColorTextBold).
				Background(ColorPrimary).
				Bold(true).
				Padding(0, 1)

	// NormalItemStyle is for unselected list items.
	NormalItemStyle = lipgloss.NewStyle().
			Foreground(ColorText).
			Padding(0, 1)

	// DimItemStyle is for less important items.
	DimItemStyle = lipgloss.NewStyle().
			Foreground(ColorTextDim).
			Padding(0, 1)
)

// -----------------------------------------------------------------------------
// Data Display Styles
// -----------------------------------------------------------------------------

var (
	// LabelStyle is for field labels.
	LabelStyle = lipgloss.NewStyle().
			Foreground(ColorTextDim)

	// ValueStyle is for field values.
	ValueStyle = lipgloss.NewStyle().
			Foreground(ColorText)

	// HighlightValueStyle is for important values.
	HighlightValueStyle = lipgloss.NewStyle().
				Foreground(ColorWarning).
				Bold(true)

	// SectionTitleStyle is for section headers within panels.
	// Clean, minimal - just bold colored text with no decorators.
	SectionTitleStyle = lipgloss.NewStyle().
				Foreground(ColorPrimary).
				Bold(true).
				MarginTop(1).
				MarginBottom(0)

	// SectionBoxStyle wraps a section with subtle visual grouping.
	SectionBoxStyle = lipgloss.NewStyle().
			PaddingLeft(1).
			MarginTop(1)

	// MetricLabelStyle is for metric category labels (CPU, RAM, Swap).
	MetricLabelStyle = lipgloss.NewStyle().
				Foreground(ColorText).
				Bold(true)

	// MetricValueStyle is for primary metric values.
	MetricValueStyle = lipgloss.NewStyle().
				Foreground(ColorWarning).
				Bold(true)

	// MetricSecondaryStyle is for secondary/context values.
	MetricSecondaryStyle = lipgloss.NewStyle().
				Foreground(ColorTextDim)

	// ProcessStyle is for process entries.
	ProcessStyle = lipgloss.NewStyle().
			Foreground(ColorSuccess)

	// PIDStyle is for process IDs.
	PIDStyle = lipgloss.NewStyle().
			Foreground(ColorPrimary)
)

// -----------------------------------------------------------------------------
// Status Bar Styles
// -----------------------------------------------------------------------------

var (
	// StatusBarStyle is the main status bar style.
	StatusBarStyle = lipgloss.NewStyle().
			Foreground(ColorTextDim).
			Background(lipgloss.Color("236")).
			Padding(0, 1)

	// HelpKeyStyle is for keyboard shortcut keys.
	HelpKeyStyle = lipgloss.NewStyle().
			Foreground(ColorPrimary).
			Bold(true)

	// HelpDescStyle is for keyboard shortcut descriptions.
	HelpDescStyle = lipgloss.NewStyle().
			Foreground(ColorTextDim)
)

// -----------------------------------------------------------------------------
// Loading & Error Styles
// -----------------------------------------------------------------------------

var (
	// LoadingStyle is for loading indicators.
	LoadingStyle = lipgloss.NewStyle().
			Foreground(ColorPrimary).
			Italic(true)

	// ErrorStyle is for error messages.
	ErrorStyle = lipgloss.NewStyle().
			Foreground(ColorDanger).
			Bold(true)

	// SuccessStyle is for success messages.
	SuccessStyle = lipgloss.NewStyle().
			Foreground(ColorSuccess).
			Bold(true)
)
