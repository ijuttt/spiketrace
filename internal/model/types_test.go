/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 ijuttt */

package model

import (
	"encoding/json"
	"testing"
)

func TestTriggerPolicyDeserialization(t *testing.T) {
	tests := []struct {
		name       string
		input      string
		wantScope  string
		wantKey    int32
		wantDesc   string
		wantOrigin int
	}{
		{
			name: "full v4 trigger with policy",
			input: `{
				"type": "cpu_spike",
				"pid": 1234,
				"comm": "stress",
				"cpu_pct": 95.0,
				"baseline_pct": 10.0,
				"delta_pct": 85.0,
				"is_new_process": false,
				"mem_available_kib": 1024,
				"mem_baseline_kib": 2048,
				"mem_delta_kib": -1024,
				"mem_used_pct": 80.0,
				"swap_used_kib": 0,
				"swap_baseline_kib": 0,
				"swap_delta_kib": 0,
				"policy": {
					"scope": "per_process",
					"scope_key": 1234,
					"description": "Cooldown per PID 1234"
				},
				"origin_snapshot_index": 3
			}`,
			wantScope:  "per_process",
			wantKey:    1234,
			wantDesc:   "Cooldown per PID 1234",
			wantOrigin: 3,
		},
		{
			name: "legacy trigger without policy (backward compat)",
			input: `{
				"type": "mem_pressure",
				"pid": 5678,
				"comm": "java",
				"cpu_pct": 10.0,
				"baseline_pct": 5.0,
				"delta_pct": 5.0,
				"is_new_process": false,
				"mem_available_kib": 512,
				"mem_baseline_kib": 4096,
				"mem_delta_kib": -3584,
				"mem_used_pct": 95.0,
				"swap_used_kib": 0,
				"swap_baseline_kib": 0,
				"swap_delta_kib": 0
			}`,
			wantScope:  "",
			wantKey:    0,
			wantDesc:   "",
			wantOrigin: 0,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var trigger Trigger
			if err := json.Unmarshal([]byte(tt.input), &trigger); err != nil {
				t.Fatalf("unmarshal failed: %v", err)
			}

			if trigger.Policy.Scope != tt.wantScope {
				t.Errorf("Policy.Scope = %q, want %q", trigger.Policy.Scope, tt.wantScope)
			}
			if trigger.Policy.ScopeKey != tt.wantKey {
				t.Errorf("Policy.ScopeKey = %d, want %d", trigger.Policy.ScopeKey, tt.wantKey)
			}
			if trigger.Policy.Description != tt.wantDesc {
				t.Errorf("Policy.Description = %q, want %q", trigger.Policy.Description, tt.wantDesc)
			}
			if trigger.OriginSnapshotIndex != tt.wantOrigin {
				t.Errorf("OriginSnapshotIndex = %d, want %d", trigger.OriginSnapshotIndex, tt.wantOrigin)
			}
		})
	}
}

func TestProcessEntryFullDeserialization(t *testing.T) {
	input := `{
		"pid": 42,
		"ppid": 1,
		"uid": 1000,
		"state": "D",
		"comm": "rsync",
		"cmdline": "/usr/bin/rsync -avz /data /backup",
		"cpu_pct": 2.5,
		"rss_kib": 51200
	}`

	var proc ProcessEntry
	if err := json.Unmarshal([]byte(input), &proc); err != nil {
		t.Fatalf("unmarshal failed: %v", err)
	}

	if proc.PID != 42 {
		t.Errorf("PID = %d, want 42", proc.PID)
	}
	if proc.PPID != 1 {
		t.Errorf("PPID = %d, want 1", proc.PPID)
	}
	if proc.UID != 1000 {
		t.Errorf("UID = %d, want 1000", proc.UID)
	}
	if proc.State != "D" {
		t.Errorf("State = %q, want \"D\"", proc.State)
	}
	if proc.Cmdline != "/usr/bin/rsync -avz /data /backup" {
		t.Errorf("Cmdline = %q, want full cmdline", proc.Cmdline)
	}
}
