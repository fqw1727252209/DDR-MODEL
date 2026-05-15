"""DDR Controller Log Visualizer.

Parses large simulation log files and generates a self-contained interactive
HTML dashboard.

Usage:
    python log_visualizer.py --input-dir <path> [--output <path>] [--max-cmd-events <int>]

Arguments:
    --input-dir         Path to simulation output directory containing .log files
    --output            Output HTML file path (default: ddr_visualizer_report.html)
    --max-cmd-events    Max command events to render per source (default: 50000)
"""

import argparse
import datetime
import json
import re
import sys
from pathlib import Path


class SchedulerSnapshot:
    """Holds a scheduler snapshot at a given timestamp."""

    def __init__(self, timestamp: int, pch: dict):
        self.timestamp = timestamp
        self.pch = pch

    def to_dict(self):
        return {
            "timestamp": self.timestamp,
            "pch": self.pch,
        }


class StateTransition:
    """Holds a state transition with optional associated scheduler snapshot and global state."""

    def __init__(
        self,
        timestamp: int,
        old_state: str,
        new_state: str,
        reason: str,
        scheduler_snapshot: SchedulerSnapshot | None,
        global_state: dict | None = None,
    ):
        self.timestamp = timestamp
        self.old_state = old_state
        self.new_state = new_state
        self.reason = reason
        self.scheduler_snapshot = scheduler_snapshot
        self.global_state = global_state or {}

    def to_dict(self):
        return {
            "timestamp": self.timestamp,
            "old_state": self.old_state,
            "new_state": self.new_state,
            "reason": self.reason,
            "scheduler_snapshot": (
                self.scheduler_snapshot.to_dict() if self.scheduler_snapshot else None
            ),
            "global_state": self.global_state,
        }


def parse_mrdimm_mode_switch(log_path: Path):
    """Parse MrdimmModeSwitch.log and return (transitions, scheduler_snapshots).

    Each transition is associated with the nearest preceding scheduler snapshot
    at the same or earlier timestamp.
    """
    timestamp_re = re.compile(r"\[INFO \]\s+\[(\d+)\s+ps\]")
    pch_re = re.compile(
        r"PCH\[(\d+)\]:\s+"
        r"ValidNtt=\(RD=(\d+)\s+WR=(\d+)\)\s+"
        r"AvailCmd=\(RD=(\d+)\s+WR=(\d+)\)\s+"
        r"ValidCAM=\(RD=(\d+)\s+WR=(\d+)\)\s+"
        r"Critical=\(HPR=(\d+)\s+LPR=(\d+)\s+TPW=(\d+)\)"
    )
    transition_re = re.compile(
        r"\[MrdimmModeSwitch\]\s+\w+\s+mode:\s+hit\s+(.+?),\s+state\s+(\w+)\s+->\s+(\w+)"
    )

    # New format (after log label update in MrdimmModeSwitch.cpp)
    global_state_re = re.compile(
        r"\[MrdimmModeSwitch\]\s+UpdateGlobalState\s+start:\s+old_state=\w+,\s+"
        r"expired_rd_hit_activing=(\d+)\s+expired_wr_hit_activing=(\d+)\s+"
        r"flush_rd_hit_activing=(\d+)\s+flush_wr_hit_activing=(\d+)\s+flush_wr_hit_actived=(\d+)\s+"
        r"rd_critical_hit_activing=(\d+)\s+wr_critical_hit_actived=(\d+)\s+"
        r"both_rd_avail_high=(\d+)\s+both_wr_avail_high=(\d+)\s+"
        r"both_rd_avail_medium=(\d+)\s+both_wr_avail_medium=(\d+)\s+"
        r"has_rd_avail=(\d+)\s+has_wr_avail=(\d+)\s+"
        r"both_rd_no_valid=(\d+)\s+has_wr_valid=(\d+)\s+both_wr_no_valid=(\d+)\s+has_rd_valid=(\d+)\s+"
        r"one_pch_rd_valid=(\d+)\s+both_wr_valid=(\d+)\s+both_rd_valid=(\d+)\s+both_wr_cam_above=(\d+)"
    )

    # Old format (before log label update)
    global_state_re_old = re.compile(
        r"\[MrdimmModeSwitch\]\s+UpdateGlobalState\s+start:\s+old_state=\w+,\s+"
        r"exp_rd=(\d+)\s+exp_wr=(\d+)\s+flush_rd=(\d+)\s+flush_wr=(\d+)\s+flush_wr_act=(\d+)\s+"
        r"rd_crit=(\d+)\s+wr_crit=(\d+)\s+rd_avail_high=(\d+)\s+wr_avail_high=(\d+)\s+"
        r"rd_avail_med=(\d+)\s+wr_avail_med=(\d+)\s+"
        r"rd_avail=(\d+)\s+wr_avail=(\d+)\s+rd_no_valid=(\d+)\s+wr_valid=(\d+)\s+"
        r"wr_no_valid=(\d+)\s+rd_valid=(\d+)\s+"
        r"one_rd_valid=(\d+)\s+both_wr_valid=(\d+)\s+both_rd_valid=(\d+)\s+wr_cam_above=(\d+)"
    )

    GLOBAL_STATE_KEYS = [
        "expired_rd_hit_activing", "expired_wr_hit_activing",
        "flush_rd_hit_activing", "flush_wr_hit_activing", "flush_wr_hit_actived",
        "rd_critical_hit_activing", "wr_critical_hit_actived",
        "both_rd_avail_high", "both_wr_avail_high",
        "both_rd_avail_medium", "both_wr_avail_medium",
        "has_rd_avail", "has_wr_avail",
        "both_rd_no_valid", "has_wr_valid", "both_wr_no_valid", "has_rd_valid",
        "one_pch_rd_valid", "both_wr_valid", "both_rd_valid", "both_wr_cam_above",
    ]

    scheduler_snapshots = []
    transitions = []
    current_timestamp = None
    current_pch = {}
    in_scheduler_info = False
    current_global_state = {}

    last_snapshot_idx = -1  # for O(1) amortized lookup

    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")

            ts_match = timestamp_re.search(line)
            if ts_match:
                current_timestamp = int(ts_match.group(1))

            if "SchedulerInfo:" in line:
                in_scheduler_info = True
                current_pch = {}
                continue

            if in_scheduler_info:
                pch_match = pch_re.search(line)  # use search, not match
                if pch_match:
                    pch_idx = int(pch_match.group(1))
                    current_pch[pch_idx] = {
                        "valid_ntt_rd": int(pch_match.group(2)),
                        "valid_ntt_wr": int(pch_match.group(3)),
                        "avail_cmd_rd": int(pch_match.group(4)),
                        "avail_cmd_wr": int(pch_match.group(5)),
                        "valid_cam_rd": int(pch_match.group(6)),
                        "valid_cam_wr": int(pch_match.group(7)),
                        "critical_hpr": int(pch_match.group(8)),
                        "critical_lpr": int(pch_match.group(9)),
                        "critical_tpw": int(pch_match.group(10)),
                    }
                    continue
                else:
                    # SchedulerInfo block ended without a PCH line
                    in_scheduler_info = False
                    if current_timestamp is not None and current_pch:
                        scheduler_snapshots.append(
                            SchedulerSnapshot(current_timestamp, current_pch)
                        )
                        current_pch = {}
                    # Fall through to allow global_state/transition parsing on this line

            gs_match = global_state_re.search(line)
            if gs_match:
                current_global_state = {
                    k: int(gs_match.group(i + 1))
                    for i, k in enumerate(GLOBAL_STATE_KEYS)
                }
            else:
                gs_match_old = global_state_re_old.search(line)
                if gs_match_old:
                    current_global_state = {
                        k: int(gs_match_old.group(i + 1))
                        for i, k in enumerate(GLOBAL_STATE_KEYS)
                    }

            trans_match = transition_re.search(line)
            if trans_match and current_timestamp is not None:
                reason = trans_match.group(1)
                old_state = trans_match.group(2)
                new_state = trans_match.group(3)

                # O(1) amortized: advance pointer to latest snapshot <= current_timestamp
                while (
                    last_snapshot_idx + 1 < len(scheduler_snapshots)
                    and scheduler_snapshots[last_snapshot_idx + 1].timestamp
                    <= current_timestamp
                ):
                    last_snapshot_idx += 1

                nearest_snapshot = (
                    scheduler_snapshots[last_snapshot_idx]
                    if last_snapshot_idx >= 0
                    else None
                )

                transitions.append(
                    StateTransition(
                        timestamp=current_timestamp,
                        old_state=old_state,
                        new_state=new_state,
                        reason=reason,
                        scheduler_snapshot=nearest_snapshot,
                        global_state=current_global_state.copy(),
                    )
                )
                current_global_state = {}

    return [t.to_dict() for t in transitions], [s.to_dict() for s in scheduler_snapshots]


