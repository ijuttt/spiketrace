# Compiler and Flags
CC ?= gcc

# Installation directories (FHS compliant)
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
DATADIR ?= $(PREFIX)/share
MANDIR ?= $(DATADIR)/man
DOCDIR ?= $(DATADIR)/doc/spiketrace
LICENSEDIR ?= $(DATADIR)/licenses/spiketrace

ifeq ($(PREFIX),/usr)
  SYSCONFDIR ?= /etc
else
  SYSCONFDIR ?= $(PREFIX)/etc
endif
STATEDIR ?= /var/lib/spiketrace

SYSTEMDDIR ?= /usr/lib/systemd
TMPFILESDIR ?= /usr/lib/tmpfiles.d

# Project-specific compiler flags (appended to allow distro overrides)
# Environment CFLAGS/LDFLAGS are respected and prepended
PROJ_CFLAGS = -Wall -Wextra -Iinclude -MMD -MP
PROJ_LDFLAGS = -lpthread

# Build-time path defines (enables distro-specific path configuration)
PROJ_CFLAGS += -DCONFIG_SYSTEM_PATH='"$(SYSCONFDIR)/spiketrace/config.toml"'
PROJ_CFLAGS += -DSPIKE_DUMP_DEFAULT_DIR='"$(STATEDIR)"'

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

all: $(DAEMON_OUT) $(VIEWER_OUT) tui gen-service

# Link Daemon Binary
$(DAEMON_OUT): $(DAEMON_OBJS)
	@mkdir -p $(@D)
	$(CC) $(DAEMON_OBJS) -o $@ $(LDFLAGS) $(PROJ_LDFLAGS)

# Link CLI Viewer Binary
$(VIEWER_OUT): $(VIEWER_OBJS)
	@mkdir -p $(@D)
	$(CC) $(VIEWER_OBJS) -o $@ $(LDFLAGS) $(PROJ_LDFLAGS)

# Compile Object Files (Generic Pattern Rule)
# CFLAGS from environment is respected, PROJ_CFLAGS adds project-specific flags
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(PROJ_CFLAGS) -c $< -o $@

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
# Uses PIE for security, trimpath for reproducibility, vendor for offline builds
tui:
	@mkdir -p $(BUILD_DIR)
	go build -buildmode=pie -trimpath -mod=vendor -o $(TUI_OUT) ./cmd/spiketrace-view

vendor:
	go mod tidy
	go mod vendor
	@echo "Vendor directory updated. Commit vendor/ changes."


install: all install-bin install-config install-man install-license install-systemd

install-bin:
	install -Dm755 $(DAEMON_OUT) $(DESTDIR)$(BINDIR)/spiketrace
	install -Dm755 $(VIEWER_OUT) $(DESTDIR)$(BINDIR)/spiketrace-view-cli
	install -Dm755 $(TUI_OUT) $(DESTDIR)$(BINDIR)/spiketrace-view

install-config:
	# Install example config to doc directory (always)
	install -Dm644 examples/config.toml $(DESTDIR)$(DOCDIR)/config.toml.example
	# Install actual config only if it doesn't exist (don't overwrite user config)
	@if [ ! -f "$(DESTDIR)$(SYSCONFDIR)/spiketrace/config.toml" ]; then \
		install -Dm644 examples/config.toml $(DESTDIR)$(SYSCONFDIR)/spiketrace/config.toml; \
		echo "Installed config to $(DESTDIR)$(SYSCONFDIR)/spiketrace/config.toml"; \
	else \
		echo "Config exists, not overwriting: $(DESTDIR)$(SYSCONFDIR)/spiketrace/config.toml"; \
	fi

install-man:
	install -Dm644 man/spiketrace.1 $(DESTDIR)$(MANDIR)/man1/spiketrace.1
	install -Dm644 man/spiketrace-view.1 $(DESTDIR)$(MANDIR)/man1/spiketrace-view.1

install-license:
	install -Dm644 LICENSE $(DESTDIR)$(LICENSEDIR)/LICENSE
	install -Dm644 NOTICE $(DESTDIR)$(LICENSEDIR)/NOTICE

install-systemd:

gen-service:
	@mkdir -p $(BUILD_DIR)
	# Generate service file with all paths templated - run for ALL builds
	sed -e 's|@BINDIR@|$(BINDIR)|g' \
	    -e 's|@STATEDIR@|$(STATEDIR)|g' \
	    -e 's|@SYSCONFDIR@|$(SYSCONFDIR)|g' \
	    spiketrace.service.in > $(BUILD_DIR)/spiketrace.service
	# Generate tmpfiles.d config from template
	sed 's|@STATEDIR@|$(STATEDIR)|g' spiketrace.tmpfiles.in > $(BUILD_DIR)/spiketrace.conf

install-systemd:
ifeq ($(PREFIX),/usr)
	install -Dm644 $(BUILD_DIR)/spiketrace.service $(DESTDIR)$(SYSTEMDDIR)/system/spiketrace.service
	install -Dm644 $(BUILD_DIR)/spiketrace.conf $(DESTDIR)$(TMPFILESDIR)/spiketrace.conf
	@echo "Installed systemd service and tmpfiles.d config"
else
	@echo "Skipping systemd auto-install (PREFIX=$(PREFIX) != /usr)"
	@echo "Use the generated file: $(BUILD_DIR)/spiketrace.service"
	@echo "Manual install: sudo cp $(BUILD_DIR)/spiketrace.service /etc/systemd/system/"
endif

# -----------------------------------------------------------------------------
# Uninstall Targets
# -----------------------------------------------------------------------------

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/spiketrace
	rm -f $(DESTDIR)$(BINDIR)/spiketrace-view-cli
	rm -f $(DESTDIR)$(BINDIR)/spiketrace-view
	rm -f $(DESTDIR)$(MANDIR)/man1/spiketrace.1
	rm -f $(DESTDIR)$(MANDIR)/man1/spiketrace-view.1
	rm -rf $(DESTDIR)$(LICENSEDIR)
	rm -rf $(DESTDIR)$(DOCDIR)
ifeq ($(PREFIX),/usr)
	rm -f $(DESTDIR)$(SYSTEMDDIR)/system/spiketrace.service
	rm -f $(DESTDIR)$(TMPFILESDIR)/spiketrace.conf
endif
	# NOTE: Config and state directory are NOT removed (user data)
	@echo "Config preserved at: $(DESTDIR)$(SYSCONFDIR)/spiketrace/"
	@echo "State preserved at: $(STATEDIR)/"
	@echo "To purge all data: rm -rf $(DESTDIR)$(SYSCONFDIR)/spiketrace $(STATEDIR)"

.PHONY: all run clean install uninstall tui vendor
.PHONY: install-bin install-config install-man install-license install-systemd
