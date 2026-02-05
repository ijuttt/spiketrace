# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -MMD -MP
LDFLAGS = -lpthread
PREFIX ?= /usr/local

# Project Structure
SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# -----------------------------------------------------------------------------
# Source Discovery & Object Generation
# -----------------------------------------------------------------------------

# Daemon sources (exclude viewer-specific files)
DAEMON_SRCS = $(wildcard $(SRC_DIR)/*.c)
DAEMON_SRC_FILTERED = $(filter-out $(SRC_DIR)/spktrace_view.c $(SRC_DIR)/json_reader.c, $(DAEMON_SRCS))
DAEMON_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(DAEMON_SRC_FILTERED))

# Viewer sources
VIEWER_SRCS = $(SRC_DIR)/json_reader.c $(SRC_DIR)/spktrace_view.c
VIEWER_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(VIEWER_SRCS))

# Binary Outputs
DAEMON_OUT = $(BUILD_DIR)/spiketrace
# Legacy CLI Viewer
VIEWER_OUT = $(BUILD_DIR)/spiketrace-view-cli
# TUI Viewer (New Default)
TUI_OUT = $(BUILD_DIR)/spiketrace-view

# -----------------------------------------------------------------------------
# Build Targets
# -----------------------------------------------------------------------------

all: $(DAEMON_OUT) $(VIEWER_OUT) tui

# Link Daemon Binary
$(DAEMON_OUT): $(DAEMON_OBJS)
	@mkdir -p $(@D)
	$(CC) $(DAEMON_OBJS) -o $@ $(LDFLAGS)

# Link CLI Viewer Binary
$(VIEWER_OUT): $(VIEWER_OBJS)
	@mkdir -p $(@D)
	$(CC) $(VIEWER_OBJS) -o $@

# Compile Object Files (Generic Pattern Rule)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Include Auto-Generated Dependencies
-include $(DAEMON_OBJS:.o=.d)
-include $(VIEWER_OBJS:.o=.d)

# -----------------------------------------------------------------------------
# Utility Targets
# -----------------------------------------------------------------------------

run: $(DAEMON_OUT)
	$(DAEMON_OUT)

clean:
	rm -rf $(BUILD_DIR)

# Go TUI Viewer (requires Go toolchain)
tui:
	@mkdir -p $(BUILD_DIR)
	@echo "Resolving Go dependencies..."
	go mod tidy
	go build -o $(TUI_OUT) ./cmd/spiketrace-view

install: all
	install -D $(DAEMON_OUT) $(DESTDIR)$(PREFIX)/bin/spiketrace
	install -D $(VIEWER_OUT) $(DESTDIR)$(PREFIX)/bin/spiketrace-view-cli
	install -D $(TUI_OUT) $(DESTDIR)$(PREFIX)/bin/spiketrace-view
	install -D spiketrace.service $(DESTDIR)/etc/systemd/system/spiketrace.service
	mkdir -p $(DESTDIR)/var/lib/spiketrace

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/spiketrace
	rm -f $(DESTDIR)$(PREFIX)/bin/spiketrace-view-cli
	rm -f $(DESTDIR)$(PREFIX)/bin/spiketrace-view
	rm -f $(DESTDIR)/etc/systemd/system/spiketrace.service

.PHONY: all run clean install uninstall tui
