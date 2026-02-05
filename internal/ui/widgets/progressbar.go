/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package widgets

import (
	"strings"

	"github.com/charmbracelet/lipgloss"
)

// ProgressBar renders a horizontal bar with optional baseline marker.
type ProgressBar struct {
	Value        float64 // Current value (0-100 for percentage)
	MaxValue     float64 // Maximum value (default 100)
	BaselineVal  float64 // Optional baseline for delta visualization
	Width        int
	FilledColor  lipgloss.Color
	EmptyColor   lipgloss.Color
	DeltaColor   lipgloss.Color
	ShowBaseline bool
}

// NewProgressBar creates a progress bar with default styling.
func NewProgressBar(value float64, width int) ProgressBar {
	return ProgressBar{
		Value:       value,
		MaxValue:    100,
		Width:       width,
		FilledColor: lipgloss.Color("62"),  // Blue
		EmptyColor:  lipgloss.Color("240"), // Dark gray
		DeltaColor:  lipgloss.Color("214"), // Orange
	}
}

// WithBaseline adds a baseline marker for delta visualization.
func (p ProgressBar) WithBaseline(baseline float64) ProgressBar {
	p.BaselineVal = baseline
	p.ShowBaseline = true
	return p
}

// WithMax sets the maximum value.
func (p ProgressBar) WithMax(max float64) ProgressBar {
	p.MaxValue = max
	return p
}

// Render produces the progress bar string.
func (p ProgressBar) Render() string {
	if p.Width <= 0 || p.MaxValue <= 0 {
		return ""
	}

	// Severity-based color selection
	filledColor := p.FilledColor
	ratio := p.Value / p.MaxValue
	if ratio > 0.9 {
		filledColor = lipgloss.Color("196") // Red - critical
	} else if ratio > 0.7 {
		filledColor = lipgloss.Color("214") // Orange - warning
	} else if ratio > 0.5 {
		filledColor = lipgloss.Color("226") // Yellow - moderate
	} else {
		filledColor = lipgloss.Color("42") // Green - healthy
	}

	filledStyle := lipgloss.NewStyle().Foreground(filledColor)
	emptyStyle := lipgloss.NewStyle().Foreground(p.EmptyColor)
	deltaStyle := lipgloss.NewStyle().Foreground(p.DeltaColor)

	if ratio > 1 {
		ratio = 1
	}
	if ratio < 0 {
		ratio = 0
	}
	filledWidth := int(ratio * float64(p.Width))

	var b strings.Builder

	if p.ShowBaseline && p.BaselineVal > 0 {
		// Delta mode: show baseline and delta separately
		baselineRatio := p.BaselineVal / p.MaxValue
		if baselineRatio > 1 {
			baselineRatio = 1
		}
		baselineWidth := int(baselineRatio * float64(p.Width))

		for i := 0; i < p.Width; i++ {
			if i < baselineWidth {
				b.WriteString(filledStyle.Render("█"))
			} else if i < filledWidth {
				b.WriteString(deltaStyle.Render("█"))
			} else {
				b.WriteString(emptyStyle.Render("░"))
			}
		}
	} else {
		// Simple mode
		for i := 0; i < p.Width; i++ {
			if i < filledWidth {
				b.WriteString(filledStyle.Render("█"))
			} else {
				b.WriteString(emptyStyle.Render("░"))
			}
		}
	}

	return b.String()
}
