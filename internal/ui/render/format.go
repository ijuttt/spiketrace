// Package render provides formatting functions for spike dump data.
package render

import (
	"fmt"

	"github.com/jegesmk/spiketrace/internal/model"
)

// Trigger formats the trigger information for display.
func Trigger(t *model.Trigger) string {
	var s string

	s += fmt.Sprintf(SectionHeaderFormat, Bold, "SPIKE TRIGGER", Reset)
	s += fmt.Sprintf("Type: %s%s%s\n", Red, t.Type, Reset)

	// Show type description if available (v3)
	if t.TypeDescription != "" {
		s += fmt.Sprintf("      %s%s%s\n", Dim, t.TypeDescription, Reset)
	}

	s += fmt.Sprintf("Process: %s[%d] %s%s\n", Cyan, t.PID, t.Comm, Reset)

	switch t.Type {
	case "cpu_delta", "cpu_new_process":
		s += fmt.Sprintf("CPU: %s%.1f%%%s (baseline: %.1f%%, delta: +%.1f%%)\n",
			Yellow, t.CPUPct, Reset, t.BaselinePct, t.DeltaPct)
	case "mem_drop":
		s += fmt.Sprintf("Available: %s%d MiB%s (baseline: %d MiB, delta: %d MiB)\n",
			Yellow, t.GetMemAvailableMiB(), Reset,
			t.GetMemBaselineMiB(), t.GetMemDeltaMiB())
	case "mem_pressure":
		s += fmt.Sprintf("RAM used: %s%.1f%%%s (available: %d MiB)\n",
			Yellow, t.MemUsedPct, Reset, t.GetMemAvailableMiB())
	case "swap_spike":
		s += fmt.Sprintf("Swap used: %s%d MiB%s (baseline: %d MiB, delta: +%d MiB)\n",
			Yellow, t.GetSwapUsedMiB(), Reset,
			t.GetSwapBaselineMiB(), t.GetSwapDeltaMiB())
	}

	return s
}

// ProcessList formats a list of processes for display.
func ProcessList(title string, procs []model.ProcessEntry, byRSS bool) string {
	var s string
	s += fmt.Sprintf(SectionHeaderFormat, Bold, title, Reset)

	for i := range procs {
		p := &procs[i]
		if byRSS {
			s += fmt.Sprintf("%*d. %s[%*d]%s %-*s %*d MiB  (CPU: %.1f%%)\n",
				ProcessRankWidth, i+1,
				Cyan, PIDDisplayWidth, p.PID, Reset,
				CommDisplayWidth, p.Comm,
				RSSWidth, p.GetRSSMiB(), p.CPUPct)
		} else {
			s += fmt.Sprintf("%*d. %s[%*d]%s %-*s %*.1f%%  (RSS: %d MiB)\n",
				ProcessRankWidth, i+1,
				Cyan, PIDDisplayWidth, p.PID, Reset,
				CommDisplayWidth, p.Comm,
				CPUPctWidth, p.CPUPct, p.GetRSSMiB())
		}
	}

	return s
}

// Memory formats memory snapshot for display.
func Memory(m *model.MemSnapshot) string {
	usedPct := m.GetUsedPct()

	var s string
	s += fmt.Sprintf(SectionHeaderFormat, Bold, "MEMORY", Reset)
	s += fmt.Sprintf("RAM: %s%.1f%% used%s (%d / %d MiB)\n",
		Yellow, usedPct, Reset,
		m.GetTotalMiB()-m.GetAvailableMiB(), m.GetTotalMiB())
	s += fmt.Sprintf("Swap: %d / %d MiB\n", m.GetSwapUsedMiB(), m.GetSwapTotalMiB())

	// Extended breakdown - show if any field is non-zero
	if m.ActiveKiB > 0 || m.InactiveKiB > 0 || m.DirtyKiB > 0 || m.SlabKiB > 0 || m.ShmemKiB > 0 {
		s += fmt.Sprintf("Active: %d MiB  Inactive: %d MiB\n",
			m.GetActiveMiB(), m.GetInactiveMiB())
		s += fmt.Sprintf("Dirty: %s%d MiB%s  Slab: %d MiB  Shmem: %d MiB\n",
			Yellow, m.GetDirtyMiB(), Reset, m.GetSlabMiB(), m.GetShmemMiB())
	}

	return s
}

// CPU formats CPU snapshot for display.
func CPU(c *model.CPUSnapshot) string {
	var s string
	s += fmt.Sprintf(SectionHeaderFormat, Bold, "CPU", Reset)
	s += fmt.Sprintf("Global: %s%.1f%%%s\n", Yellow, c.GlobalPct, Reset)

	if len(c.PerCorePct) > 0 && len(c.PerCorePct) <= MaxCoreDisplay {
		s += "Cores: "
		for i, pct := range c.PerCorePct {
			if i > 0 {
				s += " "
			}
			s += fmt.Sprintf("%.0f%%", pct)
		}
		s += "\n"
	}

	return s
}

// Header formats the header line.
func Header(dump *model.SpikeDump, snapIdx int) string {
	// Prefer human-readable timestamp if available (v3)
	if dump.CreatedAt != "" {
		return fmt.Sprintf("Spike Dump | %s | Snapshot %d/%d\n",
			dump.CreatedAt, snapIdx+1, len(dump.Snapshots))
	}
	return fmt.Sprintf("Spike Dump | Schema v%d | Snapshot %d/%d | Timestamp: %d ns\n",
		dump.SchemaVersion, snapIdx+1, len(dump.Snapshots), dump.DumpTimestampNS)
}

// Help returns the help text for the footer.
func Help() string {
	return "↑/k: Prev  ↓/j: Next  q: Quit\n"
}
