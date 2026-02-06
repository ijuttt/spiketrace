/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package components

import (
	"fmt"
	"strings"
	"time"

	"github.com/charmbracelet/bubbles/key"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/ijuttt/spiketrace/internal/analysis"
	"github.com/ijuttt/spiketrace/internal/model"
	"github.com/ijuttt/spiketrace/internal/processor"
	"github.com/ijuttt/spiketrace/internal/ui/styles"
	"github.com/ijuttt/spiketrace/internal/ui/widgets"
)

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

const (
	kiBToMiB            = 1024
	minTimelineWidth    = 20
	minProgressBarWidth = 10
	widthLabel          = 10 // Fixed width for metadata labels
)

// Action buttons
const (
	ActionLoad = iota
	ActionAnalyze
	ActionExport
	ActionCount
)

// -----------------------------------------------------------------------------
// Trigger Component
// -----------------------------------------------------------------------------

// Trigger displays file metadata and trigger information.
type Trigger struct {
	// File metadata (before loading)
	selectedFile *processor.FileInfo

	// Loaded dump data
	dump        *model.SpikeDump
	snapshotIdx int

	// Cached timeline data
	cpuTimeline analysis.Timeline
	memTimeline analysis.Timeline

	// Action state
	activeAction int

	// View state
	showMemoryDetail bool

	// Layout
	width   int
	height  int
	focused bool
	title   string
}

// NewTrigger creates a new trigger panel.
func NewTrigger() Trigger {
	return Trigger{
		title:        "📋 Info & Actions",
		activeAction: ActionLoad,
	}
}

// SetSelectedFile updates the selected file metadata.
func (t *Trigger) SetSelectedFile(file *processor.FileInfo) {
	t.selectedFile = file
}

// SetDump updates the loaded dump data.
func (t *Trigger) SetDump(dump *model.SpikeDump) {
	t.dump = dump
	t.snapshotIdx = 0
	// Build timeline on load
	if dump != nil {
		t.cpuTimeline = analysis.BuildCPUTimeline(dump)
		t.memTimeline = analysis.BuildMemoryTimeline(dump)
	}
}

// SetSize updates the component dimensions.
func (t *Trigger) SetSize(width, height int) {
	t.width = width
	t.height = height
}

// SetFocused sets the focus state.
func (t *Trigger) SetFocused(focused bool) {
	t.focused = focused
}

// SnapshotIdx returns the current snapshot index.
func (t *Trigger) SnapshotIdx() int {
	return t.snapshotIdx
}

// Dump returns the currently loaded spike dump.
func (t *Trigger) Dump() *model.SpikeDump {
	return t.dump
}

// ActiveAction returns the currently selected action.
func (t *Trigger) ActiveAction() int {
	return t.activeAction
}

// CurrentSnapshot returns the currently selected snapshot.
func (t *Trigger) CurrentSnapshot() *model.Snapshot {
	if t.dump == nil {
		return nil
	}
	if t.snapshotIdx >= 0 && t.snapshotIdx < len(t.dump.Snapshots) {
		return &t.dump.Snapshots[t.snapshotIdx]
	}
	return nil
}

// TriggerIndex returns the index of the trigger snapshot.
func (t *Trigger) TriggerIndex() int {
	return t.cpuTimeline.TriggerIndex
}

// SnapshotCount returns the number of snapshots in the dump.
func (t *Trigger) SnapshotCount() int {
	if t.dump == nil {
		return 0
	}
	return len(t.dump.Snapshots)
}

