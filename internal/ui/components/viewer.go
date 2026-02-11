/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package components

import (
	"fmt"
	"strings"

	"github.com/charmbracelet/lipgloss"
	"github.com/ijuttt/spiketrace/internal/model"
	"github.com/ijuttt/spiketrace/internal/ui/styles"
)

// -----------------------------------------------------------------------------
// Viewer Component
// -----------------------------------------------------------------------------

const (
	widthRank    = 3
	widthPID     = 7
	widthPPID    = 7
	widthUID     = 6
	widthState   = 2
	widthPersist = 10
	widthComm    = 8
	widthRSS     = 8
	widthCPU     = 6
)

// Viewer displays process lists and detailed data.
type Viewer struct {
	dump          *model.SpikeDump
	snapshot      *model.Snapshot
	snapshotIdx   int
	snapshotCount int
	width         int
	height        int
	focused       bool
	title         string
	scrollY       int
}

// NewViewer creates a new data viewer.
func NewViewer() Viewer {
	return Viewer{
		title: "📊 Process View",
	}
}

// SetState updates the viewer state with full dump context.
func (v *Viewer) SetState(dump *model.SpikeDump, idx int) {
	v.dump = dump
	v.snapshotIdx = idx
	if dump != nil {
		v.snapshotCount = len(dump.Snapshots)
		if idx >= 0 && idx < len(dump.Snapshots) {
			v.snapshot = &dump.Snapshots[idx]
		}
	} else {
		v.snapshot = nil
		v.snapshotCount = 0
	}
	v.scrollY = 0
}

// SetSize updates the component dimensions.
func (v *Viewer) SetSize(width, height int) {
	v.width = width
	v.height = height
}

// SetFocused sets the focus state.
func (v *Viewer) SetFocused(focused bool) {
	v.focused = focused
}

// View renders the viewer.
func (v Viewer) View() string {
	var b strings.Builder

	// Title
	title := styles.PanelTitleStyle.Render(v.title)
	b.WriteString(title)
	b.WriteString("\n\n")

	if v.snapshot == nil {
		b.WriteString(styles.DimItemStyle.Render("Select a file to view data"))
		return v.applyPanelStyle(b.String())
	}

	// Calculate available rows for process lists
	// Overhead:
	// - Top/Bottom Border: 2
	// - Panel Title + blank: 2
	// - CPU List Header (Title + Headers + blank): 3
	// - Spacer: 1
	// - Mem List Header (Title + Headers + blank): 3
	// Total Overhead inside panel: 9 lines (plus borders handled by style)
	// We subtract an extra 1 safety margin.
	// Total overhead: ~10 lines

	innerHeight := v.height - 2 // Remove borders
	availableRows := innerHeight - 11
	rowsPerList := availableRows / 2
	if rowsPerList < 1 {
		rowsPerList = 1
	}

	// Top by CPU
	b.WriteString(v.renderProcessList("Top by CPU", v.snapshot.Procs, false, rowsPerList))
	b.WriteString("\n\n")

	// Top by RSS
	b.WriteString(v.renderProcessList("Top by Memory", v.snapshot.TopRSSProcs, true, rowsPerList))

	return v.applyPanelStyle(b.String())
}

// Column priority (lower number = higher priority, kept longer)
type columnPriority struct {
	name     string
	width    int
	priority int
	visible  bool
}

