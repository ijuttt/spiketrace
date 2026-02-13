/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package components

import (
	"fmt"
	"strings"

	"github.com/charmbracelet/bubbles/key"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/ijuttt/spiketrace/internal/model"
	"github.com/ijuttt/spiketrace/internal/ui/styles"
)

// -----------------------------------------------------------------------------
// Viewer Component
// -----------------------------------------------------------------------------

const (
	widthRank     = 3
	widthPID      = 7
	widthPPID     = 7
	widthUID      = 6
	widthState    = 2
	widthActivity = 10
	widthComm     = 8
	widthRSS      = 8
	widthCPU      = 6
)

// Process state characters and their meanings (from Linux /proc/[pid]/stat).
const (
	procStateRunning = "R" // Running or runnable
	procStateSleep   = "S" // Interruptible sleep (waiting for event)
	procStateDisk    = "D" // Uninterruptible sleep (I/O, critical section)
	procStateZombie  = "Z" // Zombie (terminated, waiting for parent)
	procStateStopped = "T" // Stopped (by job control signal)
	procStateIdle    = "I" // Idle kernel thread
)

// List identifiers for cursor navigation.
const (
	listCPU = 0 // Top by CPU
	listRSS = 1 // Top by Memory
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
	scrollY       int
	selectedRow   int // cursor position within active list
	activeList    int // listCPU or listRSS

	// Detail overlay: shows full cmdline for a selected process.
	detailProc *model.ProcessEntry
	showDetail bool
}

// NewViewer creates a new data viewer.
func NewViewer() Viewer {
	return Viewer{}
}

// activeProcs returns the process list for the currently active list.
func (v *Viewer) activeProcs() []model.ProcessEntry {
	if v.snapshot == nil {
		return nil
	}
	if v.activeList == listRSS {
		return v.snapshot.TopRSSProcs
	}
	return v.snapshot.Procs
}

// Update handles keyboard input for the viewer.
func (v *Viewer) Update(msg tea.Msg) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		// Dismiss detail overlay first
		if v.showDetail {
			if msg.String() == "esc" {
				v.HideProcessDetail()
			}
			return
		}

		procs := v.activeProcs()

		switch {
		case key.Matches(msg, keyUp):
			if v.selectedRow > 0 {
				v.selectedRow--
			} else if v.activeList == listRSS {
				// Jump to last row of CPU list
				v.activeList = listCPU
				cpuProcs := v.snapshot.Procs
				if len(cpuProcs) > 0 {
					v.selectedRow = len(cpuProcs) - 1
				}
			}
		case key.Matches(msg, keyDown):
			if procs != nil && v.selectedRow < len(procs)-1 {
				v.selectedRow++
			} else if v.activeList == listCPU && v.snapshot != nil && len(v.snapshot.TopRSSProcs) > 0 {
				// Jump to first row of RSS list
				v.activeList = listRSS
				v.selectedRow = 0
			}
		default:
			if msg.String() == "i" {
				if procs != nil && v.selectedRow < len(procs) {
					v.ShowProcessDetail(&procs[v.selectedRow])
				}
			}
		}
	}
}

// ShowProcessDetail opens the detail overlay for the given process.
func (v *Viewer) ShowProcessDetail(proc *model.ProcessEntry) {
	v.detailProc = proc
	v.showDetail = true
}

// HideProcessDetail closes the detail overlay.
func (v *Viewer) HideProcessDetail() {
	v.detailProc = nil
	v.showDetail = false
}