// Update handles input for the trigger panel.
func (t *Trigger) Update(msg tea.Msg) tea.Cmd {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch {
		case key.Matches(msg, keyUp):
			if t.dump != nil && t.snapshotIdx > 0 {
				t.snapshotIdx--
			}
		case key.Matches(msg, keyDown):
			if t.dump != nil && t.snapshotIdx < len(t.dump.Snapshots)-1 {
				t.snapshotIdx++
			}
		// Vim-style horizontal navigation
		case msg.String() == "h":
			if t.dump != nil && t.snapshotIdx > 0 {
				t.snapshotIdx--
			}
		case msg.String() == "l":
			if t.dump != nil && t.snapshotIdx < len(t.dump.Snapshots)-1 {
				t.snapshotIdx++
			}
		// Jump to first/last
		case msg.String() == "g":
			if t.dump != nil {
				t.snapshotIdx = 0
			}
		case msg.String() == "G":
			if t.dump != nil && len(t.dump.Snapshots) > 0 {
				t.snapshotIdx = len(t.dump.Snapshots) - 1
			}
		// Jump to trigger
		case msg.String() == "0":
			if t.dump != nil && t.cpuTimeline.TriggerIndex >= 0 {
				t.snapshotIdx = t.cpuTimeline.TriggerIndex
			}
		// Toggle memory detail
		case msg.String() == "m":
			t.showMemoryDetail = !t.showMemoryDetail
		}
	}
	return nil
}

// View renders the trigger panel.
func (t Trigger) View() string {
	var b strings.Builder

	// Title
	title := styles.PanelTitleStyle.Render(t.title)
	b.WriteString(title)
	b.WriteString("\n\n")

	// Show file metadata if file is selected
	if t.selectedFile != nil {
		b.WriteString(t.renderFileInfo())
		b.WriteString("\n\n")
	}

	// Show trigger info if dump is loaded
	if t.dump != nil {
		b.WriteString(t.renderTrigger())
		b.WriteString("\n")

		// Timeline sparkline
		b.WriteString(t.renderTimeline())
		b.WriteString("\n")

		// Memory section
		snap := t.CurrentSnapshot()
		if snap != nil {
			b.WriteString(t.renderMemory(&snap.Mem))
			b.WriteString("\n")
			b.WriteString(t.renderCPU(&snap.CPU))
		}

		// Snapshot navigation
		b.WriteString("\n")
		b.WriteString(t.renderSnapshotNav())
	} else if t.selectedFile == nil {
		b.WriteString(styles.DimItemStyle.Render("Select a file to view info"))
	}

	return t.applyPanelStyle(b.String())
}

// renderFileInfo renders the selected file metadata.
func (t Trigger) renderFileInfo() string {
	var b strings.Builder
	f := t.selectedFile

	b.WriteString(styles.SectionTitleStyle.Render("File"))
	b.WriteString("\n")

	// Calculate available width for values
	// Width - Label(10) - Borders(4)
	valWidth := t.width - widthLabel - 4
	if valWidth < 10 {
		valWidth = 10
	}

	// File name (truncated)
	name := f.Name
	if len(name) > valWidth {
		// Truncate middle: "very...long.json"
		prefixLen := (valWidth - 3) / 2
		suffixLen := valWidth - 3 - prefixLen
		if prefixLen > 0 && suffixLen > 0 {
			name = name[:prefixLen] + "..." + name[len(name)-suffixLen:]
		} else {
			name = name[:valWidth]
		}
	}
	b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Name: ")))
	b.WriteString(styles.HighlightValueStyle.Render(name))
	b.WriteString("\n")

	// File size
	b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Size: ")))
	b.WriteString(styles.ValueStyle.Render(FormatFileSize(f.Size)))
	b.WriteString("\n")

	// Last modified
	b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Modified: ")))
	b.WriteString(styles.ValueStyle.Render(FormatDateTime(f.ModTime)))

	// Status indicator
	if t.dump != nil {
		b.WriteString("\n")
		b.WriteString(styles.SuccessStyle.Render("✓ Loaded"))
		// Show uptime if available
		if t.dump.UptimeSeconds > 0 {
			b.WriteString("\n")
			b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Uptime: ")))
			b.WriteString(styles.MetricSecondaryStyle.Render(formatUptime(t.dump.UptimeSeconds)))
		}
	} else {
		b.WriteString("\n")
		b.WriteString(styles.DimItemStyle.Render("Press Enter to load"))
	}

	return b.String()
}

