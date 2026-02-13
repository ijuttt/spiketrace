/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

// Package styles provides Lipgloss styles for the TUI.
package styles

import "github.com/charmbracelet/lipgloss"

// -----------------------------------------------------------------------------
// Color Palette
// -----------------------------------------------------------------------------

var (
	// Primary colors - Cyberpunk/Btop inspired
	ColorPrimary   = lipgloss.Color("39")  // Deep Sky Blue (Neon)
	ColorSecondary = lipgloss.Color("238") // Dark Gray (Borders)
	ColorAccent    = lipgloss.Color("201") // Hot Pink/Magenta
	ColorSuccess   = lipgloss.Color("46")  // Neon Green
	ColorWarning   = lipgloss.Color("214") // Orange
	ColorYellow    = lipgloss.Color("226") // Yellow (for moderate severity)
	ColorDanger    = lipgloss.Color("196") // Bright Red
	ColorMuted     = lipgloss.Color("60")  // Cool Gray
	ColorDarkGray  = lipgloss.Color("240") // Dark Gray (for empty/background)

	// Text colors
	ColorText        = lipgloss.Color("255") // White (High Contrast)
	ColorTextDim     = lipgloss.Color("246") // Dim Gray
	ColorTextBold    = lipgloss.Color("231") // Bright White
	ColorBlack       = lipgloss.Color("16")  // Black (for inverted text)
	ColorStatusBarBg = lipgloss.Color("235") // Very Dark Gray (status bar background)
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
			Foreground(ColorPrimary).
			Padding(0, 1)
)

// -----------------------------------------------------------------------------
// List Item Styles
// -----------------------------------------------------------------------------

var (
	// SelectedItemStyle is for the currently selected list item.
	SelectedItemStyle = lipgloss.NewStyle().
				Foreground(ColorBlack).   // Black text
				Background(ColorPrimary). // Cyan background
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
			Foreground(ColorPrimary).
			Bold(true)

	// ValueStyle is for field values.
	ValueStyle = lipgloss.NewStyle().
			Foreground(ColorText)

	// HighlightValueStyle is for important values.
	HighlightValueStyle = lipgloss.NewStyle().
				Foreground(ColorAccent).
				Bold(true)

	// SectionTitleStyle is for section headers within panels.
	SectionTitleStyle = lipgloss.NewStyle().
				Foreground(ColorSuccess).
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
				Foreground(ColorPrimary).
				Bold(true)

	// MetricSecondaryStyle is for secondary/context values.
	MetricSecondaryStyle = lipgloss.NewStyle().
				Foreground(ColorTextDim)

	// ProcessStyle is for process entries.
	ProcessStyle = lipgloss.NewStyle().
			Foreground(ColorTextBold).
			Bold(true)

	// PIDStyle is for process IDs.
	PIDStyle = lipgloss.NewStyle().
			Foreground(ColorMuted)
)

// -----------------------------------------------------------------------------
// Status Bar Styles
// -----------------------------------------------------------------------------

var (
	// StatusBarStyle is the main status bar style.
	StatusBarStyle = lipgloss.NewStyle().
			Foreground(ColorText).
			Background(ColorStatusBarBg). // Very dark gray bg
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
			Foreground(ColorAccent).
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
