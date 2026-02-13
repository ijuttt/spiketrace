/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package styles

import (
	"strings"

	"github.com/charmbracelet/lipgloss"
)

// BuildTitledBorder returns a lipgloss.Border whose Top field contains the
// panel title embedded into the border line, e.g.:
//
//	┌─ Process View ──────────┐
func BuildTitledBorder(title string, totalWidth int, b lipgloss.Border) lipgloss.Border {
	// innerWidth = total panel width minus the two corner runes (TopLeft + TopRight)
	innerWidth := totalWidth - lipgloss.Width(b.TopLeft) - lipgloss.Width(b.TopRight)
	if innerWidth <= 0 {
		return b
	}

	label := "─ " + title + " " // e.g. "─ Process View "
	labelWidth := lipgloss.Width(label)

	remaining := innerWidth - labelWidth
	if remaining < 0 {
		remaining = 0
	}

	// Fill the rest of the top border with the border's normal horizontal char
	topChar := b.Top
	if topChar == "" {
		topChar = "─"
	}
	b.Top = label + strings.Repeat(topChar, remaining)
	return b
}
