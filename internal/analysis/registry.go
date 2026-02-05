/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package analysis

// DefaultMetrics returns the standard set of metrics for spike analysis.
func DefaultMetrics() []Metric {
	return []Metric{
		CPUMetric{},
		MemUsedMetric{},
		SwapUsedMetric{},
		MemAvailableMetric{},
	}
}
