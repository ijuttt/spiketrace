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