// IsDetailVisible returns whether the detail overlay is open.
func (v *Viewer) IsDetailVisible() bool {
	return v.showDetail
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

	// Title removed, using border title instead
	b.WriteString("\n")

	if v.snapshot == nil {
		b.WriteString(styles.DimItemStyle.Render("Select a file to view data"))
		return v.applyPanelStyle(b.String())
	}

	// Detail overlay takes precedence
	if v.showDetail && v.detailProc != nil {
		b.WriteString(v.renderProcessDetail())
		return v.applyPanelStyle(b.String())
	}

	// Calculate available rows for process lists
	// Overhead:
	// - Top/Bottom Border: 2
	// - Panel Title removed (merged into border), just 1 blank line
	// - CPU List Header (Title + Headers + blank): 3
	// - Spacer: 1
	// - Mem List Header (Title + Headers + blank): 3
	// Total Overhead inside panel: 8 lines (plus borders handled by style)
	// We subtract an extra 1 safety margin.
	// Total overhead: ~9 lines

	innerHeight := v.height - 2 // Remove borders
	availableRows := innerHeight - 10
	rowsPerList := availableRows / 2
	if rowsPerList < 1 {
		rowsPerList = 1
	}

	// Top by CPU
	b.WriteString(v.renderProcessList("Top by CPU", v.snapshot.Procs, false, rowsPerList))
	b.WriteString("\n\n")

	// Top by RSS
	b.WriteString(v.renderProcessList("Top by Memory", v.snapshot.TopRSSProcs, true, rowsPerList))

	// Legend line — explains process state codes and activity visualization
	b.WriteString("\n")
	legend := styles.DimItemStyle.Render(
		"State: " +
			styles.SuccessStyle.Render("R") + "=Running " +
			styles.ValueStyle.Render("S") + "=Sleep (Wait) " +
			styles.ErrorStyle.Render("D") + "=Disk (Stuck) " +
			styles.HighlightValueStyle.Render("Z") + "=Zombie " +
			styles.ValueStyle.Render("T") + "=Stopped " +
			"  │  ACT: █=CPU% ░=absent",
	)
	b.WriteString(legend)

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
	// Security-relevant columns (ppid, uid, state) are priority 1 — always visible.
	columns := []columnPriority{
		{"rank", widthRank, 1, true},
		{"pid", widthPID, 1, true},
		{"state", widthState, 1, true},
		{"rss", widthRSS, 1, true},
		{"cpu", widthCPU, 1, true},
		{"comm", 8, 2, true}, // minimum width, will expand
		{"ppid", widthPPID, 3, true},
		{"uid", widthUID, 3, true},
		{"activity", widthActivity, 4, true},
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
		wRank, wPID, wPPID, wUID, wState, wActivity, wComm, wRSS, wCPU                            int
		showRank, showPID, showPPID, showUID, showState, showActivity, showComm, showRSS, showCPU bool
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
		case "activity":
			wActivity, showActivity = col.width, col.visible
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
	if showActivity {
		headerParts = append(headerParts, fmt.Sprintf("%-*s", wActivity, "ACT"))
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
			stateStyle := stateColorStyle(stateStr)
			state := stateStyle.Render(fmt.Sprintf("%*s", wState, stateStr))
			lineParts = append(lineParts, state)
		}
		if showActivity {
			activity := v.renderActivity(p.PID)
			activityStyled := styles.DimItemStyle.Render(fmt.Sprintf("%-*s", wActivity, activity))
			lineParts = append(lineParts, activityStyled)
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

		// Highlight the selected row when this list is active and viewer is focused
		listID := listCPU
		if byRSS {
			listID = listRSS
		}
		isSelected := v.focused && v.activeList == listID && i == v.selectedRow
		if isSelected {
			line = styles.SelectedItemStyle.Render("▸" + line)
		} else {
			line = " " + line
		}

		b.WriteString(line)
		if i < show-1 || truncated {
			b.WriteString("\n")
		}
	}

	if truncated {
		b.WriteString(styles.DimItemStyle.Render(fmt.Sprintf("  ... and %d more", count-show)))
	}

	// Synthetic "[others]" row for CPU list: shows hidden workload
	if !byRSS && v.snapshot != nil {
		sumCPU := 0.0
		for _, p := range procs[:show] {
			sumCPU += p.CPUPct
		}
		gap := v.snapshot.CPU.GlobalPct - sumCPU
		if gap > 1.0 {
			// Severity-based styling
			var gapStyle lipgloss.Style
			switch {
			case gap > 50:
				gapStyle = styles.ErrorStyle // Red — critical hidden load
			case gap > 20:
				gapStyle = styles.HighlightValueStyle // Pink — significant
			case gap > 5:
				gapStyle = lipgloss.NewStyle().Foreground(styles.ColorWarning).Bold(true) // Orange
			default:
				gapStyle = styles.MetricSecondaryStyle // Dim — minor
			}
			othersLine := fmt.Sprintf("  ─ [others]%*s%5.1f%%",
				wComm+wActivity+wState+wPPID+wUID-3, "", gap)
			b.WriteString("\n")
			b.WriteString(gapStyle.Render(othersLine))
		}
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
	baseStyle := styles.BasePanelStyle
	if v.focused {
		baseStyle = styles.ActivePanelStyle
	}

	// Extract the border currently set on the style so we can inject the title.
	border, hasTop, hasRight, hasBottom, hasLeft := baseStyle.GetBorder()
	_ = hasRight
	_ = hasBottom
	_ = hasLeft
	if hasTop {
		border = styles.BuildTitledBorder("Process View", v.width, border)
		baseStyle = baseStyle.BorderStyle(border)
	}

	return baseStyle.
		Width(v.width).
		Height(v.height).
		Render(content)
}

// activityBlocks maps CPU percentage (0-100%) to Unicode block height.
// Uses absolute scale: ▁ ≈ 0%, █ ≈ 100%.
var activityBlocks = []rune{'▁', '▂', '▃', '▄', '▅', '▆', '▇', '█'}

// absentBlock indicates the process was not in the top-N for that snapshot.
const absentBlock = "░"

// cpuToBlock converts a CPU percentage (0-100) to a sparkline block character.
func cpuToBlock(cpuPct float64) string {
	// Absolute scale: 0% → ▁, 100% → █
	const maxLevels = 7 // index 0-7
	idx := int(cpuPct / 100.0 * maxLevels)
	if idx > maxLevels {
		idx = maxLevels
	}
	if idx < 0 {
		idx = 0
	}
	return string(activityBlocks[idx])
}

// renderActivity generates a per-process CPU activity sparkline across snapshots.
// Each position shows the process's CPU% as a block height (▁-█) or ░ if absent.
func (v Viewer) renderActivity(pid int32) string {
	if v.dump == nil {
		return ""
	}

	var b strings.Builder
	b.WriteString("[")

	// Show activity for all snapshots, limited to widthActivity-2 (brackets)
	maxBlocks := widthActivity - 2
	snapCount := len(v.dump.Snapshots)

	// Snapshots are stored [Newest...Oldest] by the daemon.
	// We want to display [Oldest...Newest] left-to-right to match the Info & Actions timeline.
	endIdx := snapCount - 1 // oldest
	startIdx := 0           // newest
	if snapCount > maxBlocks {
		// Show the most recent maxBlocks snapshots
		startIdx = snapCount - maxBlocks
	}

	// Iterate BACKWARD (newest → oldest in array = oldest → newest visually)
	for i := endIdx; i >= startIdx; i-- {
		snap := &v.dump.Snapshots[i]
		cpuPct := -1.0 // sentinel: not found

		// Scan processes for this PID
		for j := range snap.Procs {
			if snap.Procs[j].PID == pid {
				cpuPct = snap.Procs[j].CPUPct
				break
			}
		}

		isCurrent := i == v.snapshotIdx

		if cpuPct < 0 {
			// Process absent from this snapshot
			if isCurrent {
				b.WriteString(styles.HighlightValueStyle.Render(absentBlock))
			} else {
				b.WriteString(styles.MetricSecondaryStyle.Render(absentBlock))
			}
		} else {
			block := cpuToBlock(cpuPct)
			if isCurrent {
				b.WriteString(styles.HighlightValueStyle.Render(block))
			} else {
				b.WriteString(styles.MetricValueStyle.Render(block))
			}
		}
	}

	b.WriteString("]")
	return b.String()
}

// stateColorStyle returns a lipgloss style that color-codes process state.
// D (disk/uninterruptible) and Z (zombie) are operationally significant.
func stateColorStyle(state string) lipgloss.Style {
	switch state {
	case procStateDisk:
		return styles.ErrorStyle // Red — D-state signals I/O stall
	case procStateZombie:
		return styles.HighlightValueStyle // Orange — needs parent reap
	case procStateRunning:
		return styles.SuccessStyle // Green — actively running
	default:
		return styles.ValueStyle // Default for S, T, etc.
	}
}

// renderProcessDetail renders a detail overlay for a single process.
// Shows full cmdline and attribution data that won't fit in the table.
func (v Viewer) renderProcessDetail() string {
	p := v.detailProc

	var b strings.Builder
	b.WriteString(styles.SectionTitleStyle.Render("Process Detail"))
	b.WriteString("\n\n")

	// PID + State
	b.WriteString(styles.LabelStyle.Render("PID:     "))
	b.WriteString(styles.PIDStyle.Render(fmt.Sprintf("%d", p.PID)))
	if p.State != "" {
		b.WriteString("  ")
		b.WriteString(stateColorStyle(p.State).Render(fmt.Sprintf("[%s]", p.State)))
	}
	b.WriteString("\n")

	// PPID
	b.WriteString(styles.LabelStyle.Render("PPID:    "))
	b.WriteString(styles.ValueStyle.Render(fmt.Sprintf("%d", p.PPID)))
	b.WriteString("\n")

	// UID
	b.WriteString(styles.LabelStyle.Render("UID:     "))
	b.WriteString(styles.ValueStyle.Render(fmt.Sprintf("%d", p.UID)))
	b.WriteString("\n")

	// Comm
	b.WriteString(styles.LabelStyle.Render("Comm:    "))
	b.WriteString(styles.ProcessStyle.Render(p.Comm))
	b.WriteString("\n")

	// Full command line (the key data point that was never shown before)
	b.WriteString(styles.LabelStyle.Render("Cmdline: "))
	cmdline := p.Cmdline
	if cmdline == "" {
		cmdline = "(unavailable)"
	}
	// Word-wrap within available width
	wrapWidth := v.width - 13 // 9 (label) + 4 (borders)
	if wrapWidth < 20 {
		wrapWidth = 20
	}
	b.WriteString(styles.HighlightValueStyle.Render(wrapLine(cmdline, wrapWidth)))
	b.WriteString("\n\n")

	// Metrics
	b.WriteString(styles.LabelStyle.Render("CPU:     "))
	b.WriteString(styles.MetricValueStyle.Render(fmt.Sprintf("%.1f%%", p.CPUPct)))
	b.WriteString("\n")

	b.WriteString(styles.LabelStyle.Render("RSS:     "))
	b.WriteString(styles.MetricValueStyle.Render(fmt.Sprintf("%d MiB", p.GetRSSMiB())))
	b.WriteString("\n\n")

	b.WriteString(styles.DimItemStyle.Render("Press Esc to close"))

	return b.String()
}

// wrapLine performs simple word-wrapping at the given width.
func wrapLine(s string, width int) string {
	if len(s) <= width {
		return s
	}

	var b strings.Builder
	for len(s) > width {
		b.WriteString(s[:width])
		b.WriteString("\n         ") // Align with "Cmdline: " prefix
		s = s[width:]
	}
	b.WriteString(s)
	return b.String()
}
