# spiketrace

Lightweight C daemon for Linux capturing snapshots of short resource spikes for performance troubleshooting and security anomaly detection.

## Use Cases

### Performance Troubleshooting
- Capture what caused that mysterious 2-second fan spin-up
- Identify runaway processes before they disappear
- Debug intermittent slowdowns with historical context

### Security & Threat Detection
- **Crypto miner detection**: Catch mining malware that spikes CPU then hides
- **Suspicious process spawns**: Alert on new processes consuming unusual resources
- **Memory anomalies**: Detect payload injection or memory-hungry backdoors
- **Compromised server forensics**: Review system state *before* an incident

### System Administration
- Post-mortem analysis of transient issues
- Baseline deviation monitoring
- Resource abuse detection in shared environments

## How It Works

```
┌────────────────┐    ┌────────────────┐    ┌────────────────┐    ┌────────────────┐
│    /proc/*     │──▶│  Ring Buffer   │──▶│    Anomaly     │──▶│   JSON Dump    │
│    sampling    │    │  (60 samples)  │    │   Detection    │    │   + Context    │
└────────────────┘    └────────────────┘    └────────────────┘    └────────────────┘
    1s interval         1 min history           triggers             atomic write
```

1. **Passive Sampling**: Reads `/proc/stat`, `/proc/meminfo`, `/proc/[pid]` every 1 second
2. **Ring Buffer**: Stores last 60 samples (1 minute of history)
3. **Anomaly Detection**: Triggers on CPU spikes, memory drops, swap usage, new process spawns
4. **JSON Persistence**: Atomically writes 10 recent snapshots on trigger
5. **Viewers**: Interactive TUI (Go) and legacy CLI (C) for inspecting dumps

## Requirements

- Linux (uses `/proc` filesystem)
- GCC with C99 support + pthreads
- Go 1.21+ (for the TUI viewer)

## Build & Install

```bash
make                    # build all components (daemon, TUI, CLI)
sudo make install       # install to /usr/local/bin
```

Installs:
- `/usr/local/bin/spiketrace` — daemon
- `/usr/local/bin/spiketrace-view` — interactive TUI viewer (default)
- `/usr/local/bin/spiketrace-view-cli` — legacy CLI viewer
- `/etc/systemd/system/spiketrace.service` — systemd unit (opt-in)
- `/var/lib/spiketrace/` — output directory for JSON dumps

## Running

### Systemd (Recommended)

```bash
sudo systemctl enable --now spiketrace   # enable and start
sudo systemctl status spiketrace         # check status
journalctl -u spiketrace -f              # view logs
```

### Manual

```bash
sudo ./build/spiketrace                  # logs to stderr, Ctrl+C to stop
```

## Interactive Analysis (TUI)

The `spiketrace-view` tool provides a terminal interface for analyzing dumps. It automatically discovers JSON files in your data directory.

```bash
spiketrace-view
```

### UI Layout
The viewer is divided into 3 logical panels:
1.  **Explorer (Left)**: List of all captured dumps, sorted by time.
2.  **Trigger (Top Right)**: Details of the anomaly that triggered the snapshot (e.g., "CPU Delta > 10%").
3.  **Details (Bottom Right)**: The heart of the analysis. Shows the process list for the selected snapshot.

### Features
-   **Auto-Discovery**: Automatically finds and lists `spike_*.json` files.
-   **Persistence Tracking**: The `HIST` column uses dots `[●○○●]` to visualize if a process existed in previous snapshots.
    -   `●` (Filled): Process was present in that snapshot.
    -   `○` (Empty): Process was missing (started later or terminated).
-   **Time Travel**: Navigate through the 60-snapshot history (1 minute) used to detect the spike.

### Keybindings

| Key | Action |
| :--- | :--- |
| `Tab` | Switch focus between panels (Explorer <-> Details) |
| `j` / `↓` | Move cursor down |
| `k` / `↑` | Move cursor up |
| `Enter` | Load selected dump file |
| `]` / `n` | Next snapshot (Time travel forward) |
| `[` / `p` | Previous snapshot (Time travel backward) |
| `q` | Quit |

### Legacy CLI Output
Dump summary to stdout (useful for piping to grep/jq):

```bash
spiketrace-view-cli /var/lib/spiketrace/spike_*.json
```