// renderTrigger renders the trigger information.
func (t Trigger) renderTrigger() string {
	var b strings.Builder
	tr := &t.dump.Trigger
	progressBarWidth := t.width - 20
	if progressBarWidth < minProgressBarWidth {
		progressBarWidth = minProgressBarWidth
	}

	b.WriteString(styles.SectionTitleStyle.Render("Spike Trigger"))
	b.WriteString("\n")

	// Type with color based on severity
	typeStyle := styles.HighlightValueStyle
	b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Type: ")))
	b.WriteString(typeStyle.Render(tr.Type))
	b.WriteString("\n")

	// Type description (v3)
	if tr.TypeDescription != "" {
		// Indent description
		desc := tr.TypeDescription
		// Wrap or truncate? Truncate for now to keep clean lines
		descWidth := t.width - widthLabel - 4
		if len(desc) > descWidth && descWidth > 3 {
			desc = desc[:descWidth-3] + "..."
		}
		b.WriteString(strings.Repeat(" ", widthLabel))
		b.WriteString(styles.DimItemStyle.Render(desc))
		b.WriteString("\n")
	}

	// Process
	// Calculate max length for command
	// Width - Label(10) - PID(8) - " " - NewBadge(6) - Borders(4) = Width - 29
	commWidth := t.width - widthLabel - 8 - 1 - 6 - 4
	if commWidth < 10 {
		commWidth = 10
	}
	comm := tr.Comm
	if len(comm) > commWidth {
		comm = comm[:commWidth-3] + "..."
	}

	b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Process: ")))
	b.WriteString(styles.PIDStyle.Render(fmt.Sprintf("[%d]", tr.PID)))
	b.WriteString(" ")
	b.WriteString(styles.ProcessStyle.Render(comm))
	if tr.IsNewProcess {
		b.WriteString(styles.HighlightValueStyle.Render(" [NEW]"))
	}
	b.WriteString("\n")

	// Type-specific details with delta bar
	switch tr.Type {
	case "cpu_delta", "cpu_new_process":
		b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "CPU: ")))
		b.WriteString(styles.HighlightValueStyle.Render(fmt.Sprintf("%.1f%%", tr.CPUPct)))
		b.WriteString(styles.DimItemStyle.Render(fmt.Sprintf(" (Δ+%.1f%%)", tr.DeltaPct)))
		b.WriteString("\n")
		// Delta bar
		bar := widgets.NewProgressBar(tr.CPUPct, progressBarWidth).
			WithBaseline(tr.BaselinePct)
		b.WriteString(strings.Repeat(" ", widthLabel))
		b.WriteString(bar.Render())
	case "mem_drop":
		b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Available:")))
		b.WriteString(styles.HighlightValueStyle.Render(fmt.Sprintf("%d MiB", tr.GetMemAvailableMiB())))
		b.WriteString(styles.DimItemStyle.Render(fmt.Sprintf(" (Δ%d MiB)", tr.GetMemDeltaMiB())))
		// Memory delta bar (inverted: lower available = more used)
		if tr.GetMemBaselineMiB() > 0 {
			usedPct := 100.0 - (float64(tr.GetMemAvailableMiB()) / float64(tr.GetMemBaselineMiB()) * 100)
			if usedPct < 0 {
				usedPct = 0
			}
			b.WriteString("\n")
			bar := widgets.NewProgressBar(usedPct, progressBarWidth)
			b.WriteString(strings.Repeat(" ", widthLabel))
			b.WriteString(bar.Render())
		}
	case "mem_pressure":
		b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "RAM used: ")))
		b.WriteString(styles.HighlightValueStyle.Render(fmt.Sprintf("%.1f%%", tr.MemUsedPct)))
		b.WriteString("\n")
		bar := widgets.NewProgressBar(tr.MemUsedPct, progressBarWidth)
		b.WriteString(strings.Repeat(" ", widthLabel))
		b.WriteString(bar.Render())
	case "swap_spike":
		b.WriteString(styles.LabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Swap: ")))
		b.WriteString(styles.HighlightValueStyle.Render(fmt.Sprintf("%d MiB", tr.GetSwapUsedMiB())))
		b.WriteString(styles.DimItemStyle.Render(fmt.Sprintf(" (Δ+%d MiB)", tr.GetSwapDeltaMiB())))
		// Swap delta bar
		if tr.GetSwapBaselineMiB() > 0 || tr.GetSwapUsedMiB() > 0 {
			baseline := float64(tr.GetSwapBaselineMiB())
			current := float64(tr.GetSwapUsedMiB())
			maxVal := baseline
			if current > maxVal {
				maxVal = current
			}
			if maxVal > 0 {
				pct := current / maxVal * 100
				b.WriteString("\n")
				bar := widgets.NewProgressBar(pct, progressBarWidth).WithBaseline(baseline / maxVal * 100)
				b.WriteString(strings.Repeat(" ", widthLabel))
				b.WriteString(bar.Render())
			}
		}
	}

	return b.String()
}