class CommandEvent:
    def __init__(self, timestamp, cmd, pch, source, trans_id=None, bank=None, col=None):
        self.timestamp = timestamp  # int, ps
        self.cmd = cmd  # str: ACT, RDA, RD, WR, WRA, REFAB, etc.
        self.pch = pch  # int: 0 or 1
        self.source = source  # str: 'dfi_cmd', 'mc_0', or 'mc_1'
        self.trans_id = trans_id  # int or None
        self.bank = bank or {}  # dict with keys like bankgroup, bank, real_ba, real_bg
        self.col = col  # int or None

    def to_dict(self):
        return {
            "timestamp": self.timestamp,
            "cmd": self.cmd,
            "pch": self.pch,
            "source": self.source,
            "trans_id": self.trans_id,
            "bank": self.bank,
            "col": self.col,
        }


def extract_bank_dict(line):
    """Extract bank fields from a BankAddress block in a line."""
    bank = {}
    bg_match = re.search(r"bankgroup:\s*(\d+)", line)
    if bg_match:
        bank["bankgroup"] = int(bg_match.group(1))
    b_match = re.search(r"\bbank:\s*(\d+)", line)
    if b_match:
        bank["bank"] = int(b_match.group(1))
    rba_match = re.search(r"real_ba:\s*(\d+)", line)
    if rba_match:
        bank["real_ba"] = int(rba_match.group(1))
    rbg_match = re.search(r"real_bg:\s*(\d+)", line)
    if rbg_match:
        bank["real_bg"] = int(rbg_match.group(1))
    return bank


def parse_dfi_cmd_log(log_path):
    """Parse phy_delay_model_DfiCmd.log and return a list of CommandEvent."""
    timestamp_re = re.compile(r"\[INFO \]\s+\[(\d+)\s+ps\]")
    trans_id_re = re.compile(r"Trans Id:\s*(-?\d+)")
    cmd_re = re.compile(r"Cmd:\s*(\w+)")
    pch_re = re.compile(r"pse_ch:\s*(\d+)")
    col_re = re.compile(r"Col:\s*(\d+)")

    events = []
    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")

            ts_match = timestamp_re.search(line)
            if not ts_match:
                continue
            timestamp = int(ts_match.group(1))

            trans_match = trans_id_re.search(line)
            trans_id = int(trans_match.group(1)) if trans_match else None

            cmd_match = cmd_re.search(line)
            cmd = cmd_match.group(1) if cmd_match else None

            pch_match = pch_re.search(line)
            pch = int(pch_match.group(1)) if pch_match else None

            bank = extract_bank_dict(line)

            col_match = col_re.search(line)
            col = int(col_match.group(1)) if col_match else None

            events.append(
                CommandEvent(
                    timestamp=timestamp,
                    cmd=cmd,
                    pch=pch,
                    source="dfi_cmd",
                    trans_id=trans_id,
                    bank=bank,
                    col=col,
                )
            )

    return [e.to_dict() for e in events]


def parse_mc_log(log_path, mc_index):
    """Parse Mc_0.log or Mc_1.log and return a list of CommandEvent."""
    timestamp_re = re.compile(r"\[INFO \]\s+\[(\d+)\s+ps\]")
    pch_re = re.compile(r"PCH\s+(\d+)")
    cmd_re = re.compile(r"cmd=(\w+)")
    col_re = re.compile(r"Col:\s*(\d+)")

    events = []
    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")

            ts_match = timestamp_re.search(line)
            if not ts_match:
                continue
            timestamp = int(ts_match.group(1))

            pch_match = pch_re.search(line)
            pch = int(pch_match.group(1)) if pch_match else None

            cmd_match = cmd_re.search(line)
            cmd = cmd_match.group(1) if cmd_match else None

            bank = extract_bank_dict(line)

            col_match = col_re.search(line)
            col = int(col_match.group(1)) if col_match else None

            events.append(
                CommandEvent(
                    timestamp=timestamp,
                    cmd=cmd,
                    pch=pch,
                    source=f"mc_{mc_index}",
                    trans_id=None,
                    bank=bank,
                    col=col,
                )
            )

    return [e.to_dict() for e in events]


