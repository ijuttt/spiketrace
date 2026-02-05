// Package widgets provides reusable TUI visualization components.
package widgets

import (
	"strings"

	"github.com/charmbracelet/lipgloss"
)

// Sparkline renders a time-series as a Unicode bar chart.
type Sparkline struct {
	Data           []float64
	Width          int
	HighlightIndex int // Index to highlight (e.g., trigger moment)
	NormalColor    lipgloss.Color
	HighlightColor lipgloss.Color
}

// sparkBlocks are Unicode block elements for 8 levels of height.
var sparkBlocks = []rune{'▁', '▂', '▃', '▄', '▅', '▆', '▇', '█'}

// NewSparkline creates a sparkline with default styling.
func NewSparkline(data []float64, width int) Sparkline {
	return Sparkline{
		Data:           data,
		Width:          width,
		HighlightIndex: -1,
		NormalColor:    lipgloss.Color("62"),  // Blue
		HighlightColor: lipgloss.Color("196"), // Red
	}
}

// WithHighlight sets the index to highlight.
func (s Sparkline) WithHighlight(idx int) Sparkline {
	s.HighlightIndex = idx
	return s
}

// Render produces the sparkline string.
func (s Sparkline) Render() string {
	if len(s.Data) == 0 {
		return ""
	}

	// Find min/max for normalization
	minVal, maxVal := s.Data[0], s.Data[0]
	for _, v := range s.Data {
		if v < minVal {
			minVal = v
		}
		if v > maxVal {
			maxVal = v
		}
	}

	// Handle flat line
	valRange := maxVal - minVal
	if valRange == 0 {
		valRange = 1
	}

	// Sample data to fit width
	samples := s.sampleData()

	var b strings.Builder
	normalStyle := lipgloss.NewStyle().Foreground(s.NormalColor)
	highlightStyle := lipgloss.NewStyle().Foreground(s.HighlightColor).Bold(true)

	for i, val := range samples {
		// Normalize to 0-7 range
		normalized := (val - minVal) / valRange
		blockIdx := int(normalized * 7)
		if blockIdx > 7 {
			blockIdx = 7
		}
		if blockIdx < 0 {
			blockIdx = 0
		}

		char := string(sparkBlocks[blockIdx])

		// Check if this point corresponds to highlight
		isHighlight := s.HighlightIndex >= 0 && s.mapSampleToData(i, len(samples)) == s.HighlightIndex

		if isHighlight {
			b.WriteString(highlightStyle.Render(char))
		} else {
			b.WriteString(normalStyle.Render(char))
		}
	}

	return b.String()
}

// sampleData reduces data points to fit within width.
func (s Sparkline) sampleData() []float64 {
	if len(s.Data) <= s.Width {
		return s.Data
	}

	result := make([]float64, s.Width)
	ratio := float64(len(s.Data)) / float64(s.Width)

	for i := 0; i < s.Width; i++ {
		idx := int(float64(i) * ratio)
		if idx >= len(s.Data) {
			idx = len(s.Data) - 1
		}
		result[i] = s.Data[idx]
	}

	return result
}

// mapSampleToData maps a sample index back to original data index.
func (s Sparkline) mapSampleToData(sampleIdx, sampleCount int) int {
	if sampleCount >= len(s.Data) {
		return sampleIdx
	}
	ratio := float64(len(s.Data)) / float64(sampleCount)
	return int(float64(sampleIdx) * ratio)
}