// renderTimeline renders the CPU timeline sparkline.
func (t Trigger) renderTimeline() string {
	var b strings.Builder
	timelineWidth := t.width - 6
	if timelineWidth < minTimelineWidth {
		timelineWidth = minTimelineWidth
	}

	b.WriteString(styles.SectionTitleStyle.Render("Timeline"))
	b.WriteString("\n")

	if len(t.cpuTimeline.Points) == 0 {
		b.WriteString(styles.DimItemStyle.Render("No timeline data"))
		return b.String()
	}

	// Extract values for sparkline
	cpuValues := make([]float64, len(t.cpuTimeline.Points))
	for i, p := range t.cpuTimeline.Points {
		cpuValues[i] = p.Value
	}

	// Render CPU sparkline
	b.WriteString(styles.MetricSecondaryStyle.Render("CPU "))
	cpuSpark := widgets.NewSparkline(cpuValues, timelineWidth).
		WithHighlight(t.cpuTimeline.TriggerIndex)
	b.WriteString(cpuSpark.Render())
	b.WriteString("\n")

	// Render Memory sparkline
	if len(t.memTimeline.Points) > 0 {
		memValues := make([]float64, len(t.memTimeline.Points))
		for i, p := range t.memTimeline.Points {
			memValues[i] = p.Value
		}
		b.WriteString(styles.MetricSecondaryStyle.Render("RAM "))
		memSpark := widgets.NewSparkline(memValues, timelineWidth).
			WithHighlight(t.memTimeline.TriggerIndex)
		b.WriteString(memSpark.Render())
		b.WriteString("\n")
	}

	// Time axis labels
	if len(t.cpuTimeline.Points) > 0 {
		firstOffset := t.cpuTimeline.Points[0].OffsetSeconds
		lastOffset := t.cpuTimeline.Points[len(t.cpuTimeline.Points)-1].OffsetSeconds
		b.WriteString("    ") // Indent to match label "CPU " (4 chars)
		b.WriteString(styles.DimItemStyle.Render(fmt.Sprintf("T%.0fs", firstOffset)))
		padding := timelineWidth - 8
		if padding > 0 {
			b.WriteString(strings.Repeat(" ", padding))
		}
		b.WriteString(styles.DimItemStyle.Render(fmt.Sprintf("T%.0fs", lastOffset)))
	}

	// Current position indicator
	b.WriteString("\n")
	if t.snapshotIdx >= 0 && t.snapshotIdx < len(t.cpuTimeline.Points) {
		offset := t.cpuTimeline.Points[t.snapshotIdx].OffsetSeconds
		prefix := ""
		if offset >= 0 {
			prefix = "+"
		}
		b.WriteString(styles.LabelStyle.Render("Position: "))
		b.WriteString(styles.HighlightValueStyle.Render(fmt.Sprintf("T%s%.0fs", prefix, offset)))
	}

	return b.String()
}

