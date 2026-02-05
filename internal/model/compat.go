// Package model provides compatibility helpers for different schema versions.
package model

// GetTotalMiB returns total memory in MiB, computing from KiB if MiB field is 0.
func (m *MemSnapshot) GetTotalMiB() uint64 {
	if m.TotalMiB > 0 {
		return m.TotalMiB
	}
	return m.TotalKiB / 1024
}

// GetAvailableMiB returns available memory in MiB.
func (m *MemSnapshot) GetAvailableMiB() uint64 {
	if m.AvailableMiB > 0 {
		return m.AvailableMiB
	}
	return m.AvailableKiB / 1024
}

// GetFreeMiB returns free memory in MiB.
func (m *MemSnapshot) GetFreeMiB() uint64 {
	if m.FreeMiB > 0 {
		return m.FreeMiB
	}
	return m.FreeKiB / 1024
}

// GetSwapTotalMiB returns swap total in MiB.
func (m *MemSnapshot) GetSwapTotalMiB() uint64 {
	if m.SwapTotalMiB > 0 {
		return m.SwapTotalMiB
	}
	return m.SwapTotalKiB / 1024
}

// GetSwapFreeMiB returns swap free in MiB.
func (m *MemSnapshot) GetSwapFreeMiB() uint64 {
	if m.SwapFreeMiB > 0 {
		return m.SwapFreeMiB
	}
	return m.SwapFreeKiB / 1024
}

// GetSwapUsedMiB returns swap used in MiB.
func (m *MemSnapshot) GetSwapUsedMiB() uint64 {
	if m.SwapUsedMiB > 0 {
		return m.SwapUsedMiB
	}
	return (m.SwapTotalKiB - m.SwapFreeKiB) / 1024
}

// GetActiveMiB returns active memory in MiB.
func (m *MemSnapshot) GetActiveMiB() uint64 {
	if m.ActiveMiB > 0 {
		return m.ActiveMiB
	}
	return m.ActiveKiB / 1024
}

// GetInactiveMiB returns inactive memory in MiB.
func (m *MemSnapshot) GetInactiveMiB() uint64 {
	if m.InactiveMiB > 0 {
		return m.InactiveMiB
	}
	return m.InactiveKiB / 1024
}

// GetDirtyMiB returns dirty memory in MiB.
func (m *MemSnapshot) GetDirtyMiB() uint64 {
	if m.DirtyMiB > 0 {
		return m.DirtyMiB
	}
	return m.DirtyKiB / 1024
}

// GetSlabMiB returns slab memory in MiB.
func (m *MemSnapshot) GetSlabMiB() uint64 {
	if m.SlabMiB > 0 {
		return m.SlabMiB
	}
	return m.SlabKiB / 1024
}

// GetShmemMiB returns shared memory in MiB.
func (m *MemSnapshot) GetShmemMiB() uint64 {
	if m.ShmemMiB > 0 {
		return m.ShmemMiB
	}
	return m.ShmemKiB / 1024
}

// GetUsedPct returns memory usage percentage.
func (m *MemSnapshot) GetUsedPct() float64 {
	if m.UsedPct > 0 {
		return m.UsedPct
	}
	if m.TotalKiB == 0 {
		return 0
	}
	return 100.0 * float64(m.TotalKiB-m.AvailableKiB) / float64(m.TotalKiB)
}

// GetRSSMiB returns process RSS in MiB.
func (p *ProcessEntry) GetRSSMiB() uint64 {
	if p.RSSMiB > 0 {
		return p.RSSMiB
	}
	return p.RSSKiB / 1024
}

// GetMemAvailableMiB returns trigger memory available in MiB.
func (t *Trigger) GetMemAvailableMiB() uint64 {
	if t.MemAvailableMiB > 0 {
		return t.MemAvailableMiB
	}
	return t.MemAvailableKiB / 1024
}

// GetMemBaselineMiB returns trigger memory baseline in MiB.
func (t *Trigger) GetMemBaselineMiB() uint64 {
	if t.MemBaselineMiB > 0 {
		return t.MemBaselineMiB
	}
	return t.MemBaselineKiB / 1024
}

// GetMemDeltaMiB returns trigger memory delta in MiB.
func (t *Trigger) GetMemDeltaMiB() int64 {
	if t.MemDeltaMiB != 0 {
		return t.MemDeltaMiB
	}
	return t.MemDeltaKiB / 1024
}

// GetSwapUsedMiB returns trigger swap used in MiB.
func (t *Trigger) GetSwapUsedMiB() uint64 {
	if t.SwapUsedMiB > 0 {
		return t.SwapUsedMiB
	}
	return t.SwapUsedKiB / 1024
}

// GetSwapBaselineMiB returns trigger swap baseline in MiB.
func (t *Trigger) GetSwapBaselineMiB() uint64 {
	if t.SwapBaselineMiB > 0 {
		return t.SwapBaselineMiB
	}
	return t.SwapBaselineKiB / 1024
}

// GetSwapDeltaMiB returns trigger swap delta in MiB.
func (t *Trigger) GetSwapDeltaMiB() int64 {
	if t.SwapDeltaMiB != 0 {
		return t.SwapDeltaMiB
	}
	return t.SwapDeltaKiB / 1024
}
