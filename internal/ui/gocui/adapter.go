/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

// Package gocui provides the gocui-based TUI implementation.
package gocui

import (
	"github.com/ijuttt/spiketrace/internal/app"
	"github.com/ijuttt/spiketrace/internal/ui/render"
	lib "github.com/jroimartin/gocui"
)

// -----------------------------------------------------------------------------
// View Names
// -----------------------------------------------------------------------------

const (
	ViewHeader  = "header"
	ViewTrigger = "trigger"
	ViewProcs   = "procs"
	ViewFooter  = "footer"
)

// -----------------------------------------------------------------------------
// Adapter Implementation
// -----------------------------------------------------------------------------

// Adapter implements ui.UI using gocui.
type Adapter struct {
	gui    *lib.Gui
	state  *app.State
	layout *Layout
}

// New creates a new gocui adapter.
func New() (*Adapter, error) {
	g, err := lib.NewGui(lib.OutputNormal)
	if err != nil {
		return nil, err
	}
	g.Cursor = false
	return &Adapter{gui: g}, nil
}

// Run implements ui.UI.
func (a *Adapter) Run(state *app.State) error {
	a.state = state
	a.gui.SetManagerFunc(a.layoutManager)
	if err := a.setupBindings(); err != nil {
		return err
	}
	err := a.gui.MainLoop()
	if err == lib.ErrQuit {
		return nil
	}
	return err
}

// Close implements ui.UI.
func (a *Adapter) Close() {
	a.gui.Close()
}

// -----------------------------------------------------------------------------
// Layout Management
// -----------------------------------------------------------------------------

// layoutManager creates and updates all views.
func (a *Adapter) layoutManager(g *lib.Gui) error {
	maxX, maxY := g.Size()
	a.layout = NewLayout(maxX, maxY)

	// Check terminal size
	if a.layout.IsTerminalTooSmall() {
		// Could render a "terminal too small" message here
		// For now, just proceed with cramped layout
	}

	if err := a.setupHeader(g); err != nil {
		return err
	}
	if err := a.setupTriggerPanel(g); err != nil {
		return err
	}
	if err := a.setupProcessPanel(g); err != nil {
		return err
	}
	if err := a.setupFooter(g); err != nil {
		return err
	}

	return a.renderAll()
}

// setupHeader initializes the header view.
func (a *Adapter) setupHeader(g *lib.Gui) error {
	x0, y0, x1, y1 := a.layout.HeaderBounds()
	v, err := g.SetView(ViewHeader, x0, y0, x1, y1)
	if err != nil && err != lib.ErrUnknownView {
		return err
	}
	v.Frame = false
	return nil
}

// setupTriggerPanel initializes the trigger/info panel.
func (a *Adapter) setupTriggerPanel(g *lib.Gui) error {
	x0, y0, x1, y1 := a.layout.TriggerPanelBounds()
	v, err := g.SetView(ViewTrigger, x0, y0, x1, y1)
	if err != nil && err != lib.ErrUnknownView {
		return err
	}
	v.Title = " Trigger "
	v.Wrap = true
	return nil
}

// setupProcessPanel initializes the process list panel.
func (a *Adapter) setupProcessPanel(g *lib.Gui) error {
	x0, y0, x1, y1 := a.layout.ProcessPanelBounds()
	v, err := g.SetView(ViewProcs, x0, y0, x1, y1)
	if err != nil && err != lib.ErrUnknownView {
		return err
	}
	v.Title = " Processes "
	v.Wrap = true
	return nil
}

// setupFooter initializes the footer/help view.
func (a *Adapter) setupFooter(g *lib.Gui) error {
	x0, y0, x1, y1 := a.layout.FooterBounds()
	v, err := g.SetView(ViewFooter, x0, y0, x1, y1)
	if err != nil && err != lib.ErrUnknownView {
		return err
	}
	v.Frame = false
	return nil
}

// -----------------------------------------------------------------------------
// Rendering
// -----------------------------------------------------------------------------

// renderAll updates all view contents.
func (a *Adapter) renderAll() error {
	dump := a.state.Dump()
	snap := a.state.CurrentSnapshot()
	idx := a.state.SelectedIdx()

	// Header
	if v, err := a.gui.View(ViewHeader); err == nil {
		v.Clear()
		v.Write([]byte(render.Header(dump, idx)))
	}

	// Trigger panel
	if v, err := a.gui.View(ViewTrigger); err == nil {
		v.Clear()
		v.Write([]byte(render.Trigger(&dump.Trigger)))
		if snap != nil {
			v.Write([]byte("\n"))
			v.Write([]byte(render.Memory(&snap.Mem)))
			v.Write([]byte("\n"))
			v.Write([]byte(render.CPU(&snap.CPU)))
		}
	}

	// Process list panel
	if v, err := a.gui.View(ViewProcs); err == nil {
		v.Clear()
		if snap != nil {
			v.Write([]byte(render.ProcessList("TOP BY CPU", snap.Procs, false)))
			v.Write([]byte("\n"))
			v.Write([]byte(render.ProcessList("TOP BY RSS", snap.TopRSSProcs, true)))
		}
	}

	// Footer
	if v, err := a.gui.View(ViewFooter); err == nil {
		v.Clear()
		v.Write([]byte(render.Help()))
	}

	return nil
}

// -----------------------------------------------------------------------------
// Key Bindings
// -----------------------------------------------------------------------------

// setupBindings configures keybindings.
func (a *Adapter) setupBindings() error {
	bindings := []struct {
		key     interface{}
		handler func(*lib.Gui, *lib.View) error
	}{
		{lib.KeyCtrlC, a.quit},
		{'q', a.quit},
		{lib.KeyArrowDown, a.nextSnapshot},
		{'j', a.nextSnapshot},
		{lib.KeyArrowUp, a.prevSnapshot},
		{'k', a.prevSnapshot},
	}

	for _, b := range bindings {
		if err := a.gui.SetKeybinding("", b.key, lib.ModNone, b.handler); err != nil {
			return err
		}
	}

	return nil
}

func (a *Adapter) quit(g *lib.Gui, v *lib.View) error {
	return lib.ErrQuit
}

func (a *Adapter) nextSnapshot(g *lib.Gui, v *lib.View) error {
	a.state.SelectNext()
	return nil
}

func (a *Adapter) prevSnapshot(g *lib.Gui, v *lib.View) error {
	a.state.SelectPrev()
	return nil
}