// renderMemory renders memory information.
func (t Trigger) renderMemory(m *model.MemSnapshot) string {
	var b strings.Builder

	// Calculate width for inline progress bars (RAM/Swap)
	// Overhead: Label(10) + Pct(5) + Space(1) + Values(~20) + Padding(4) = ~40
	progressBarWidth := t.width - 40
	if progressBarWidth < minProgressBarWidth {
		progressBarWidth = minProgressBarWidth
	}

	// Section title with toggle hint
	if t.showMemoryDetail {
		b.WriteString(styles.SectionTitleStyle.Render("Memory ▼"))
	} else {
		b.WriteString(styles.SectionTitleStyle.Render("Memory"))
	}
	b.WriteString("\n")

	usedPct := m.GetUsedPct()
	usedMiB := m.GetTotalMiB() - m.GetAvailableMiB()
	totalMiB := m.GetTotalMiB()

	// RAM with progress bar
	b.WriteString(styles.MetricLabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "RAM")))
	bar := widgets.NewProgressBar(usedPct, progressBarWidth)
	b.WriteString(bar.Render())
	b.WriteString(" ")
	b.WriteString(styles.MetricValueStyle.Render(fmt.Sprintf("%.0f%%", usedPct)))
	b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf(" %d/%d Mi", usedMiB, totalMiB)))
	b.WriteString("\n")

	// Detailed breakdown (when toggled)
	if t.showMemoryDetail {
		activeMiB := m.GetActiveMiB()
		inactiveMiB := m.GetInactiveMiB()
		dirtyMiB := m.GetDirtyMiB()
		slabMiB := m.GetSlabMiB()
		shmemMiB := m.GetShmemMiB()

		padding := strings.Repeat(" ", widthLabel)
		b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("%sActive %4d Mi  Inactive %4d Mi", padding, activeMiB, inactiveMiB)))
		b.WriteString("\n")
		b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("%sDirty  %4d Mi  Slab     %4d Mi", padding, dirtyMiB, slabMiB)))
		if shmemMiB > 0 {
			b.WriteString("\n")
			b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("%sShmem  %4d Mi", padding, shmemMiB)))
		}
		b.WriteString("\n")
	}

	// Swap with progress bar (highlight if used)
	swapUsed := m.GetSwapUsedMiB()
	swapTotal := m.GetSwapTotalMiB()
	swapPct := float64(0)
	if swapTotal > 0 {
		swapPct = float64(swapUsed) / float64(swapTotal) * 100
	}

	b.WriteString(styles.MetricLabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "Swap")))
	swapBar := widgets.NewProgressBar(swapPct, progressBarWidth)
	b.WriteString(swapBar.Render())
	b.WriteString(" ")
	if swapUsed > 0 {
		b.WriteString(styles.HighlightValueStyle.Render(fmt.Sprintf("%d Mi", swapUsed)))
	} else {
		b.WriteString(styles.MetricSecondaryStyle.Render("0 Mi"))
	}
	b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("/%d Mi", swapTotal)))

	return b.String()
}