### What's Recorded
Each dump contains:
- **Trigger**: Anomaly type, causing process PID/name, metrics
- **Snapshots** (10 most recent):
    - CPU: global + per-core usage
    - Memory: total, available, active, inactive, dirty, slab, swap
    - Top 10 processes by CPU and by RSS

## Configuration

By default, `spiketrace` uses safe compiled-in parameters.

- **Config Path**: `~/.config/spiketrace/config.toml`
- **Template**: `examples/config.toml`
- **Reload Command**: `sudo pkill -HUP spiketrace` (zero downtime)

### Configuration Reference

Below is a complete reference of every configuration key available in `config.toml`, including default values and descriptions.

#### Anomaly Detection
| Key                          | Default | Description                                                                              |
|------------------------------|---------|------------------------------------------------------------------------------------------|
| `cpu_delta_threshold_pct`    | `10.0`  | Trigger if a process's CPU usage increases by this percentage points above its baseline. |
| `new_process_threshold_pct`  | `5.0`   | Trigger if a *new* process immediately consumes this much CPU.                           |
| `mem_drop_threshold_mib`     | `512`   | Trigger if available RAM suddenly drops by this amount (in MiB).                         |
| `mem_pressure_threshold_pct` | `90.0`  | Trigger if total system memory usage exceeds this percentage.                            |
| `swap_spike_threshold_mib`   | `256`   | Trigger if swap usage increases by this amount (in MiB).                                 |
| `cooldown_seconds`           | `5.0`   | Minimum time to wait before triggering again for the same anomaly/process.               |

#### Trigger Policy (New)
| Key     | Default       | Options                                               | Description |
|---------|---------------|-------------------------------------------------------|-------------|
| `scope` | `per_process` | `per_process`, `process_group`, `parent`, `system`    | Defines how triggers are grouped. `per_process` = default (PID-based). `process_group` = shared cooldown for all processes in PGID. `parent` = shared cooldown for children of same PPID. `system` = one global trigger cooldown. |

#### Sampling Engine
| Key                          | Default | Description                                                                  |
|------------------------------|---------|------------------------------------------------------------------------------|
| `sampling_interval_seconds`  | `1.0`   | Time between checks. Lower values increase CPU usage but catch shorter spikes.|
| `ring_buffer_capacity`       | `60`    | Number of past snapshots to keep in memory (history length).                 |
| `context_snapshots_per_dump` | `10`    | How many historical snapshots to include in the JSON dump when a trigger fires.|

#### Process Tracking
| Key                     | Default | Description                                                               |
|-------------------------|---------|---------------------------------------------------------------------------|
| `max_processes_tracked` | `512`   | Maximum number of processes to track baselines for (saves daemon memory). |
| `top_processes_stored`  | `10`    | Number of "Top CPU" and "Top RSS" processes saved inside *each* snapshot. |

#### Features (Toggles)
| Key                       | Default | Description                                  |
|---------------------------|---------|----------------------------------------------|
| `enable_cpu_detection`    | `true`  | Enable/Disable CPU spike triggers.           |
| `enable_memory_detection` | `true`  | Enable/Disable Memory drop/pressure triggers.|
| `enable_swap_detection`   | `true`  | Enable/Disable Swap usage triggers.          |

#### Output
| Key                | Default               | Description                                |
|--------------------|-----------------------|--------------------------------------------|
| `output_directory` | `/var/lib/spiketrace` | Directory where JSON dump files are written.|

#### Advanced Tuning
| Key                      | Default | Description                                                                |
|--------------------------|---------|----------------------------------------------------------------------------|
| `memory_baseline_alpha`  | `0.2`   | EMA weight for memory baseline (0.00-1.00). Higher = baseline adapts faster.|
| `process_baseline_alpha` | `0.3`   | EMA weight for process CPU baseline (0.00-1.00).                           |

## Limitations

- **1-second granularity**: Very short spikes (<1s) may be partially missed
- **Attribution, not causation**: Shows *which* process spiked, not *why*
- **CPU/RAM/Swap only**: No network or disk I/O monitoring
- **Top 10 processes**: Per snapshot, by CPU and by RSS

## Security

spiketrace runs as root to read system-wide `/proc` data.

- Does not execute external commands
- Does not open network sockets
- Writes JSON locally only
- No data leaves the system

Root access is required to read kernel virtual memory at `/proc` across all processes.

## License

This project is licensed under the GPL-2.0 License - see the [LICENSE](LICENSE) file for details.
