/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

// Package app provides core application state and business logic.
package app

import (
	"sync"

	"github.com/ijuttt/spiketrace/internal/model"
)

// State holds the application state in a thread-safe manner.
type State struct {
	mu          sync.RWMutex
	dump        *model.SpikeDump
	selectedIdx int
	showHelp    bool
}

// NewState creates a new application state with the given dump.
func NewState(dump *model.SpikeDump) *State {
	return &State{
		dump:        dump,
		selectedIdx: 0,
		showHelp:    false,
	}
}

// Dump returns the spike dump (read-only).
func (s *State) Dump() *model.SpikeDump {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.dump
}

// SelectedIdx returns the currently selected snapshot index.
func (s *State) SelectedIdx() int {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.selectedIdx
}

// CurrentSnapshot returns the currently selected snapshot.
func (s *State) CurrentSnapshot() *model.Snapshot {
	s.mu.RLock()
	defer s.mu.RUnlock()
	if s.selectedIdx >= 0 && s.selectedIdx < len(s.dump.Snapshots) {
		return &s.dump.Snapshots[s.selectedIdx]
	}
	return nil
}

// SelectNext moves to the next snapshot.
func (s *State) SelectNext() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.selectedIdx < len(s.dump.Snapshots)-1 {
		s.selectedIdx++
	}
}

// SelectPrev moves to the previous snapshot.
func (s *State) SelectPrev() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.selectedIdx > 0 {
		s.selectedIdx--
	}
}

// ToggleHelp toggles the help display.
func (s *State) ToggleHelp() {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.showHelp = !s.showHelp
}

// ShowHelp returns whether help is currently shown.
func (s *State) ShowHelp() bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.showHelp
}