def sample_command_events(events, max_events=50000):
    """Adaptive sampling: if too many events, bin by time and keep representative samples."""
    if len(events) <= max_events:
        return events

    events = sorted(events, key=lambda e: e["timestamp"])
    min_ts = events[0]["timestamp"]
    max_ts = events[-1]["timestamp"]
    duration = max_ts - min_ts
    if duration == 0:
        return events[:max_events]

    num_bins = max_events
    bin_size = duration / num_bins

    bins = {}
    for e in events:
        bin_idx = int((e["timestamp"] - min_ts) / bin_size)
        if bin_idx not in bins:
            bins[bin_idx] = []
        bins[bin_idx].append(e)

    sampled = []
    for bin_idx in sorted(bins.keys()):
        bin_events = bins[bin_idx]
        if len(bin_events) <= 3:
            sampled.extend(bin_events)
        else:
            sampled.append(bin_events[0])
            sampled.append(bin_events[len(bin_events) // 2])
            sampled.append(bin_events[-1])

    return sampled[:max_events]


def aggregate_scheduler_for_transitions(transitions, scheduler_snapshots):
    """Ensure every transition has an associated scheduler snapshot."""
    snapshots_by_ts = {s["timestamp"]: s for s in scheduler_snapshots}
    for t in transitions:
        if t["scheduler_snapshot"] is None:
            nearest_ts = None
            for ts in sorted(snapshots_by_ts.keys()):
                if ts <= t["timestamp"]:
                    nearest_ts = ts
                else:
                    break
            if nearest_ts is not None:
                t["scheduler_snapshot"] = snapshots_by_ts[nearest_ts]
    return transitions


HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>DDR Controller Log Visualizer</title>
<style>
:root {
  --bg-primary: #0f172a;
  --bg-secondary: #1e293b;
  --bg-panel: #334155;
  --text-primary: #f1f5f9;
  --text-secondary: #94a3b8;
  --rd-color: #00d4ff;
  --wr-color: #ff9f1c;
  --act-color: #2ec4b6;
  --rda-color: #00d4ff;
  --wr-color2: #ff9f1c;
  --ref-color: #9b5de5;
  --grid-line: #1e293b;
  --accent: #6366f1;
  --border-radius: 8px;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
  background: var(--bg-primary);
  color: var(--text-primary);
  overflow-x: hidden;
}
header {
  padding: 16px 24px;
  background: var(--bg-secondary);
  border-bottom: 1px solid var(--bg-panel);
  display: flex;
  justify-content: space-between;
  align-items: center;
}
header h1 { font-size: 18px; font-weight: 600; letter-spacing: 0.5px; }
header .meta { font-size: 12px; color: var(--text-secondary); }
#app {
  display: grid;
  grid-template-columns: 1fr 320px;
  grid-template-rows: auto auto auto auto;
  gap: 12px;
  padding: 16px;
  max-width: 1800px;
  margin: 0 auto;
}
.chart-container {
  background: var(--bg-secondary);
  border-radius: var(--border-radius);
  padding: 12px;
  position: relative;
}
.chart-container h2 {
  font-size: 13px;
  color: var(--text-secondary);
  margin-bottom: 8px;
  text-transform: uppercase;
  letter-spacing: 0.8px;
}
.chart-canvas {
  width: 100%;
  height: 160px;
  background: var(--bg-primary);
  border-radius: 4px;
  cursor: crosshair;
}
#detail-panel {
  grid-column: 2;
  grid-row: 1 / span 4;
  background: var(--bg-secondary);
  border-radius: var(--border-radius);
  padding: 16px;
  position: sticky;
  top: 16px;
  height: fit-content;
  max-height: 90vh;
  overflow-y: auto;
}
#detail-panel h2 {
  font-size: 13px;
  color: var(--text-secondary);
  margin-bottom: 12px;
  text-transform: uppercase;
  letter-spacing: 0.8px;
}
.detail-section {
  margin-bottom: 16px;
  padding-bottom: 12px;
  border-bottom: 1px solid var(--bg-panel);
}
.detail-section:last-child { border-bottom: none; }
.detail-section h3 {
  font-size: 12px;
  color: var(--text-secondary);
  margin-bottom: 8px;
}
.pch-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 11px;
}
.pch-table th, .pch-table td {
  text-align: left;
  padding: 4px 6px;
  border-bottom: 1px solid var(--bg-panel);
}
.pch-table th { color: var(--text-secondary); font-weight: 500; }
.metric-bar {
  height: 4px;
  background: var(--bg-panel);
  border-radius: 2px;
  margin-top: 2px;
  overflow: hidden;
}
.metric-bar-fill {
  height: 100%;
  border-radius: 2px;
  transition: width 0.2s ease;
}
.metric-bar-fill.rd { background: var(--rd-color); }
.metric-bar-fill.wr { background: var(--wr-color); }
.tooltip {
  position: absolute;
  background: rgba(15, 23, 42, 0.95);
  border: 1px solid var(--bg-panel);
  border-radius: 6px;
  padding: 8px 12px;
  font-size: 11px;
  pointer-events: none;
  z-index: 1000;
  display: none;
  box-shadow: 0 4px 12px rgba(0,0,0,0.3);
  max-width: 300px;
}
.tooltip .ts { color: var(--text-secondary); margin-bottom: 4px; }
.tooltip .reason { color: var(--accent); font-weight: 500; }
.controls {
  display: flex;
  gap: 8px;
  margin-bottom: 8px;
}
.controls button {
  background: var(--bg-panel);
  color: var(--text-primary);
  border: none;
  padding: 4px 10px;
  border-radius: 4px;
  font-size: 11px;
  cursor: pointer;
  transition: background 0.15s;
}
.controls button:hover { background: var(--accent); }
#stats-panel {
  grid-column: 1 / -1;
  background: var(--bg-secondary);
  border-radius: var(--border-radius);
  padding: 12px 16px;
}
.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
  gap: 8px;
  margin-top: 8px;
}
.stat-card {
  background: var(--bg-primary);
  padding: 8px 12px;
  border-radius: 6px;
  font-size: 11px;
}
.stat-card .label { color: var(--text-secondary); margin-bottom: 2px; }
.stat-card .value { font-size: 16px; font-weight: 600; }
@media (max-width: 1200px) {
  #app { grid-template-columns: 1fr; }
  #detail-panel { grid-column: 1; grid-row: auto; position: static; }
}
</style>
</head>
<body>
  <header>
    <div>
      <h1>DDR Controller Log Visualizer</h1>
      <div class="meta">Input: {{input_dir}} | Generated: {{generated_time}}</div>
    </div>
    <div class="meta">Transitions: {{transition_count}} | Commands: {{command_count}}</div>
  </header>
  <div id="app">
    <div class="chart-container">
      <div class="controls">
        <button onclick="zoomIn()">Zoom In</button>
        <button onclick="zoomOut()">Zoom Out</button>
        <button onclick="resetZoom()">Reset</button>
      </div>
      <h2>Read/Write State Timeline</h2>
      <canvas id="state-timeline" class="chart-canvas" height="160"></canvas>
      <div id="state-tooltip" class="tooltip"></div>
    </div>
    
    <div id="detail-panel">
      <h2>State Detail</h2>
      <div id="detail-content">
        <p style="color: var(--text-secondary); font-size: 12px;">Hover or click a state transition point to see details.</p>
      </div>
    </div>

    <div class="chart-container">
      <h2>Command Swimlanes</h2>
      <canvas id="command-swimlanes" class="chart-canvas" height="200"></canvas>
    </div>

    <div class="chart-container">
      <h2>Scheduler Metrics</h2>
      <canvas id="scheduler-metrics" class="chart-canvas" height="180"></canvas>
    </div>

    <div class="chart-container">
      <h2>Critical Status Timeline</h2>
      <canvas id="critical-status" class="chart-canvas" height="120"></canvas>
    </div>

    <div id="stats-panel">
      <h2>State Switch Statistics</h2>
      <div id="stats-content" class="stats-grid"></div>
    </div>
  </div>

  <script>
  const DATA = {{data_json}};
  // ==================== ChartEngine Base ====================
  class ChartEngine {
    constructor(canvasId, data) {
      this.canvas = document.getElementById(canvasId);
      this.ctx = this.canvas.getContext('2d');
      this.data = data;
      this.dpr = window.devicePixelRatio || 1;
      this.padding = { top: 10, right: 10, bottom: 20, left: 50 };
      this.visibleRange = { start: data.time_range.min, end: data.time_range.max };
      this.hoverCallback = null;
      this.clickCallback = null;
      this.zoomCallback = null;
      this._setupCanvas();
      this._bindEvents();
    }

    _setupCanvas() {
      const rect = this.canvas.getBoundingClientRect();
      this.canvas.width = rect.width * this.dpr;
      this.canvas.height = rect.height * this.dpr;
      this.ctx.scale(this.dpr, this.dpr);
      this.width = rect.width;
      this.height = rect.height;
    }

    timeToX(ts) {
      const range = this.visibleRange.end - this.visibleRange.start;
      if (range === 0) return this.padding.left;
      const plotWidth = this.width - this.padding.left - this.padding.right;
      return this.padding.left + ((ts - this.visibleRange.start) / range) * plotWidth;
    }

    xToTime(x) {
      const range = this.visibleRange.end - this.visibleRange.start;
      const plotWidth = this.width - this.padding.left - this.padding.right;
      const ratio = (x - this.padding.left) / plotWidth;
      return this.visibleRange.start + ratio * range;
    }

    setVisibleRange(start, end) {
      this.visibleRange.start = Math.max(this.data.time_range.min, start);
      this.visibleRange.end = Math.min(this.data.time_range.max, end);
      this.render();
    }

    zoom(factor, centerTime) {
      const range = this.visibleRange.end - this.visibleRange.start;
      const newRange = range * factor;
      const half = newRange / 2;
      this.setVisibleRange(centerTime - half, centerTime + half);
    }

    pan(deltaTs) {
      this.setVisibleRange(
        this.visibleRange.start + deltaTs,
        this.visibleRange.end + deltaTs
      );
    }

    _bindEvents() {
      let isDragging = false;
      let lastX = 0;

      this.canvas.addEventListener('wheel', (e) => {
        e.preventDefault();
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const centerTime = this.xToTime(x);
        const factor = e.deltaY < 0 ? 0.8 : 1.25;
        this.zoom(factor, centerTime);
        if (this.zoomCallback) this.zoomCallback(this.visibleRange);
      });

      this.canvas.addEventListener('mousedown', (e) => {
        isDragging = true;
        lastX = e.clientX;
        this.canvas.style.cursor = 'grabbing';
      });

      window.addEventListener('mousemove', (e) => {
        if (!isDragging) {
          const rect = this.canvas.getBoundingClientRect();
          const x = e.clientX - rect.left;
          const y = e.clientY - rect.top;
          this._handleHover(x, y);
          return;
        }
        const dx = e.clientX - lastX;
        lastX = e.clientX;
        const range = this.visibleRange.end - this.visibleRange.start;
        const plotWidth = this.width - this.padding.left - this.padding.right;
        const deltaTs = -(dx / plotWidth) * range;
        this.pan(deltaTs);
        if (this.zoomCallback) this.zoomCallback(this.visibleRange);
      });

      window.addEventListener('mouseup', () => {
        isDragging = false;
        this.canvas.style.cursor = 'crosshair';
      });

      this.canvas.addEventListener('click', (e) => {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        this._handleClick(x, y);
      });
    }

    _handleHover(x, y) {}
    _handleClick(x, y) {}
    render() {}
  }

  // ==================== Global Zoom/Pan Sync ====================
  let charts = [];
  function registerChart(chart) {
    charts.push(chart);
    chart.zoomCallback = (range) => {
      charts.forEach(c => {
        if (c !== chart) c.setVisibleRange(range.start, range.end);
      });
    };
  }

  function zoomIn() {
    const center = (charts[0].visibleRange.start + charts[0].visibleRange.end) / 2;
    charts.forEach(c => c.zoom(0.7, center));
  }

  function zoomOut() {
    const center = (charts[0].visibleRange.start + charts[0].visibleRange.end) / 2;
    charts.forEach(c => c.zoom(1.4, center));
  }

  function resetZoom() {
    const min = charts[0].data.time_range.min;
    const max = charts[0].data.time_range.max;
    charts.forEach(c => c.setVisibleRange(min, max));
  }

  // ==================== StateTimelineChart ====================
  class StateTimelineChart extends ChartEngine {
    constructor(canvasId, data) {
      super(canvasId, data);
      this.transitionPoints = data.transitions;
      this.bandHeight = this.height - this.padding.top - this.padding.bottom;
      // Extract refresh timestamps from commands for overlay rendering
      this.refreshTimes = [];
      if (data.commands) {
        for (const c of data.commands) {
          if (c.cmd === 'REFAB' || c.cmd === 'REFsb' || c.cmd === 'REFab') {
            this.refreshTimes.push(c.timestamp);
          }
        }
      }
    }

    render() {
      const ctx = this.ctx;
      const w = this.width;
      const h = this.height;
      ctx.clearRect(0, 0, w, h);

      if (this.transitionPoints.length === 0) return;

      const range = this.visibleRange.end - this.visibleRange.start;
      const plotWidth = w - this.padding.left - this.padding.right;

      // Build state segments from transitions
      let segments = [];
      let currentState = this.transitionPoints[0].old_state;
      let segmentStart = this.transitionPoints[0].timestamp;

      for (const t of this.transitionPoints) {
        if (t.old_state !== currentState || t.timestamp !== segmentStart) {
          segments.push({ state: currentState, start: segmentStart, end: t.timestamp });
        }
        currentState = t.new_state;
        segmentStart = t.timestamp;
      }
      segments.push({ state: currentState, start: segmentStart, end: this.data.time_range.max });

      // Draw bands
      for (const seg of segments) {
        const x1 = this.timeToX(seg.start);
        const x2 = this.timeToX(seg.end);
        if (x2 < this.padding.left || x1 > w - this.padding.right) continue;
        ctx.fillStyle = seg.state === 'Rd' ? 'rgba(0, 212, 255, 0.25)' : 'rgba(255, 159, 28, 0.25)';
        ctx.fillRect(
          Math.max(x1, this.padding.left),
          this.padding.top,
          Math.min(x2, w - this.padding.right) - Math.max(x1, this.padding.left),
          this.bandHeight
        );
        ctx.strokeStyle = seg.state === 'Rd' ? '#00d4ff' : '#ff9f1c';
        ctx.lineWidth = 1;
        ctx.strokeRect(
          Math.max(x1, this.padding.left),
          this.padding.top,
          Math.min(x2, w - this.padding.right) - Math.max(x1, this.padding.left),
          this.bandHeight
        );
      }

      // Draw transition points
      for (const t of this.transitionPoints) {
        const x = this.timeToX(t.timestamp);
        if (x < this.padding.left || x > w - this.padding.right) continue;
        
        ctx.beginPath();
        ctx.arc(x, this.padding.top + this.bandHeight / 2, 4, 0, Math.PI * 2);
        ctx.fillStyle = '#f1f5f9';
        ctx.fill();
        ctx.strokeStyle = '#0f172a';
        ctx.lineWidth = 1;
        ctx.stroke();
      }

      // Draw refresh command markers
      ctx.strokeStyle = '#9b5de5';
      ctx.lineWidth = 1.5;
      const refreshY = this.padding.top + this.bandHeight - 2;
      for (const ts of this.refreshTimes) {
        const x = this.timeToX(ts);
        if (x < this.padding.left || x > w - this.padding.right) continue;
        ctx.beginPath();
        ctx.moveTo(x, refreshY - 6);
        ctx.lineTo(x, refreshY);
        ctx.stroke();
      }

      // Draw axes
      this._drawAxes(ctx);
    }

    _drawAxes(ctx) {
      ctx.strokeStyle = '#334155';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(this.padding.left, this.height - this.padding.bottom);
      ctx.lineTo(this.width - this.padding.right, this.height - this.padding.bottom);
      ctx.stroke();

      const range = this.visibleRange.end - this.visibleRange.start;
      const numTicks = 5;
      ctx.fillStyle = '#94a3b8';
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'center';
      for (let i = 0; i <= numTicks; i++) {
        const ts = this.visibleRange.start + (range * i / numTicks);
        const x = this.timeToX(ts);
        ctx.fillText((ts / 1000).toFixed(1) + ' ns', x, this.height - 4);
      }
    }

    _handleHover(x, y) {
      const tooltip = document.getElementById('state-tooltip');
      let nearest = null;
      let minDist = Infinity;
      for (const t of this.transitionPoints) {
        const tx = this.timeToX(t.timestamp);
        const dist = Math.abs(x - tx);
        if (dist < 10 && dist < minDist) {
          minDist = dist;
          nearest = t;
        }
      }
      if (nearest) {
        tooltip.style.display = 'block';
        const rect = this.canvas.getBoundingClientRect();
        tooltip.style.left = (rect.left + window.scrollX + x + 10) + 'px';
        tooltip.style.top = (rect.top + window.scrollY + this.padding.top) + 'px';

        let criticalHtml = '';
        if (nearest.scheduler_snapshot && nearest.scheduler_snapshot.pch) {
          const parts = [];
          for (const [idx, pch] of Object.entries(nearest.scheduler_snapshot.pch)) {
            const c = [];
            if (pch.critical_hpr) c.push('HPR');
            if (pch.critical_lpr) c.push('LPR');
            if (pch.critical_tpw) c.push('TPW');
            if (c.length) parts.push(`PCH${idx}=${c.join('+')}`);
          }
          if (parts.length) {
            criticalHtml = `<div style="margin-top:4px;font-size:10px;color:#ef4444;">Critical: ${parts.join(' ')}</div>`;
          }
        }

        tooltip.innerHTML = `
          <div class="ts">${nearest.timestamp} ps</div>
          <div>${nearest.old_state} &rarr; ${nearest.new_state}</div>
          <div class="reason">${nearest.reason}</div>
          ${criticalHtml}
        `;
        if (this.hoverCallback) this.hoverCallback(nearest);
      } else {
        tooltip.style.display = 'none';
      }
    }

    _handleClick(x, y) {
      let nearest = null;
      let minDist = Infinity;
      for (const t of this.transitionPoints) {
        const tx = this.timeToX(t.timestamp);
        const dist = Math.abs(x - tx);
        if (dist < 10 && dist < minDist) {
          minDist = dist;
          nearest = t;
        }
      }
      if (nearest && this.clickCallback) {
        this.clickCallback(nearest);
      }
    }
  }

  // ==================== SwimlaneChart ====================
  class SwimlaneChart extends ChartEngine {
    constructor(canvasId, data) {
      super(canvasId, data);
      this.commands = data.commands;
      this.lanes = [
        { key: 'PCH0_ACT', label: 'PCH0 ACT', filter: c => c.pch === 0 && c.cmd === 'ACT' },
        { key: 'PCH0_RD', label: 'PCH0 RD/RDA', filter: c => c.pch === 0 && (c.cmd === 'RD' || c.cmd === 'RDA') },
        { key: 'PCH0_WR', label: 'PCH0 WR/WRA', filter: c => c.pch === 0 && (c.cmd === 'WR' || c.cmd === 'WRA') },
        { key: 'PCH0_REF', label: 'PCH0 REF', filter: c => c.pch === 0 && (c.cmd === 'REFAB' || c.cmd === 'REFsb' || c.cmd === 'REFab') },
        { key: 'PCH1_ACT', label: 'PCH1 ACT', filter: c => c.pch === 1 && c.cmd === 'ACT' },
        { key: 'PCH1_RD', label: 'PCH1 RD/RDA', filter: c => c.pch === 1 && (c.cmd === 'RD' || c.cmd === 'RDA') },
        { key: 'PCH1_WR', label: 'PCH1 WR/WRA', filter: c => c.pch === 1 && (c.cmd === 'WR' || c.cmd === 'WRA') },
        { key: 'PCH1_REF', label: 'PCH1 REF', filter: c => c.pch === 1 && (c.cmd === 'REFAB' || c.cmd === 'REFsb' || c.cmd === 'REFab') },
      ];
      this.laneHeight = (this.height - this.padding.top - this.padding.bottom) / this.lanes.length;
    }

    render() {
      const ctx = this.ctx;
      const w = this.width;
      const h = this.height;
      ctx.clearRect(0, 0, w, h);

      // Draw lane backgrounds
      for (let i = 0; i < this.lanes.length; i++) {
        const y = this.padding.top + i * this.laneHeight;
        ctx.fillStyle = i % 2 === 0 ? 'rgba(30, 41, 59, 0.5)' : 'rgba(30, 41, 59, 0.3)';
        ctx.fillRect(this.padding.left, y, w - this.padding.left - this.padding.right, this.laneHeight);

        ctx.fillStyle = '#94a3b8';
        ctx.font = '10px sans-serif';
        ctx.textAlign = 'right';
        ctx.textBaseline = 'middle';
        ctx.fillText(this.lanes[i].label, this.padding.left - 6, y + this.laneHeight / 2);
      }

      // Draw commands
      const cmdColors = {
        'ACT': '#2ec4b6',
        'RD': '#00d4ff',
        'RDA': '#00d4ff',
        'WR': '#ff9f1c',
        'WRA': '#ff9f1c',
        'REFAB': '#9b5de5',
      };

      const dotRadius = Math.max(2, Math.min(4, this.laneHeight * 0.15));

      for (const cmd of this.commands) {
        const x = this.timeToX(cmd.timestamp);
        if (x < this.padding.left || x > w - this.padding.right) continue;

        for (let i = 0; i < this.lanes.length; i++) {
          if (this.lanes[i].filter(cmd)) {
            const y = this.padding.top + i * this.laneHeight + this.laneHeight / 2;
            ctx.beginPath();
            ctx.arc(x, y, dotRadius, 0, Math.PI * 2);
            ctx.fillStyle = cmdColors[cmd.cmd] || '#94a3b8';
            ctx.fill();
            break;
          }
        }
      }

      // Axes
      ctx.strokeStyle = '#334155';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(this.padding.left, h - this.padding.bottom);
      ctx.lineTo(w - this.padding.right, h - this.padding.bottom);
      ctx.stroke();
    }
  }

  // ==================== Detail Panel ====================
  function updateDetailPanel(transition) {
    const container = document.getElementById('detail-content');
    const ss = transition.scheduler_snapshot;

    let html = '<div class="detail-section">';
    html += '<h3>Transition Info</h3>';
    html += `<div style="font-size: 12px; margin-bottom: 4px;"><strong>Time:</strong> ${transition.timestamp} ps</div>`;
    html += `<div style="font-size: 12px; margin-bottom: 4px;"><strong>Change:</strong> ${transition.old_state} &rarr; ${transition.new_state}</div>`;
    html += `<div style="font-size: 12px; color: var(--accent);"><strong>Reason:</strong> ${transition.reason}</div>`;
    html += '</div>';

    // Global State Conditions
    if (transition.global_state && Object.keys(transition.global_state).length > 0) {
      html += '<div class="detail-section">';
      html += '<h3>Global State Conditions</h3>';

      const groups = [
        {
          title: 'Page Hit & Critical',
          items: [
            { key: 'expired_rd_hit_activing', label: 'Exp RD Active' },
            { key: 'expired_wr_hit_activing', label: 'Exp WR Active' },
            { key: 'flush_rd_hit_activing', label: 'Flush RD Active' },
            { key: 'flush_wr_hit_activing', label: 'Flush WR Active' },
            { key: 'flush_wr_hit_actived', label: 'Flush WR Acted' },
            { key: 'rd_critical_hit_activing', label: 'RD Crit Active' },
            { key: 'wr_critical_hit_actived', label: 'WR Crit Acted' },
          ]
        },
        {
          title: 'Avail Count',
          items: [
            { key: 'both_rd_avail_high', label: 'Both RD High' },
            { key: 'both_wr_avail_high', label: 'Both WR High' },
            { key: 'both_rd_avail_medium', label: 'Both RD Med' },
            { key: 'both_wr_avail_medium', label: 'Both WR Med' },
            { key: 'has_rd_avail', label: 'Has RD Avail' },
            { key: 'has_wr_avail', label: 'Has WR Avail' },
          ]
        },
        {
          title: 'Valid Count',
          items: [
            { key: 'both_rd_no_valid', label: 'Both RD No Valid' },
            { key: 'has_wr_valid', label: 'Has WR Valid' },
            { key: 'both_wr_no_valid', label: 'Both WR No Valid' },
            { key: 'has_rd_valid', label: 'Has RD Valid' },
            { key: 'one_pch_rd_valid', label: 'One PCH RD Valid' },
            { key: 'both_wr_valid', label: 'Both WR Valid' },
            { key: 'both_rd_valid', label: 'Both RD Valid' },
          ]
        },
        {
          title: 'CAM Threshold',
          items: [
            { key: 'both_wr_cam_above', label: 'Both WR CAM Above' },
          ]
        }
      ];

      html += `<div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px;">`;
      for (const group of groups) {
        html += `<div style="background: var(--bg-primary); border-radius: 6px; padding: 10px;">`;
        html += `<div style="font-size: 10px; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 6px;">${group.title}</div>`;
        html += `<div style="display: flex; flex-wrap: wrap; gap: 4px;">`;
        for (const item of group.items) {
          const val = transition.global_state[item.key];
          const isActive = val === 1;
          const bg = isActive ? 'rgba(99, 102, 241, 0.15)' : 'rgba(30, 41, 59, 0.5)';
          const color = isActive ? '#818cf8' : '#64748b';
          const border = isActive ? '1px solid rgba(99, 102, 241, 0.3)' : '1px solid rgba(51, 65, 85, 0.5)';
          html += `<span style="display: inline-block; padding: 3px 6px; border-radius: 4px; font-size: 10px; background: ${bg}; color: ${color}; border: ${border};">${item.label}: <strong>${val}</strong></span>`;
        }
        html += `</div></div>`;
      }
      html += `</div>`;
      html += `</div>`;
    }

    if (ss) {
      for (const [pchIdx, pchData] of Object.entries(ss.pch)) {
        html += `<div class="detail-section">`;
        html += `<h3>PCH ${pchIdx}</h3>`;
        html += `<table class="pch-table">`;
        html += `<tr><th>Metric</th><th>RD</th><th>WR</th></tr>`;

        const metrics = [
          ['ValidNtt', pchData.valid_ntt_rd, pchData.valid_ntt_wr],
          ['AvailCmd', pchData.avail_cmd_rd, pchData.avail_cmd_wr],
          ['ValidCAM', pchData.valid_cam_rd, pchData.valid_cam_wr],
        ];
        for (const [name, rd, wr] of metrics) {
          const maxVal = Math.max(rd, wr, 1);
          html += `<tr><td>${name}</td>`;
          html += `<td><div class="metric-bar"><div class="metric-bar-fill rd" style="width: ${(rd / maxVal) * 100}%"></div></div>${rd}</td>`;
          html += `<td><div class="metric-bar"><div class="metric-bar-fill wr" style="width: ${(wr / maxVal) * 100}%"></div></div>${wr}</td>`;
          html += `</tr>`;
        }
        const criticalStatus = [
          { key: 'critical_hpr', label: 'HPR', color: '#ef4444' },
          { key: 'critical_lpr', label: 'LPR', color: '#f97316' },
          { key: 'critical_tpw', label: 'TPW', color: '#a855f7' },
        ];
        html += `<tr><td>Critical</td><td colspan="2" style="padding: 6px;">`;
        html += `<div style="display: flex; gap: 10px;">`;
        for (const cs of criticalStatus) {
          const isActive = pchData[cs.key] === 1;
          const bg = isActive ? cs.color : '#334155';
          const glow = isActive ? `box-shadow: 0 0 6px ${cs.color};` : '';
          html += `<div style="display: flex; align-items: center; gap: 4px;">`;
          html += `<span style="width: 10px; height: 10px; border-radius: 50%; background: ${bg}; ${glow} display: inline-block; transition: all 0.2s;"></span>`;
          html += `<span style="font-size: 10px; color: ${isActive ? cs.color : '#64748b'}; font-weight: ${isActive ? 600 : 400};">${cs.label}</span>`;
          html += `</div>`;
        }
        html += `</div></td></tr>`;
        html += `</table>`;
        html += `</div>`;
      }
    } else {
      html += `<p style="color: var(--text-secondary); font-size: 12px;">No scheduler snapshot available.</p>`;
    }

    container.innerHTML = html;
  }

  // ==================== MetricsChart ====================
  class MetricsChart extends ChartEngine {
    constructor(canvasId, data) {
      super(canvasId, data);
      this.transitions = data.transitions;
      this.metrics = ['valid_ntt_rd', 'valid_ntt_wr', 'avail_cmd_rd', 'avail_cmd_wr', 'valid_cam_rd', 'valid_cam_wr'];
      this.colors = {
        'valid_ntt_rd': '#00d4ff',
        'valid_ntt_wr': '#ff9f1c',
        'avail_cmd_rd': '#2ec4b6',
        'avail_cmd_wr': '#ff6b6b',
        'valid_cam_rd': '#9b5de5',
        'valid_cam_wr': '#f9c74f',
      };
      this.labels = {
        'valid_ntt_rd': 'ValidNtt RD',
        'valid_ntt_wr': 'ValidNtt WR',
        'avail_cmd_rd': 'AvailCmd RD',
        'avail_cmd_wr': 'AvailCmd WR',
        'valid_cam_rd': 'ValidCAM RD',
        'valid_cam_wr': 'ValidCAM WR',
      };
    }

    render() {
      const ctx = this.ctx;
      const w = this.width;
      const h = this.height;
      ctx.clearRect(0, 0, w, h);

      const seriesData = {};
      for (const m of this.metrics) seriesData[m] = [];

      for (const t of this.transitions) {
        if (!t.scheduler_snapshot || !t.scheduler_snapshot.pch) continue;
        const ts = t.timestamp;
        for (const m of this.metrics) {
          let val = 0;
          for (const pch of Object.values(t.scheduler_snapshot.pch)) {
            val += pch[m] || 0;
          }
          seriesData[m].push({ ts, val });
        }
      }

      let maxVal = 1;
      for (const m of this.metrics) {
        for (const pt of seriesData[m]) {
          if (pt.val > maxVal) maxVal = pt.val;
        }
      }

      const plotHeight = h - this.padding.top - this.padding.bottom;
      const plotWidth = w - this.padding.left - this.padding.right;

      // Draw grid lines
      ctx.strokeStyle = '#1e293b';
      ctx.lineWidth = 1;
      for (let i = 0; i <= 4; i++) {
        const y = this.padding.top + (plotHeight * i / 4);
        ctx.beginPath();
        ctx.moveTo(this.padding.left, y);
        ctx.lineTo(w - this.padding.right, y);
        ctx.stroke();
        ctx.fillStyle = '#94a3b8';
        ctx.font = '9px sans-serif';
        ctx.textAlign = 'right';
        ctx.fillText(Math.round(maxVal * (1 - i / 4)), this.padding.left - 4, y + 3);
      }

      // Draw lines
      for (const m of this.metrics) {
        const pts = seriesData[m];
        if (pts.length < 2) continue;
        ctx.beginPath();
        ctx.strokeStyle = this.colors[m];
        ctx.lineWidth = 1.5;
        let first = true;
        for (const pt of pts) {
          const x = this.timeToX(pt.ts);
          const y = this.padding.top + plotHeight * (1 - pt.val / maxVal);
          if (x < this.padding.left || x > w - this.padding.right) continue;
          if (first) { ctx.moveTo(x, y); first = false; }
          else { ctx.lineTo(x, y); }
        }
        ctx.stroke();
      }

      // Legend
      let lx = this.padding.left;
      const ly = 14;
      for (const m of this.metrics) {
        ctx.fillStyle = this.colors[m];
        ctx.fillRect(lx, ly - 6, 10, 3);
        ctx.fillStyle = '#94a3b8';
        ctx.font = '9px sans-serif';
        ctx.textAlign = 'left';
        ctx.fillText(this.labels[m], lx + 14, ly);
        lx += ctx.measureText(this.labels[m]).width + 28;
      }

      // Axes
      ctx.strokeStyle = '#334155';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(this.padding.left, h - this.padding.bottom);
      ctx.lineTo(w - this.padding.right, h - this.padding.bottom);
      ctx.stroke();
    }
  }

  // ==================== CriticalStatusChart ====================
  class CriticalStatusChart extends ChartEngine {
    constructor(canvasId, data) {
      super(canvasId, data);
      this.transitions = data.transitions;
      this.criticalTypes = ['hpr', 'lpr', 'tpw'];
      this.colors = {
        'hpr': '#ef4444',
        'lpr': '#f97316',
        'tpw': '#a855f7',
      };
      this.labels = {
        'hpr': 'HPR Critical',
        'lpr': 'LPR Critical',
        'tpw': 'TPW Critical',
      };
    }

    render() {
      const ctx = this.ctx;
      const w = this.width;
      const h = this.height;
      ctx.clearRect(0, 0, w, h);

      const seriesData = {};
      for (const ct of this.criticalTypes) seriesData[ct] = [];

      for (const t of this.transitions) {
        if (!t.scheduler_snapshot || !t.scheduler_snapshot.pch) continue;
        const ts = t.timestamp;
        for (const ct of this.criticalTypes) {
          const key = 'critical_' + ct;
          let val = 0;
          for (const pch of Object.values(t.scheduler_snapshot.pch)) {
            if (pch[key] === 1) { val = 1; break; }
          }
          seriesData[ct].push({ ts, val });
        }
      }

      const plotHeight = h - this.padding.top - this.padding.bottom;
      const plotWidth = w - this.padding.left - this.padding.right;
      const laneHeight = plotHeight / this.criticalTypes.length;

      // Draw horizontal reference lines and labels for each critical type
      for (let i = 0; i < this.criticalTypes.length; i++) {
        const y = this.padding.top + i * laneHeight + laneHeight / 2;
        ctx.strokeStyle = '#1e293b';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(this.padding.left, y);
        ctx.lineTo(w - this.padding.right, y);
        ctx.stroke();

        ctx.fillStyle = '#94a3b8';
        ctx.font = '10px sans-serif';
        ctx.textAlign = 'right';
        ctx.textBaseline = 'middle';
        ctx.fillText(this.labels[this.criticalTypes[i]], this.padding.left - 6, y);
      }

      // Draw critical state markers (only active ones)
      const dotRadius = Math.max(2, Math.min(4, laneHeight * 0.15));
      for (const ct of this.criticalTypes) {
        const pts = seriesData[ct];
        const i = this.criticalTypes.indexOf(ct);
        const baseY = this.padding.top + i * laneHeight + laneHeight / 2;

        for (const pt of pts) {
          if (pt.val !== 1) continue;
          const x = this.timeToX(pt.ts);
          if (x < this.padding.left || x > w - this.padding.right) continue;

          ctx.beginPath();
          ctx.arc(x, baseY, dotRadius, 0, Math.PI * 2);
          ctx.fillStyle = this.colors[ct];
          ctx.fill();
        }
      }

      // Axes
      ctx.strokeStyle = '#334155';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(this.padding.left, h - this.padding.bottom);
      ctx.lineTo(w - this.padding.right, h - this.padding.bottom);
      ctx.stroke();
    }
  }

  // ==================== Statistics Panel ====================
  function renderStats() {
    const container = document.getElementById('stats-content');
    const stats = DATA.stats;
    let html = '';

    for (const [reason, count] of Object.entries(stats.reasons)) {
      const pct = ((count / stats.total) * 100).toFixed(1);
      html += `<div class="stat-card">
        <div class="label">${reason}</div>
        <div class="value">${count} <span style="font-size:11px;color:var(--text-secondary)">(${pct}%)</span></div>
      </div>`;
    }

    for (const [transition, count] of Object.entries(stats.state_counts)) {
      const pct = ((count / stats.total) * 100).toFixed(1);
      html += `<div class="stat-card">
        <div class="label">${transition}</div>
        <div class="value">${count} <span style="font-size:11px;color:var(--text-secondary)">(${pct}%)</span></div>
      </div>`;
    }

    container.innerHTML = html;
  }

  // ==================== Initialize Charts ====================
  const stateChart = new StateTimelineChart('state-timeline', DATA);
  stateChart.hoverCallback = (t) => updateDetailPanel(t);
  stateChart.clickCallback = (t) => updateDetailPanel(t);
  registerChart(stateChart);
  stateChart.render();

  const swimlaneChart = new SwimlaneChart('command-swimlanes', DATA);
  registerChart(swimlaneChart);
  swimlaneChart.render();

  const metricsChart = new MetricsChart('scheduler-metrics', DATA);
  registerChart(metricsChart);
  metricsChart.render();

  const criticalChart = new CriticalStatusChart('critical-status', DATA);
  registerChart(criticalChart);
  criticalChart.render();

  renderStats();

  window.addEventListener('resize', () => {
    charts.forEach(c => {
      c._setupCanvas();
      c.render();
    });
  });
  </script>
