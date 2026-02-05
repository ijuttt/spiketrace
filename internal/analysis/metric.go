/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

// Package analysis provides viewer-side computation for spike dump data.
// This layer sits between the raw model types and presentation, computing
// derived metrics, time-series data, and cross-snapshot analysis.
package analysis

import "github.com/jegesmk/spiketrace/internal/model"

// Metric defines a pluggable data extractor for snapshot analysis.
// Implement this interface to add new metrics without modifying rendering code.
type Metric interface {
	Name() string
	Unit() string
	Extract(snap *model.Snapshot) float64
}

// CPUMetric extracts global CPU usage percentage.
type CPUMetric struct{}

func (CPUMetric) Name() string                      { return "CPU" }
func (CPUMetric) Unit() string                      { return "%" }
func (CPUMetric) Extract(s *model.Snapshot) float64 { return s.CPU.GlobalPct }

// MemUsedMetric extracts memory usage percentage.
type MemUsedMetric struct{}

func (MemUsedMetric) Name() string                      { return "Memory" }
func (MemUsedMetric) Unit() string                      { return "%" }
func (MemUsedMetric) Extract(s *model.Snapshot) float64 { return s.Mem.GetUsedPct() }

// SwapUsedMetric extracts swap usage in MiB.
type SwapUsedMetric struct{}

func (SwapUsedMetric) Name() string                      { return "Swap" }
func (SwapUsedMetric) Unit() string                      { return "MiB" }
func (SwapUsedMetric) Extract(s *model.Snapshot) float64 { return float64(s.Mem.GetSwapUsedMiB()) }

// MemAvailableMetric extracts available memory in MiB.
type MemAvailableMetric struct{}

func (MemAvailableMetric) Name() string                      { return "Available" }
func (MemAvailableMetric) Unit() string                      { return "MiB" }
func (MemAvailableMetric) Extract(s *model.Snapshot) float64 { return float64(s.Mem.GetAvailableMiB()) }
