package analysis

import "github.com/jegesmk/spiketrace/internal/model"

// TimePoint represents a single point in a time-series.
type TimePoint struct {
	OffsetSeconds float64 // Relative to trigger (negative = before)
	Value         float64
	IsTrigger     bool // True if this is the trigger snapshot
}

// Timeline represents a time-series of metric values across snapshots.
type Timeline struct {
	MetricName   string
	MetricUnit   string
	Points       []TimePoint
	TriggerIndex int // Index of the trigger snapshot
	MinValue     float64
	MaxValue     float64
}

// BuildTimeline extracts time-series data from a dump using the given metric.
func BuildTimeline(dump *model.SpikeDump, metric Metric) Timeline {
	if dump == nil || len(dump.Snapshots) == 0 {
		return Timeline{}
	}

	tl := Timeline{
		MetricName:   metric.Name(),
		MetricUnit:   metric.Unit(),
		Points:       make([]TimePoint, 0, len(dump.Snapshots)),
		TriggerIndex: -1,
		MinValue:     1e18,
		MaxValue:     -1e18,
	}

	// Find trigger index (snapshot closest to offset 0 or last one)
	triggerIdx := len(dump.Snapshots) - 1
	for i, snap := range dump.Snapshots {
		if snap.OffsetSeconds >= 0 {
			triggerIdx = i
			break
		}
	}
	tl.TriggerIndex = triggerIdx

	for i, snap := range dump.Snapshots {
		val := metric.Extract(&snap)

		if val < tl.MinValue {
			tl.MinValue = val
		}
		if val > tl.MaxValue {
			tl.MaxValue = val
		}

		tl.Points = append(tl.Points, TimePoint{
			OffsetSeconds: snap.OffsetSeconds,
			Value:         val,
			IsTrigger:     i == triggerIdx,
		})
	}

	return tl
}

// BuildCPUTimeline is a convenience function for CPU timeline.
func BuildCPUTimeline(dump *model.SpikeDump) Timeline {
	return BuildTimeline(dump, CPUMetric{})
}

// BuildMemoryTimeline is a convenience function for memory usage timeline.
func BuildMemoryTimeline(dump *model.SpikeDump) Timeline {
	return BuildTimeline(dump, MemUsedMetric{})
}
