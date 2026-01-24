CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
PREFIX = /usr/local
LDFLAGS = -lpthread

# Daemon sources
DAEMON_SRC = \
	src/anomaly_detector.c \
	src/cpu.c \
	src/json_writer.c \
	src/mem.c \
	src/proc.c \
	src/ringbuf.c \
	src/snapshot_builder.c \
	src/spike_dump.c \
	src/spktrace.c
DAEMON_OUT = build/spiketrace

# Viewer sources
VIEWER_SRC = \
	src/json_reader.c \
	src/spktrace_view.c
VIEWER_OUT = build/spiketrace-view

all: $(DAEMON_OUT) $(VIEWER_OUT)

$(DAEMON_OUT): $(DAEMON_SRC)
	$(CC) $(CFLAGS) $(DAEMON_SRC) -o $(DAEMON_OUT) $(LDFLAGS)

$(VIEWER_OUT): $(VIEWER_SRC)
	$(CC) $(CFLAGS) $(VIEWER_SRC) -o $(VIEWER_OUT)

run: $(DAEMON_OUT)
	$(DAEMON_OUT)

clean:
	rm -f $(DAEMON_OUT) $(VIEWER_OUT)

install: $(DAEMON_OUT) $(VIEWER_OUT)
	install -D $(DAEMON_OUT) $(DESTDIR)$(PREFIX)/bin/spiketrace
	install -D $(VIEWER_OUT) $(DESTDIR)$(PREFIX)/bin/spiketrace-view
	install -D spiketrace.service /etc/systemd/system/spiketrace.service
	mkdir -p /var/lib/spiketrace
	systemctl daemon-reload

uninstall:
	systemctl stop spiketrace || true
	systemctl disable spiketrace || true
	rm -f $(DESTDIR)$(PREFIX)/bin/spiketrace
	rm -f $(DESTDIR)$(PREFIX)/bin/spiketrace-view
	rm -f /etc/systemd/system/spiketrace.service
	systemctl daemon-reload
