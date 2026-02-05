// Package render provides formatting functions for spike dump data.
package render

// -----------------------------------------------------------------------------
// Memory Size Constants
// -----------------------------------------------------------------------------

const (
	// KiBToMiB converts kibibytes to mebibytes (1024 KiB = 1 MiB).
	KiBToMiB = 1024
)

// -----------------------------------------------------------------------------
// Display Limits
// -----------------------------------------------------------------------------

const (
	// MaxCoreDisplay is the maximum number of CPU cores to display individually.
	// Beyond this count, individual core percentages are not shown.
	MaxCoreDisplay = 16

	// ProcessRankWidth is the width for displaying process rank numbers.
	ProcessRankWidth = 2

	// PIDDisplayWidth is the width for displaying process IDs.
	PIDDisplayWidth = 5

	// CommDisplayWidth is the width for displaying process command names.
	CommDisplayWidth = 15

	// CPUPctWidth is the width for displaying CPU percentage.
	CPUPctWidth = 6

	// RSSWidth is the width for displaying RSS memory in MiB.
	RSSWidth = 6
)

// -----------------------------------------------------------------------------
// Format Strings
// -----------------------------------------------------------------------------

const (
	// SectionHeaderFormat is the format for section titles.
	SectionHeaderFormat = "%s=== %s ===%s\n"
)