</body>
</html>"""


def compute_stats(transitions):
    from collections import Counter
    reasons = Counter()
    state_counts = Counter()
    for t in transitions:
        reasons[t["reason"]] += 1
        state_counts[f"{t['old_state']}->{t['new_state']}"] += 1
    return {
        "reasons": dict(reasons.most_common()),
        "state_counts": dict(state_counts),
        "total": len(transitions),
    }


def generate_html(input_dir, transitions, command_events):
    data = {
        "transitions": transitions,
        "commands": command_events,
        "stats": compute_stats(transitions),
        "time_range": {
            "min": min(t["timestamp"] for t in transitions) if transitions else 0,
            "max": max(t["timestamp"] for t in transitions) if transitions else 0,
        }
    }
    generated_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    html = HTML_TEMPLATE
    html = html.replace("{{input_dir}}", str(input_dir))
    html = html.replace("{{generated_time}}", generated_time)
    html = html.replace("{{transition_count}}", str(len(transitions)))
    html = html.replace("{{command_count}}", str(len(command_events)))
    data_json = json.dumps(data)
    # Prevent </script> injection and template placeholder collision
    data_json = data_json.replace("</script>", "\\u003c/script\\u003e")
    data_json = data_json.replace("<!--", "\\u003c!--")
    html = html.replace("{{data_json}}", data_json)
    return html


def main():
    parser = argparse.ArgumentParser(
        description="Parse simulation log files and generate an interactive HTML dashboard."
    )
    parser.add_argument(
        "--input-dir",
        required=True,
        help="Path to simulation output directory containing .log files",
    )
    parser.add_argument(
        "--output",
        default="ddr_visualizer_report.html",
        help="Output HTML file path. If a relative filename is given, it is placed in --input-dir (default: ddr_visualizer_report.html)",
    )
    # Reserved for adaptive sampling (Task 4)
    parser.add_argument(
        "--max-cmd-events",
        type=int,
        default=50000,
        help="Max command events to render per source (default: 50000)."
    )

    args = parser.parse_args()

    try:
        input_dir = Path(args.input_dir)
        if not input_dir.exists() or not input_dir.is_dir():
            print(f"Error: --input-dir does not exist or is not a directory: {input_dir}", file=sys.stderr)
            sys.exit(1)

        output_path = Path(args.output)
        # If output is a relative filename (no directory components), place it in input-dir
        if not output_path.is_absolute() and len(output_path.parts) == 1:
            output_path = input_dir / output_path

        print(f"Input directory: {input_dir}")
        print(f"Output path: {output_path}")

        mode_switch_log = input_dir / "MrdimmModeSwitch.log"
        transitions = []
        scheduler_snapshots = []
        if mode_switch_log.exists():
            print("Parsing MrdimmModeSwitch.log ...")
            transitions, scheduler_snapshots = parse_mrdimm_mode_switch(mode_switch_log)
            print(f"  Found {len(transitions)} state transitions")
            print(f"  Found {len(scheduler_snapshots)} scheduler snapshots")
        else:
            print(f"Warning: {mode_switch_log} not found")

        # Parse command logs
        command_events = []
        dfi_cmd_log = input_dir / "phy_delay_model_DfiCmd.log"
        if dfi_cmd_log.exists():
            print("Parsing phy_delay_model_DfiCmd.log ...")
            events = parse_dfi_cmd_log(dfi_cmd_log)
            print(f"  Found {len(events)} DFI command events")
            command_events.extend(events)

        for mc_idx in [0, 1]:
            mc_log = input_dir / f"Mc_{mc_idx}.log"
            if mc_log.exists():
                print(f"Parsing Mc_{mc_idx}.log ...")
                events = parse_mc_log(mc_log, mc_idx)
                print(f"  Found {len(events)} Mc_{mc_idx} command events")
                command_events.extend(events)

        # Apply adaptive sampling to command events
        print(f"Total command events before sampling: {len(command_events)}")
        command_events = sample_command_events(command_events, args.max_cmd_events)
        print(f"Total command events after sampling: {len(command_events)}")

        # Ensure transitions have scheduler snapshots
        transitions = aggregate_scheduler_for_transitions(transitions, scheduler_snapshots)

        # Clip visualization to the last read/write command time to avoid
        # trailing empty regions caused by endless refresh commands.
        rw_cmds = {"RD", "RDA", "WR", "WRA"}
        last_rw_time = None
        for e in command_events:
            if e.get("cmd") in rw_cmds:
                ts = e.get("timestamp")
                if ts is not None and (last_rw_time is None or ts > last_rw_time):
                    last_rw_time = ts

        if last_rw_time is not None:
            orig_trans_count = len(transitions)
            orig_cmd_count = len(command_events)
            transitions = [t for t in transitions if t["timestamp"] <= last_rw_time]
            command_events = [e for e in command_events if e["timestamp"] <= last_rw_time]
            print(
                f"Clipped to last RD/RDA/WR/WRA at {last_rw_time} ps \n"
                f"  (transitions: {orig_trans_count} -> {len(transitions)}), \n"
                f"  (commands: {orig_cmd_count} -> {len(command_events)})"
            )

        print("Generating HTML report ...")
        html_content = generate_html(input_dir, transitions, command_events)
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(html_content)
        print(f"Done. Report written to: {output_path}")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()