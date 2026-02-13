/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package analysis

import (
	"testing"

	"github.com/ijuttt/spiketrace/internal/model"
)

func TestIoWaitMetricExtract(t *testing.T) {
	m := IoWaitMetric{}

	if m.Name() != "I/O Wait" {
		t.Errorf("Name() = %q, want \"I/O Wait\"", m.Name())
	}
	if m.Unit() != "%" {
		t.Errorf("Unit() = %q, want \"%%\"", m.Unit())
	}

	snap := &model.Snapshot{
		CPU: model.CPUSnapshot{
			GlobalPct: 50.0,
			IoWaitPct: 12.5,
		},
	}

	got := m.Extract(snap)
	if got != 12.5 {
		t.Errorf("Extract() = %f, want 12.5", got)
	}
}

func TestIoWaitTimelineBuilds(t *testing.T) {
	dump := &model.SpikeDump{
		SchemaVersion: 5,
		Trigger: model.Trigger{
			Type: "cpu_spike",
			PID:  100,
			Comm: "test",
		},
		Snapshots: []model.Snapshot{
			{
				TimestampNS:   1000000000,
				OffsetSeconds: -2.0,
				CPU:           model.CPUSnapshot{GlobalPct: 10.0, IoWaitPct: 5.0},
				Mem:           model.MemSnapshot{TotalKiB: 1024, AvailableKiB: 512},
			},
			{
				TimestampNS:   2000000000,
				OffsetSeconds: -1.0,
				CPU:           model.CPUSnapshot{GlobalPct: 20.0, IoWaitPct: 15.0},
				Mem:           model.MemSnapshot{TotalKiB: 1024, AvailableKiB: 256},
			},
			{
				TimestampNS:   3000000000,
				OffsetSeconds: 0.0,
				CPU:           model.CPUSnapshot{GlobalPct: 90.0, IoWaitPct: 45.0},
				Mem:           model.MemSnapshot{TotalKiB: 1024, AvailableKiB: 128},
			},
		},
	}

	tl := BuildIoWaitTimeline(dump)

	if len(tl.Points) != 3 {
		t.Fatalf("expected 3 points, got %d", len(tl.Points))
	}

	if tl.MinValue != 5.0 {
		t.Errorf("MinValue = %f, want 5.0", tl.MinValue)
	}
	if tl.MaxValue != 45.0 {
		t.Errorf("MaxValue = %f, want 45.0", tl.MaxValue)
	}

	// Check that values are correct
	expected := []float64{5.0, 15.0, 45.0}
	for i, p := range tl.Points {
		if p.Value != expected[i] {
			t.Errorf("Point[%d].Value = %f, want %f", i, p.Value, expected[i])
		}
	}
}

func TestDefaultMetricsIncludesIoWait(t *testing.T) {
	metrics := DefaultMetrics()
	found := false
	for _, m := range metrics {
		if m.Name() == "I/O Wait" {
			found = true
			break
		}
	}
	if !found {
		t.Error("DefaultMetrics() does not include IoWaitMetric")
	}
}
