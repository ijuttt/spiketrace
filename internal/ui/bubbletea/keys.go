package bubbletea

import "github.com/charmbracelet/bubbles/key"

// -----------------------------------------------------------------------------
// Key Bindings
// -----------------------------------------------------------------------------

// KeyMap defines all keyboard shortcuts for the application.
type KeyMap struct {
	// Navigation
	Up    key.Binding
	Down  key.Binding
	Left  key.Binding
	Right key.Binding

	// Actions
	Enter  key.Binding
	Tab    key.Binding
	Escape key.Binding

	// Application
	Quit   key.Binding
	Help   key.Binding
	Reload key.Binding

	// Snapshot navigation
	NextSnapshot key.Binding
	PrevSnapshot key.Binding
}

// DefaultKeyMap returns the default key bindings.
func DefaultKeyMap() KeyMap {
	return KeyMap{
		Up: key.NewBinding(
			key.WithKeys("up", "k"),
			key.WithHelp("↑/k", "up"),
		),
		Down: key.NewBinding(
			key.WithKeys("down", "j"),
			key.WithHelp("↓/j", "down"),
		),
		Left: key.NewBinding(
			key.WithKeys("left", "h"),
			key.WithHelp("←/h", "left"),
		),
		Right: key.NewBinding(
			key.WithKeys("right", "l"),
			key.WithHelp("→/l", "right"),
		),
		Enter: key.NewBinding(
			key.WithKeys("enter"),
			key.WithHelp("enter", "select"),
		),
		Tab: key.NewBinding(
			key.WithKeys("tab"),
			key.WithHelp("tab", "next panel"),
		),
		Escape: key.NewBinding(
			key.WithKeys("esc"),
			key.WithHelp("esc", "back"),
		),
		Quit: key.NewBinding(
			key.WithKeys("q", "ctrl+c"),
			key.WithHelp("q", "quit"),
		),
		Help: key.NewBinding(
			key.WithKeys("?"),
			key.WithHelp("?", "help"),
		),
		Reload: key.NewBinding(
			key.WithKeys("r"),
			key.WithHelp("r", "reload"),
		),
		NextSnapshot: key.NewBinding(
			key.WithKeys("n", "]"),
			key.WithHelp("n/]", "next snapshot"),
		),
		PrevSnapshot: key.NewBinding(
			key.WithKeys("p", "["),
			key.WithHelp("p/[", "prev snapshot"),
		),
	}
}

// ShortHelp returns abbreviated help.
func (k KeyMap) ShortHelp() []key.Binding {
	return []key.Binding{k.Tab, k.Enter, k.Help, k.Quit}
}

// FullHelp returns complete help.
func (k KeyMap) FullHelp() [][]key.Binding {
	return [][]key.Binding{
		{k.Up, k.Down, k.Left, k.Right},
		{k.Enter, k.Tab, k.Escape},
		{k.NextSnapshot, k.PrevSnapshot},
		{k.Reload, k.Help, k.Quit},
	}
}
