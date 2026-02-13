/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

// Package model provides data types for parsing spiketrace JSON dumps.
package model

// SpikeDump represents the top-level structure of a spike dump JSON file.
type SpikeDump struct {
	SchemaVersion   int        `json:"schema_version"`
	CreatedAt       string     `json:"created_at,omitempty"`     // v3: ISO8601 wall-clock timestamp
	UptimeSeconds   float64    `json:"uptime_seconds,omitempty"` // v3: human-readable uptime
	DumpTimestampNS uint64     `json:"dump_timestamp_ns"`        // backward compat
	Trigger         Trigger    `json:"trigger"`
	Snapshots       []Snapshot `json:"snapshots"`
}

// TriggerPolicy describes the cooldown grouping scope used by the daemon
// when deciding whether to suppress repeated triggers (schema v4+).
type TriggerPolicy struct {
	Scope       string `json:"scope"`       // "per_process", "process_group", "parent", "system"
	ScopeKey    int32  `json:"scope_key"`   // PID, PGID, PPID, or 0 depending on scope
	Description string `json:"description"` // human-readable explanation
}

// Trigger contains information about what caused the spike dump.
type Trigger struct {
	Type            string  `json:"type"`
	TypeDescription string  `json:"type_description,omitempty"` // v3: human-readable description
	PID             int32   `json:"pid"`
	Comm            string  `json:"comm"`
	CPUPct          float64 `json:"cpu_pct"`
	BaselinePct     float64 `json:"baseline_pct"`
	DeltaPct        float64 `json:"delta_pct"`
	IsNewProcess    bool    `json:"is_new_process"`
	// KiB fields (backward compatibility)
	MemAvailableKiB uint64  `json:"mem_available_kib"`
	MemBaselineKiB  uint64  `json:"mem_baseline_kib"`
	MemDeltaKiB     int64   `json:"mem_delta_kib"`
	MemUsedPct      float64 `json:"mem_used_pct"`
	SwapUsedKiB     uint64  `json:"swap_used_kib"`
	SwapBaselineKiB uint64  `json:"swap_baseline_kib"`
	SwapDeltaKiB    int64   `json:"swap_delta_kib"`
	// MiB fields (v3: human-readable)
	MemAvailableMiB uint64 `json:"mem_available_mib,omitempty"`
	MemBaselineMiB  uint64 `json:"mem_baseline_mib,omitempty"`
	MemDeltaMiB     int64  `json:"mem_delta_mib,omitempty"`
	SwapUsedMiB     uint64 `json:"swap_used_mib,omitempty"`
	SwapBaselineMiB uint64 `json:"swap_baseline_mib,omitempty"`
	SwapDeltaMiB    int64  `json:"swap_delta_mib,omitempty"`
	// v4: trigger policy context for cooldown grouping
	Policy              TriggerPolicy `json:"policy"`
	OriginSnapshotIndex int           `json:"origin_snapshot_index"`
}

// Snapshot represents a point-in-time system state.
type Snapshot struct {
	TimestampNS   uint64         `json:"timestamp_ns"`
	UptimeSeconds float64        `json:"uptime_seconds,omitempty"` // v3: human-readable
	OffsetSeconds float64        `json:"offset_seconds,omitempty"` // v3: relative to trigger
	CPU           CPUSnapshot    `json:"cpu"`
	Mem           MemSnapshot    `json:"mem"`
	Procs         []ProcessEntry `json:"procs"`
	TopRSSProcs   []ProcessEntry `json:"top_rss_procs"`
}

// CPUSnapshot contains CPU usage data.
type CPUSnapshot struct {
	GlobalPct  float64   `json:"global_pct"`
	PerCorePct []float64 `json:"per_core_pct"`
	IoWaitPct  float64   `json:"iowait_pct,omitempty"` // v5: I/O wait for disk bottleneck detection
}

// MemSnapshot contains memory usage data.
type MemSnapshot struct {
	// KiB fields (backward compatibility)
	TotalKiB     uint64 `json:"total_kib"`
	AvailableKiB uint64 `json:"available_kib"`
	FreeKiB      uint64 `json:"free_kib"`
	SwapTotalKiB uint64 `json:"swap_total_kib"`
	SwapFreeKiB  uint64 `json:"swap_free_kib"`
	ActiveKiB    uint64 `json:"active_kib"`
	InactiveKiB  uint64 `json:"inactive_kib"`
	DirtyKiB     uint64 `json:"dirty_kib"`
	SlabKiB      uint64 `json:"slab_kib"`
	ShmemKiB     uint64 `json:"shmem_kib"`
	// MiB fields (v3: human-readable)
	TotalMiB     uint64  `json:"total_mib,omitempty"`
	AvailableMiB uint64  `json:"available_mib,omitempty"`
	FreeMiB      uint64  `json:"free_mib,omitempty"`
	SwapTotalMiB uint64  `json:"swap_total_mib,omitempty"`
	SwapFreeMiB  uint64  `json:"swap_free_mib,omitempty"`
	SwapUsedMiB  uint64  `json:"swap_used_mib,omitempty"`
	ActiveMiB    uint64  `json:"active_mib,omitempty"`
	InactiveMiB  uint64  `json:"inactive_mib,omitempty"`
	DirtyMiB     uint64  `json:"dirty_mib,omitempty"`
	SlabMiB      uint64  `json:"slab_mib,omitempty"`
	ShmemMiB     uint64  `json:"shmem_mib,omitempty"`
	UsedPct      float64 `json:"used_pct,omitempty"` // v3: computed
}

// ProcessEntry represents a single process in the snapshot.
type ProcessEntry struct {
	PID     int32   `json:"pid"`
	PPID    int32   `json:"ppid,omitempty"`  // v5: parent PID for lineage tracing
	UID     uint32  `json:"uid,omitempty"`   // v5: user ID for security attribution
	State   string  `json:"state,omitempty"` // v5: process state (R/S/D/Z)
	Comm    string  `json:"comm"`
	Cmdline string  `json:"cmdline,omitempty"` // v5: full command line for forensics
	CPUPct  float64 `json:"cpu_pct"`
	RSSKiB  uint64  `json:"rss_kib"`
	RSSMiB  uint64  `json:"rss_mib,omitempty"` // v3: human-readable
}
