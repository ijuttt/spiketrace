// Package ui defines the UI interface for the spiketrace viewer.
package ui

import "github.com/jegesmk/spiketrace/internal/app"

// UI abstracts the terminal UI implementation.
// This allows swapping gocui for another TUI library without changing app logic.
type UI interface {
	// Run starts the UI main loop with the given application state.
	Run(state *app.State) error
	// Close releases UI resources.
	Close()
}
