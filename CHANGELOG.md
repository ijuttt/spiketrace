## [0.1.1] - 2026-02-13

### 💼 Other

- *(go)* Track vendor directory and add PKGBUILD prepare step

### 📚 Documentation

- Update .SRCINFO for prepare step
## [0.1.0] - 2026-02-13

### 🚀 Features

- *(cpu)* Add per-core CPU jiffies reader
- *(cpu)* Add per-core CPU usage calculation
- *(mem)* Add read memory function
- *(cpu)* Add more jiffies fields and cleanup parsing
- *(common)* Add ringbuffer error codes
- *(core)* Add snapshot data structures
- *(ringbuf)* Add snapshot ring buffer interface
- *(ringbuf)* Add base includes and monotonic timestamp helper
- *(ringbuf)* Implement ring buffer lifecycle
- *(ringbuf)* Add snapshot push with overwrite handling
- *(ringbuf)* Add snapshot read APIs
- *(ringbuf)* Add buffer state helpers and clear operation
- *(snapshot)* Add process command name to snapshot entries
- *(common)* Define process collector error codes
- *(cpu)* Expose total_jiffies helper
- *(proc)* Implement process snapshot collector
- *(snapshot)* Add snapshot builder module
- *(proc)* Add CPU baseline EMA and RSS-based process ranking
- *(snapshot)* Expose proc samples for anomaly detection
- *(anomaly)* Add CPU, memory, and swap spike detection engine
- *(json)* Add lightweight JSON reader and writer utilities
- *(dump)* Persist spike context snapshots atomically
- *(daemon)* Wire anomaly detection and spike dumping into main loop
- *(cli)* Add spiketrace-view for inspecting minimal spike dumps
- *(build)* Add Makefile and systemd service unit
- *(ui)* Add TUI viewer and core refactor
- *(core)* Implement Trigger Policy Layer
- *(core)* Add process aggregation and system config path
- *(core)* Implement spike origin detection
- *(cli)* Add --help and --version arguments
- *(viewer)* Extend memory info with schema v2 fields
- *(core)* Implement automatic and manual log deletion logic
- *(ui)* Implement log deletion and confirmation dialog
- Implement self-healing directory creation
- *(core)* Implement schema v5 process metrics and iowait
- *(ui)* Display iowait and improve process list truncation
- *(ui)* Implement draggable panel resizing
- *(core)* Implement I/O wait metrics and trigger policy
- *(ui)* Refresh theme with neon/cyberpunk palette
- *(ui)* Enhance process detail view overlay
- *(ui)* Visualize I/O wait metrics
- *(build)* Centralized version management

### 🐛 Bug Fixes

- *(cpu)* Correct per-core usage delta and harden /proc/stat parsing
- *(ringbuf)* Define _POSIX_C_SOURCE for clock_gettime compatibility
- *(proc)* Fix CPU% sorting and delta handling
- *(core)* Security hardening and reliability improvements
- *(go)* Update module path to match repository owner
- *(core)* Resolve warnings and UI bugs
- Resolve post-rebase conflicts and import errors
- Resolve post-rebase conflicts, import errors, and localization
- *(core)* Remove duplicate includes in spktrace.c
- *(log_manager)* Handle malloc failure with SPKT_ERR_OUT_OF_MEMORY
- Improve installation and systemd integration
- *(core)* Correct iteration logic in json_reader_skip
- Prevent slice bounds out of range panic in Viewer
- Resolve slice bounds panic in Viewer

### 💼 Other

- *(make)* Overhaul install targets for distro compliance
- *(arch)* Add initial PKGBUILD and install script
- Feat/auto-delete-log into dev
- *(pkg)* Refactor AUR compliance and systemd-sysusers

### 🚜 Refactor

- *(cpu)* Improve naming and error handling
- *(ringbuf)* Remove overflow warning and simplify overwrite logic
- *(core)* Enable build-time path configuration
- *(ui)* Unify titled border rendering

### 📚 Documentation

- *(cpu)* Add minimal function documentation
- *(readme)* Update readme
- *(readme)* Update installation and configuration guide
- Remove arch installation guide
- *(readme)* Revamp structure and add demo video

### 🎨 Styling

- Standardize comments to C-style /* */
- *(ui)* Simplify activity legend characters

### ⚙️ Miscellaneous Tasks

- *(license)* Add GPLv2 license
- *(license)* Add SPDX identifiers and compliance headers
- *(packaging)* Add systemd tmpfiles.d config
- Update ignore patterns
- Fix .clangd ignore pattern
- *(arch)* Generate .SRCINFO
- Fix demo video & add shields
