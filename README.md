<div align="center">

# spiketrace
<p >
  <img src="https://img.shields.io/github/commit-activity/m/ijuttt/spiketrace?style=for-the-badge" />
  <img src="https://img.shields.io/github/last-commit/ijuttt/spiketrace?style=for-the-badge" />
  <img src="https://img.shields.io/github/license/ijuttt/spiketrace?style=for-the-badge" />
  <img src="https://img.shields.io/github/stars/ijuttt/spiketrace?style=for-the-badge" />
</p>

Lightweight C daemon for Linux capturing snapshots of short resource spikes for performance troubleshooting and security anomaly detection.

https://github.com/user-attachments/assets/2f1f8996-c6a8-4c83-a112-04828dbbdb95

<br>

<a href="#why-spiketrace">Why</a> |
<a href="#how-it-works">How It Works</a> |
<a href="#installation">Installation</a> |
<a href="#running-the-service">Running</a> |
<a href="#notifications--integrations">Integrations</a> |
<a href="#interactive-analysis-tui">TUI</a> |
<a href="#configuration">Configuration</a> |
<a href="#file-locations">Locations</a> |
<a href="#uninstall">Uninstall</a> |
<a href="#limitations">Limitations</a>

</div>

---

## Why spiketrace?

Modern Linux systems are noisy and ephemeral. Most monitoring stacks are too heavy to run at high resolution continuously and still miss **short-lived spikes**. `spiketrace` records them efficiently, persisting data only when thresholds trigger.

### The Approach
- **Minimalist & Fast**: A small C daemon with a predictable footprint and no database or agent cluster required.
- **Gap-Filler**: Catches ephemeral behavior (like malware or runaway scripts) that disappears before a traditional agent can register it.
- **Context-Aware**: Always includes historical snapshots from *before* the trigger fired to explain the "how it started."
- **Operationally Simple**: No complex stack or SaaS; just a JSON dumps and a TUI viewer.

**Unlike [atop](https://github.com/Atoptool/atop) or [htop](https://github.com/htop-dev/htop)**: While those tools are excellent for live monitoring or continuous logging, `spiketrace` is purpose-built to capture forensic-level detail of the brief, rare moments that traditional tools average out or miss entirely.

### Key Use Cases
- **Performance**: Capture that mysterious fan spin-up or intermittent slowdown.
- **Security**: Identify suspicious short-lived processes or sudden memory anomalies.
- **Forensics**: Review system state exactly as it was *seconds before* an incident.

> **Caught a spike you can't explain or reproduce? spiketrace is built for exactly that.**


<details>
<summary><b>How It Works</b></summary>

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

</details>

## Installation
**Build Requirements**: GCC, Go 1.21+, and Make.

```bash
git clone https://github.com/ijuttt/spiketrace.git
cd spiketrace

make                    # build all components
sudo make install       # install to /usr/local

# IMPORTANT: For /usr/local installs, you must copy the service file manually:
sudo cp build/spiketrace.service /etc/systemd/system/
```

## Running the Service

### Systemd (Recommended)
```bash
# Start the service and enable it on boot
sudo systemctl enable --now spiketrace

# Watch logs (where spikes and triggers are reported)
journalctl -u spiketrace -f
```

### Manual Execution
```bash
sudo spiketrace
```

###  Non-Root Access (Important)
The daemon runs as `root` to read system usage, but dump files are group-owned by `spiketrace`. To allow a regular user to read dumps and use the viewer:

```bash
sudo usermod -aG spiketrace $USER
# You must log out and log back in for this to take effect
```


<details>
<summary><b>Notifications & Integrations</b></summary>

### Notifications & Integrations

`spiketrace` is designed to be integration-friendly. Since it writes standard JSON files to `/var/lib/spiketrace`, you can easily hook it into your own notification system.

**Ideas for integrations:**
- Use **Systemd Path Units** (recommended) to trigger a script whenever a new dump is created.
- Watch filenames for specific triggers (e.g., `spike_cpu_*.json`) to send alerts to Telegram/Discord.
- Sync the dump directory to a central server for cross-system forensics.

</details>

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

**System Config**: `/etc/spiketrace/config.toml` (Edit this file to customize behavior)  
**Default Reference**: `[examples/config.toml](examples/config.toml)`

> **Note**: Tune these detection thresholds to match your system's baseline and avoid false alarms according to your needs.

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


<details>
<summary><b>Limitations</b></summary>

## Limitations

- **Sampling Trade-off**: High-resolution sampling (sub-second) is supported via configuration but will increase daemon CPU usage.
- **Attribution, not causation**: Identifies the spiking process, but does not provide internal stack traces or application-level profiling.
- **Metrics Scope**: Currently limited to CPU, RAM, and Swap. (Network and Disk I/O monitoring are on the development roadmap).
- **Snapshot Depth**: Each snapshot stores the "Top N" resource-consuming processes to keep dump files small and efficient.

</details>

<details>
<summary><b>License</b></summary>

This project is licensed under the GPL-2.0 License - see the [LICENSE](LICENSE) file for details.

</details>
