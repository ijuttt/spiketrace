// spiketrace-view is a Terminal User Interface for viewing spiketrace dumps.
package main

import (
	"fmt"
	"log"
	"os"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/jegesmk/spiketrace/internal/config"
	"github.com/jegesmk/spiketrace/internal/ui/bubbletea"
)

func main() {
	// Handle help flag
	if len(os.Args) >= 2 {
		if os.Args[1] == "-h" || os.Args[1] == "--help" {
			printUsage()
			os.Exit(0)
		}
	}

	// Create and run the Bubble Tea program
	p := tea.NewProgram(
		bubbletea.NewApp(),
		tea.WithAltScreen(),       // Use alternate screen buffer
		tea.WithMouseCellMotion(), // Enable mouse support
	)

	if _, err := p.Run(); err != nil {
		log.Fatalf("Error running program: %v", err)
	}
}

// printUsage displays usage information.
func printUsage() {
	fmt.Fprintf(os.Stderr, "Usage: %s [options]\n\n", os.Args[0])
	fmt.Fprintln(os.Stderr, "Spiketrace Viewer - A professional TUI for viewing spiketrace dumps.")
	fmt.Fprintln(os.Stderr, "")
	fmt.Fprintln(os.Stderr, "The application automatically discovers dump files from configured paths.")
	fmt.Fprintln(os.Stderr, "")
	fmt.Fprintln(os.Stderr, "Options:")
	fmt.Fprintln(os.Stderr, "  -h, --help     Show this help message")
	fmt.Fprintln(os.Stderr, "")
	fmt.Fprintln(os.Stderr, "Auto-discovery paths (searched in order):")
	for i, p := range config.GetDataPaths() {
		fmt.Fprintf(os.Stderr, "  %d. %s\n", i+1, p)
	}
	fmt.Fprintln(os.Stderr, "")
	fmt.Fprintln(os.Stderr, "Environment variables:")
	fmt.Fprintf(os.Stderr, "  %s  Override the data directory\n", config.EnvDataDir)
	fmt.Fprintln(os.Stderr, "")
	fmt.Fprintln(os.Stderr, "Keybindings:")
	fmt.Fprintln(os.Stderr, "  Tab          Switch between panels")
	fmt.Fprintln(os.Stderr, "  ↑/k ↓/j      Navigate within panel")
	fmt.Fprintln(os.Stderr, "  Enter        Select file / Load dump")
	fmt.Fprintln(os.Stderr, "  n/] p/[      Next/Previous snapshot")
	fmt.Fprintln(os.Stderr, "  r            Reload file list")
	fmt.Fprintln(os.Stderr, "  q            Quit")
}
