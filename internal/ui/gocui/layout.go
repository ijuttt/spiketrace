package gocui

// -----------------------------------------------------------------------------
// Layout Constants
// -----------------------------------------------------------------------------

const (
	// HeaderHeight is the height of the header view (including borders).
	HeaderHeight = 3

	// FooterHeight is the height of the footer view (including borders).
	FooterHeight = 2

	// ContentTopOffset is the Y position where content views start.
	ContentTopOffset = HeaderHeight

	// MinPanelWidth is the minimum width for side panels.
	MinPanelWidth = 40

	// DefaultPanelSplit is the default horizontal split ratio (50%).
	DefaultPanelSplit = 2
)

// -----------------------------------------------------------------------------
// Layout Margins
// -----------------------------------------------------------------------------

const (
	// ViewPadding is the spacing between view elements.
	ViewPadding = 1
)

// Layout manages view positioning and sizing calculations.
type Layout struct {
	maxX, maxY int
}

// NewLayout creates a new layout calculator with the given terminal size.
func NewLayout(maxX, maxY int) *Layout {
	return &Layout{maxX: maxX, maxY: maxY}
}

// HeaderBounds returns the bounds for the header view.
// Returns x0, y0, x1, y1.
func (l *Layout) HeaderBounds() (int, int, int, int) {
	return 0, 0, l.maxX - 1, HeaderHeight - 1
}

// TriggerPanelBounds returns the bounds for the trigger/info panel (left side).
// Returns x0, y0, x1, y1.
func (l *Layout) TriggerPanelBounds() (int, int, int, int) {
	contentHeight := l.contentHeight()
	return 0, ContentTopOffset, l.midX() - 1, ContentTopOffset + contentHeight
}

// ProcessPanelBounds returns the bounds for the process list panel (right side).
// Returns x0, y0, x1, y1.
func (l *Layout) ProcessPanelBounds() (int, int, int, int) {
	contentHeight := l.contentHeight()
	return l.midX(), ContentTopOffset, l.maxX - 1, ContentTopOffset + contentHeight
}

// FooterBounds returns the bounds for the footer/help view.
// Returns x0, y0, x1, y1.
func (l *Layout) FooterBounds() (int, int, int, int) {
	return 0, l.maxY - FooterHeight - 1, l.maxX - 1, l.maxY - 1
}

// midX returns the horizontal midpoint for panel splitting.
func (l *Layout) midX() int {
	return l.maxX / DefaultPanelSplit
}

// contentHeight returns the available height for content panels.
func (l *Layout) contentHeight() int {
	return l.maxY - HeaderHeight - FooterHeight - 2
}

// IsTerminalTooSmall checks if the terminal is too small for the TUI.
func (l *Layout) IsTerminalTooSmall() bool {
	return l.maxX < MinPanelWidth*2 || l.maxY < HeaderHeight+FooterHeight+5
}
