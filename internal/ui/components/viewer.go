package components

import (
	"fmt"
	"strings"

	"github.com/jegesmk/spiketrace/internal/model"
	"github.com/jegesmk/spiketrace/internal/ui/styles"
)

// -----------------------------------------------------------------------------
// Viewer Component
// -----------------------------------------------------------------------------

const (
	widthRank    = 3
	widthPID     = 7
	widthPersist = 12
	// widthComm is calculated dynamically
	widthRSS = 8
	widthCPU = 6
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

// renderProcessList renders a list of processes.
func (v Viewer) renderProcessList(title string, procs []model.ProcessEntry, byRSS bool, limit int) string {
	var b strings.Builder

	b.WriteString(styles.SectionTitleStyle.Render(title))
	b.WriteString("\n")

	if len(procs) == 0 {
		b.WriteString(styles.DimItemStyle.Render("No processes"))
		return b.String()
	}

	// Calculate flexible column width for Command
	// Fixed spacing: 5 spaces between 6 columns
	fixedWidth := widthRank + widthPID + widthPersist + widthRSS + widthCPU + 5
	// Available width (subtracting 4 for panel padding/borders roughly)
	widthComm := v.width - fixedWidth - 4
	if widthComm < 10 {
		widthComm = 10 // Minimum safe width
	}

	// Header
	if byRSS {
		header := fmt.Sprintf("%-*s %-*s %-*s %-*s %*s %*s",
			widthRank, "#",
			widthPID, "PID",
			widthPersist, "HIST",
			widthComm, "COMM",
			widthRSS, "RSS",
			widthCPU, "CPU")
		b.WriteString(styles.LabelStyle.Render(header))
	} else {
		header := fmt.Sprintf("%-*s %-*s %-*s %-*s %*s %*s",
			widthRank, "#",
			widthPID, "PID",
			widthPersist, "HIST",
			widthComm, "COMM",
			widthCPU, "CPU",
			widthRSS, "RSS")
		b.WriteString(styles.LabelStyle.Render(header))
	}
	b.WriteString("\n")

	// Process entries
	count := len(procs)
	show := limit
	truncated := false

	if count > limit {
		// Reserve 1 line for "... and X more"
		show = limit - 1
		truncated = true
	}

	// Safety clamp
	if show < 0 {
		show = 0
	}

	for i := 0; i < show && i < count; i++ {
		p := procs[i]

		// Rank
		rank := styles.ValueStyle.Render(fmt.Sprintf("%*d.", widthRank-1, i+1))

		// PID (dynamic width)
		// We format the PID specifically to fit the width
		pidStr := fmt.Sprintf("[%*d]", widthPID-2, p.PID)
		pid := styles.PIDStyle.Render(pidStr)

		// Persistence
		persist := v.renderPersistence(p.PID)
		persistStyled := styles.DimItemStyle.Render(fmt.Sprintf("%-*s", widthPersist, persist))

		// Command (dynamic width with truncation)
		comm := p.Comm
		// Available width for command text inside column
		maxComm := widthComm
		if len(comm) > maxComm {
			comm = comm[:maxComm] // Truncate to fit column (no ellipsis to save space, or len-1?)
			// Actually nice to have ellipsis.
			if maxComm > 3 {
				comm = comm[:maxComm-3] + "..."
			} else {
				comm = comm[:maxComm]
			}
		}
		commStyled := styles.ProcessStyle.Render(fmt.Sprintf("%-*s", widthComm, comm))

		// Values
		var line string
		if byRSS {
			rss := styles.HighlightValueStyle.Render(fmt.Sprintf("%*d Mi", widthRSS-3, p.RSSKiB/kiBToMiB))
			cpu := styles.ValueStyle.Render(fmt.Sprintf("%*.1f%%", widthCPU-1, p.CPUPct))
			line = fmt.Sprintf("%s %s %s %s %s %s", rank, pid, persistStyled, commStyled, rss, cpu)
		} else {
			cpu := styles.HighlightValueStyle.Render(fmt.Sprintf("%*.1f%%", widthCPU-1, p.CPUPct))
			rss := styles.ValueStyle.Render(fmt.Sprintf("%*d Mi", widthRSS-3, p.RSSKiB/kiBToMiB))
			line = fmt.Sprintf("%s %s %s %s %s %s", rank, pid, persistStyled, commStyled, cpu, rss)
		}

		b.WriteString(line)
		if i < show-1 || truncated {
			b.WriteString("\n")
		}
	}

	// Show count if truncated
	if truncated {
		// b.WriteString("\n") // Already added newline above
		b.WriteString(styles.DimItemStyle.Render(fmt.Sprintf("  ... and %d more", count-show)))
	}

	return b.String()
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
