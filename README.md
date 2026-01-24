# spiketrace

a lightweight background daemon that acts as a **black box recorder** for short-lived cpu, ram, and swap spikes.

## the problem

your laptop fan suddenly spins up loudly for 2–3 seconds, then settles back down. by the time you open `htop` or `btop`, everything looks normal. you have no idea what caused it.

spiketrace solves this by continuously monitoring system state in the background and automatically capturing a snapshot when a spike is detected — including the 10 seconds of history *before* the spike occurred.

## what spiketrace is not

- **not a real-time monitor** — use `htop`/`btop` for that
- **not a profiler** — does not trace syscalls or stack frames
- **not a replacement for top** — it's a recorder, not a viewer
- **not for sustained high load** — designed for transient spikes only

## how it works

1. **passive sampling**: daemon reads `/proc/stat`, `/proc/meminfo`, and `/proc/[pid]` every 1 second
2. **ring buffer**: stores last 60 samples (1 minute of history)
3. **anomaly detection**: triggers on:
   - cpu spike (process jumps 10%+ from baseline)
   - new process spike (spawns at 5%+ cpu)
   - memory drop (available ram drops 512+ mib suddenly)
   - memory pressure (90%+ ram used)
   - swap spike (swap usage jumps 256+ mib)
4. **json persistence**: on trigger, extracts 10 recent snapshots and writes atomically to disk
5. **cli inspection**: `spiketrace-view` displays dumps in human-readable format

## build & install

```bash
# build
make

# install (requires root)
sudo make install
```

this installs:
- `/usr/local/bin/spiketrace` — the daemon
- `/usr/local/bin/spiketrace-view` — the cli viewer
- `/etc/systemd/system/spiketrace.service` — systemd unit (opt-in)
- `/var/lib/spiketrace/` — output directory for json dumps

## running spiketrace

### systemd (recommended)

the service is **opt-in** — it is installed but not enabled by default.

```bash
# enable and start
sudo systemctl enable --now spiketrace

# check status
sudo systemctl status spiketrace

# view logs
journalctl -u spiketrace -f

# stop
sudo systemctl stop spiketrace
```

### manual execution

```bash
sudo ./build/spiketrace
```

the daemon logs to stderr. press ctrl+c to stop.

## viewing spike data

json dumps are written to `/var/lib/spiketrace/` with filenames like:
```
spike_<timestamp>_<counter>.json
```

### using spiketrace-view

```bash
spiketrace-view /var/lib/spiketrace/spike_*.json
```

example output:
```
spike dump: spike_123456789_0.json
timestamp (monotonic): 123456789 ns

=== spike trigger ===
type: cpu_delta
process: [12345] firefox
cpu: 85.2% (baseline: 5.0%, delta: +80.2%)

=== top processes by cpu ===
 1. [12345] firefox           85.2%  (rss: 2048 mib)
 2. [ 1001] xorg              12.0%  (rss: 512 mib)

=== top processes by rss ===
 1. [12345] firefox           2048 mib  (cpu: 85.2%)
 2. [ 9876] chromium          1536 mib  (cpu: 3.1%)
```

### what's recorded

each dump contains:
- **trigger**: type, process pid/name, metrics
- **snapshots** (10 most recent):
  - timestamp
  - global + per-core cpu usage
  - memory stats (total, available, swap)
  - top 10 processes by cpu
  - top 10 processes by rss

## default thresholds

| threshold | default | description |
|-----------|---------|-------------|
| cpu delta | 10% | process cpu jump from baseline |
| new process | 5% | new process initial cpu |
| memory drop | 512 mib | sudden available ram drop |
| memory pressure | 90% | ram used triggers alert |
| swap spike | 256 mib | sudden swap usage increase |
| cooldown | 5 sec | per-trigger cooldown |

thresholds are compile-time constants. edit `include/anomaly_detector.h` to change.

## limitations

- **1-second polling granularity**: very short spikes (<1 second) may be partially missed
- **attribution, not causation**: shows *which* process spiked, not *why*
- **no network/disk i/o**: only monitors cpu, ram, and swap
- **top 10 only**: snapshot includes top 10 processes by cpu and by rss

## security notes

“is this spying on my processes?”

spiketrace runs as root in order to read system-wide /proc data.

- it does not execute external commands.
- it does not open network sockets.
- it writes json snapshots locally only.
- no data leaves the system unless the user explicitly copies it.

root access is required to read kernel virtual memory at `/proc` across all processes.

## requirements

- linux (uses `/proc` filesystem)
- gcc with c99 support
- pthreads