// renderCPU renders CPU information.
func (t Trigger) renderCPU(c *model.CPUSnapshot) string {
	var b strings.Builder

	// Calculate width for CPU bars
	// Overhead: Label(10) + Pct(5) + Padding(4) = ~19
	progressBarWidth := t.width - 19
	if progressBarWidth < minProgressBarWidth {
		progressBarWidth = minProgressBarWidth
	}

	b.WriteString(styles.SectionTitleStyle.Render("CPU"))
	b.WriteString("\n")

	// Global CPU with progress bar
	b.WriteString(styles.MetricLabelStyle.Render(fmt.Sprintf("%-*s", widthLabel, "All")))
	bar := widgets.NewProgressBar(c.GlobalPct, progressBarWidth)
	b.WriteString(bar.Render())
	b.WriteString(" ")
	b.WriteString(styles.MetricValueStyle.Render(fmt.Sprintf("%.0f%%", c.GlobalPct)))

	// Per-core display
	if len(c.PerCorePct) > 0 && len(c.PerCorePct) <= 8 {
		b.WriteString("\n")
		b.WriteString(styles.MetricSecondaryStyle.Render(strings.Repeat(" ", widthLabel)))
		for i, pct := range c.PerCorePct {
			if i > 0 {
				b.WriteString(" ")
			}
			b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("%.0f", pct)))
		}
	} else if len(c.PerCorePct) > 8 {
		b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf(" (%d cores)", len(c.PerCorePct))))
	}

	return b.String()
}

// renderSnapshotNav renders snapshot navigation with visual progress.
func (t Trigger) renderSnapshotNav() string {
	var b strings.Builder
	count := len(t.dump.Snapshots)

	// Trigger timestamp
	if t.dump.CreatedAt != "" {
		ts := formatTriggerTime(t.dump.CreatedAt)
		if ts != "" {
			b.WriteString(styles.LabelStyle.Render("Triggered "))
			b.WriteString(styles.MetricValueStyle.Render(ts))
			b.WriteString("\n")
		}
	}

	// Snapshot index with visual dots
	b.WriteString(styles.LabelStyle.Render("Snapshot  "))
	b.WriteString(styles.MetricValueStyle.Render(fmt.Sprintf("%d", t.snapshotIdx+1)))
	b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("/%d ", count)))

	// Visual progress dots (max 10 to avoid overflow)
	maxDots := 10
	if count <= maxDots {
		for i := 0; i < count; i++ {
			if i <= t.snapshotIdx {
				b.WriteString(styles.MetricValueStyle.Render("●"))
			} else {
				b.WriteString(styles.MetricSecondaryStyle.Render("○"))
			}
		}
	} else {
		// Compact indicator for many snapshots
		pct := float64(t.snapshotIdx+1) / float64(count) * 100
		b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("(%.0f%%)", pct)))
	}
	b.WriteString("\n")

	// Relative time position
	if t.snapshotIdx >= 0 && t.snapshotIdx < len(t.cpuTimeline.Points) {
		offset := t.cpuTimeline.Points[t.snapshotIdx].OffsetSeconds
		b.WriteString(styles.LabelStyle.Render("Position  "))
		if offset == 0 {
			b.WriteString(styles.HighlightValueStyle.Render("TRIGGER"))
		} else if offset < 0 {
			b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("T%.0fs", offset)))
		} else {
			b.WriteString(styles.MetricSecondaryStyle.Render(fmt.Sprintf("T+%.0fs", offset)))
		}
	}

	return b.String()
}

// formatTriggerTime extracts HH:MM:SS from ISO8601 timestamp.
func formatTriggerTime(isoTimestamp string) string {
	t, err := time.Parse(time.RFC3339, isoTimestamp)
	if err != nil {
		return ""
	}
	return t.Format("15:04:05")
}

// formatUptime converts seconds to human-readable format (Xd Xh Xm).
func formatUptime(seconds float64) string {
	totalSeconds := int64(seconds)
	days := totalSeconds / 86400
	hours := (totalSeconds % 86400) / 3600
	minutes := (totalSeconds % 3600) / 60

	if days > 0 {
		return fmt.Sprintf("%dd %dh", days, hours)
	}
	if hours > 0 {
		return fmt.Sprintf("%dh %dm", hours, minutes)
	}
	return fmt.Sprintf("%dm", minutes)
}

// applyPanelStyle applies the appropriate panel style.
func (t Trigger) applyPanelStyle(content string) string {
	style := styles.BasePanelStyle
	if t.focused {
		style = styles.ActivePanelStyle
	}

	return style.
		Width(t.width).
		Height(t.height).
		Render(content)
}