// renderProcessList renders a list of processes with dynamic column visibility.
func (v Viewer) renderProcessList(title string, procs []model.ProcessEntry, byRSS bool, limit int) string {
	var b strings.Builder

	b.WriteString(styles.SectionTitleStyle.Render(title))
	b.WriteString("\n")

	if len(procs) == 0 {
		b.WriteString(styles.DimItemStyle.Render("No processes"))
		return b.String()
	}

	// Define columns with priority (1=highest, 5=lowest)
	columns := []columnPriority{
		{"rank", widthRank, 1, true},
		{"pid", widthPID, 1, true},
		{"rss", widthRSS, 1, true},
		{"cpu", widthCPU, 1, true},
		{"comm", 8, 1, true}, // minimum width, will expand
		{"state", widthState, 2, true},
		{"uid", widthUID, 3, true},
		{"ppid", widthPPID, 4, true},
		{"persist", widthPersist, 5, true},
	}

	availableWidth := v.width - 4 // Account for panel padding/borders
	minCommWidth := 8

	// Calculate and iteratively hide lowest priority columns until we fit
	for {
		// Calculate fixed width (all visible except comm)
		fixedWidth := 0
		visibleCount := 0
		commIdx := -1
		for i := range columns {
			if columns[i].visible {
				if columns[i].name != "comm" {
					fixedWidth += columns[i].width
				} else {
					commIdx = i
				}
				visibleCount++
			}
		}

		spacesWidth := 0
		if visibleCount > 0 {
			spacesWidth = visibleCount - 1
		}

		widthComm := availableWidth - fixedWidth - spacesWidth

		if widthComm >= minCommWidth {
			if commIdx >= 0 {
				columns[commIdx].width = widthComm
			}
			break
		}

		// Find lowest priority visible column (excluding critical ones with priority 1)
		lowestPriority := 0
		lowestIdx := -1
		for i, col := range columns {
			if col.visible && col.priority > 1 && col.priority > lowestPriority { // FIX: > instead of >=
				lowestPriority = col.priority
				lowestIdx = i
			}
		}

		if lowestIdx == -1 {
			// No more columns to hide, force minimum comm width
			if commIdx >= 0 {
				columns[commIdx].width = minCommWidth
			}
			break
		}

		// Hide the lowest priority column
		columns[lowestIdx].visible = false
	}

	// Extract final widths and visibility
	var (
		wRank, wPID, wPPID, wUID, wState, wPersist, wComm, wRSS, wCPU                            int
		showRank, showPID, showPPID, showUID, showState, showPersist, showComm, showRSS, showCPU bool
	)

	for _, col := range columns {
		switch col.name {
		case "rank":
			wRank, showRank = col.width, col.visible
		case "pid":
			wPID, showPID = col.width, col.visible
		case "ppid":
			wPPID, showPPID = col.width, col.visible
		case "uid":
			wUID, showUID = col.width, col.visible
		case "state":
			wState, showState = col.width, col.visible
		case "persist":
			wPersist, showPersist = col.width, col.visible
		case "comm":
			wComm, showComm = col.width, col.visible
		case "rss":
			wRSS, showRSS = col.width, col.visible
		case "cpu":
			wCPU, showCPU = col.width, col.visible
		}
	}

	// Build header
	var headerParts []string
	if showRank {
		headerParts = append(headerParts, fmt.Sprintf("%-*s", wRank, "#"))
	}
	if showPID {
		headerParts = append(headerParts, fmt.Sprintf("%-*s", wPID, "PID"))
	}
	if showPPID {
		headerParts = append(headerParts, fmt.Sprintf("%-*s", wPPID, "PPID"))
	}
	if showUID {
		headerParts = append(headerParts, fmt.Sprintf("%-*s", wUID, "UID"))
	}
	if showState {
		headerParts = append(headerParts, fmt.Sprintf("%-*s", wState, "S"))
	}
	if showPersist {
		headerParts = append(headerParts, fmt.Sprintf("%-*s", wPersist, "HIST"))
	}
	if showComm {
		headerParts = append(headerParts, fmt.Sprintf("%-*s", wComm, "COMM"))
	}

	// Order CPU/RSS based on byRSS flag
	if byRSS {
		if showRSS {
			headerParts = append(headerParts, fmt.Sprintf("%*s", wRSS, "RSS"))
		}
		if showCPU {
			headerParts = append(headerParts, fmt.Sprintf("%*s", wCPU, "CPU"))
		}
	} else {
		if showCPU {
			headerParts = append(headerParts, fmt.Sprintf("%*s", wCPU, "CPU"))
		}
		if showRSS {
			headerParts = append(headerParts, fmt.Sprintf("%*s", wRSS, "RSS"))
		}
	}

	header := strings.Join(headerParts, " ")
	b.WriteString(styles.LabelStyle.Render(header))
	b.WriteString("\n")

	// Process entries
	count := len(procs)
	show := limit
	truncated := false
	if count > limit {
		show = limit - 1
		truncated = true
	}
	if show < 0 {
		show = 0
	}

	for i := 0; i < show && i < count; i++ {
		p := procs[i]
		var lineParts []string

		if showRank {
			rank := styles.ValueStyle.Render(fmt.Sprintf("%*d.", wRank-1, i+1))
			lineParts = append(lineParts, rank)
		}
		if showPID {
			pidStr := fmt.Sprintf("[%*d]", wPID-2, p.PID)
			pid := styles.PIDStyle.Render(pidStr)
			lineParts = append(lineParts, pid)
		}
		if showPPID {
			ppidStr := fmt.Sprintf("%*d", wPPID, p.PPID)
			ppid := styles.ValueStyle.Render(ppidStr)
			lineParts = append(lineParts, ppid)
		}
		if showUID {
			uidStr := fmt.Sprintf("%*d", wUID, p.UID)
			uid := styles.ValueStyle.Render(uidStr)
			lineParts = append(lineParts, uid)
		}
		if showState {
			stateStr := string(p.State)
			if stateStr == "" {
				stateStr = "?"
			}
			state := styles.ValueStyle.Render(fmt.Sprintf("%*s", wState, stateStr))
			lineParts = append(lineParts, state)
		}
		if showPersist {
			persist := v.renderPersistence(p.PID)
			persistStyled := styles.DimItemStyle.Render(fmt.Sprintf("%-*s", wPersist, persist))
			lineParts = append(lineParts, persistStyled)
		}
		if showComm {
			comm := p.Comm
			// FIX: Use rune count for unicode-safe truncation
			commRunes := []rune(comm)
			if len(commRunes) > wComm {
				if wComm > 3 {
					comm = string(commRunes[:wComm-3]) + "..."
				} else if wComm > 0 {
					comm = string(commRunes[:wComm])
				}
			}
			commStyled := styles.ProcessStyle.Render(fmt.Sprintf("%-*s", wComm, comm))
			lineParts = append(lineParts, commStyled)
		}

		// RSS and CPU values
		rssVal := fmt.Sprintf("%*d Mi", wRSS-3, p.RSSKiB/kiBToMiB)
		cpuVal := fmt.Sprintf("%*.1f%%", wCPU-1, p.CPUPct)

		if byRSS {
			if showRSS {
				lineParts = append(lineParts, styles.HighlightValueStyle.Render(rssVal))
			}
			if showCPU {
				lineParts = append(lineParts, styles.ValueStyle.Render(cpuVal))
			}
		} else {
			if showCPU {
				lineParts = append(lineParts, styles.HighlightValueStyle.Render(cpuVal))
			}
			if showRSS {
				lineParts = append(lineParts, styles.ValueStyle.Render(rssVal))
			}
		}

		line := strings.Join(lineParts, " ")
		// Safety: Truncate to available width
		line = truncateToWidth(line, availableWidth)

		b.WriteString(line)
		if i < show-1 || truncated {
			b.WriteString("\n")
		}
	}

	if truncated {
		b.WriteString(styles.DimItemStyle.Render(fmt.Sprintf("  ... and %d more", count-show)))
	}

	return b.String()
}

