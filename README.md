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

## Installation

### Arch Linux (AUR)
A generic PKGBUILD is provided. If using an AUR helper:

```bash
yay -S spiketrace-git
```

### Manual Build
**Requirements**: GCC (C99+), pthreads, Go 1.21+ (for TUI), make.

```bash
git clone https://github.com/ijuttt/spiketrace.git
cd spiketrace

make                    # build all components
sudo make install       # install to /usr/local
```

For a distro-compliant install (installing to `/usr` and including systemd units):
```bash
sudo make PREFIX=/usr install
```

## Running the Service

### Systemd (Recommended)
```bash
sudo systemctl enable --now spiketrace   # enable and start
sudo systemctl status spiketrace         # check status
journalctl -u spiketrace -f              # view logs
```

To reload configuration without restarting:
```bash
sudo systemctl reload spiketrace
```

### Manual Execution
```bash
sudo spiketrace
```

### ⚠️ Non-Root Access (Important)
The daemon runs as `root` to read system usage, but dump files are group-owned by `spiketrace`. To allow a regular user to read dumps and use the viewer:

```bash
sudo usermod -aG spiketrace $USER
# You must log out and log back in for this to take effect
```

## Interactive Analysis (TUI)

The `spiketrace-view` tool automatically discovers JSON files in `/var/lib/spiketrace`.

```bash
spiketrace-view
```

### UI Navigation
| Key | Action |
| :--- | :--- |
| `Tab` | Switch focus between panels (Explorer <-> Details) |
| `j` / `↓` | Move cursor down |
| `k` / `↑` | Move cursor up |
| `Enter` | Load selected dump file |
| `]` / `n` | Next snapshot (Time travel forward) |
| `[` / `p` | Previous snapshot (Time travel backward) |
| `q` | Quit |

**Panels:**
1.  **Explorer (Left)**: List of all captured dumps.
2.  **Trigger (Top Right)**: Details of what triggered the anomaly.
3.  **Details (Bottom Right)**: Process list and metrics for the selected snapshot.

### Legacy CLI Output
Dump summary to stdout (useful for scripting):
```bash
spiketrace-view-cli /var/lib/spiketrace/spike_*.json
```

## Configuration

**Location**: `/etc/spiketrace/config.toml`  
**Example**: `/usr/share/doc/spiketrace/config.toml.example`

<details>
<summary><strong>Click to view full configuration reference</strong></summary>

### Anomaly Detection
| Key                          | Default | Description                                                                              |
|------------------------------|---------|------------------------------------------------------------------------------------------|
| `cpu_delta_threshold_pct`    | `10.0`  | Trigger if a process's CPU usage increases by this percentage points above its baseline. |
| `new_process_threshold_pct`  | `5.0`   | Trigger if a *new* process immediately consumes this much CPU.                           |
| `mem_drop_threshold_mib`     | `512`   | Trigger if available RAM suddenly drops by this amount (in MiB).                         |
| `mem_pressure_threshold_pct` | `90.0`  | Trigger if total system memory usage exceeds this percentage.                            |
| `swap_spike_threshold_mib`   | `256`   | Trigger if swap usage increases by this amount (in MiB).                                 |
| `cooldown_seconds`           | `5.0`   | Minimum time to wait before triggering again for the same anomaly/process.               |

### Sampling Engine
| Key                          | Default | Description                                                                  |
|------------------------------|---------|------------------------------------------------------------------------------|
| `sampling_interval_seconds`  | `1.0`   | Time between checks. Lower values increase CPU usage but catch shorter spikes.|
| `ring_buffer_capacity`       | `60`    | Number of past snapshots to keep in memory (history length).                 |
| `context_snapshots_per_dump` | `10`    | How many historical snapshots to include in the JSON dump when a trigger fires.|

### Process Tracking
| Key                     | Default | Description                                                               |
|-------------------------|---------|---------------------------------------------------------------------------|
| `max_processes_tracked` | `512`   | Maximum number of processes to track baselines for (saves daemon memory). |
| `top_processes_stored`  | `10`    | Number of "Top CPU" and "Top RSS" processes saved inside *each* snapshot. |

### Features (Toggles)
| Key                       | Default | Description                                  |
|---------------------------|---------|----------------------------------------------|
| `enable_cpu_detection`    | `true`  | Enable/Disable CPU spike triggers.           |
| `enable_memory_detection` | `true`  | Enable/Disable Memory drop/pressure triggers.|
| `enable_swap_detection`   | `true`  | Enable/Disable Swap usage triggers.          |

### Advanced Tuning
| Key                      | Default | Description                                                                |
|--------------------------|---------|----------------------------------------------------------------------------|
| `memory_baseline_alpha`  | `0.2`   | EMA weight for memory baseline (0.00-1.00). Higher = baseline adapts faster.|
| `process_baseline_alpha` | `0.3`   | EMA weight for process CPU baseline (0.00-1.00).                           |

</details>

## File Locations

| Path | Description |
|------|-------------|
| `/etc/spiketrace/config.toml` | System-wide configuration |
| `/var/lib/spiketrace/` | Spike dump output directory (JSON files) |
| `/usr/lib/systemd/system/spiketrace.service` | Systemd service unit |
| `/usr/share/man/man1/spiketrace.1` | Manual page |

## Uninstall

### Manual Uninstall
```bash
sudo make uninstall
```
*Note: This preserves user data. To fully purge:*
```bash
sudo rm -rf /etc/spiketrace /var/lib/spiketrace
sudo groupdel spiketrace
```



## Limitations

- **1-second granularity**: Very short spikes (<1s) may be partially missed
- **Attribution, not causation**: Shows *which* process spiked, not *why*
- **CPU/RAM/Swap only**: No network or disk I/O monitoring
- **Top 10 processes**: Per snapshot, by CPU and by RSS

## License
This project is licensed under the GPL-2.0 License - see the [LICENSE](LICENSE) file for details.