// truncateToWidth truncates a string to fit within maxWidth, accounting for ANSI codes.
func truncateToWidth(s string, maxWidth int) string {
	w := lipgloss.Width(s)
	if w <= maxWidth {
		return s
	}

	return lipgloss.NewStyle().MaxWidth(maxWidth).Render(s)
}

// applyPanelStyle applies the appropriate panel style.
func (v Viewer) applyPanelStyle(content string) string {
	style := styles.BasePanelStyle
	if v.focused {
		style = styles.ActivePanelStyle
	}

	return style.
		Width(v.width).
		Height(v.height).
		Render(content)
}

// renderPersistence generates a visual history of the process across snapshots.
func (v Viewer) renderPersistence(pid int32) string {
	if v.dump == nil {
		return ""
	}

	var b strings.Builder
	b.WriteString("[")

	// Show history for all snapshots, limited to widthPersist-2
	maxDots := widthPersist - 2
	snapCount := len(v.dump.Snapshots)

	startIdx := 0
	if snapCount > maxDots {
		// If too many snapshots, show window around current index or last N
		// Simple approach: show last N
		startIdx = snapCount - maxDots
	}

	for i := startIdx; i < snapCount; i++ {
		snap := &v.dump.Snapshots[i]
		found := false
		// Scan processes (simple linear search is fast enough for <1000 procs)
		for j := range snap.Procs {
			if snap.Procs[j].PID == pid {
				found = true
				break
			}
		}

		if i == v.snapshotIdx {
			// Highlight current snapshot
			if found {
				b.WriteString(styles.HighlightValueStyle.Render("●"))
			} else {
				// Should theoretically always be found if we are rendering it,
				// unless this function is called for a PID not in current list (unlikely)
				b.WriteString(styles.HighlightValueStyle.Render("○"))
			}
		} else {
			if found {
				b.WriteString(styles.MetricValueStyle.Render("●"))
			} else {
				b.WriteString(styles.MetricSecondaryStyle.Render("○"))
			}
		}
	}

	b.WriteString("]")
	return b.String()
}
