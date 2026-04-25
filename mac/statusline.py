#!/usr/bin/env python3
"""Claude Code statusline hook that pushes usage data to a cydStudio device.

Claude Code invokes this on every turn with a JSON payload on stdin. We extract
the five-hour (session) and seven-day (weekly) rate-limit fields, POST them to
the device's /data/claude push datasource, and print a short statusline string
for the Claude Code TUI.

Configure in ~/.claude/settings.json:

    {
      "statusLine": {
        "type": "command",
        "command": "/absolute/path/to/cydStudio/mac/statusline.py"
      }
    }

Env vars:
    CYDSTUDIO_DEVICE_URL   Base URL of the device (default http://claude-monitor.local).
                           The script POSTs to {base}/data/claude.
"""
from __future__ import annotations

import datetime as dt
import json
import os
import sys
import urllib.request

DEVICE_URL = os.environ.get("CYDSTUDIO_DEVICE_URL", "http://claude-monitor.local")
PUSH_PATH  = "/data/claude"
POST_TIMEOUT_SEC = 1.0


def read_input() -> dict:
    try:
        return json.load(sys.stdin)
    except Exception:
        return {}


def to_seconds_until(iso_ts: str | None) -> int | None:
    if not iso_ts:
        return None
    try:
        ts = dt.datetime.fromisoformat(iso_ts.replace("Z", "+00:00"))
        delta = (ts - dt.datetime.now(tz=ts.tzinfo)).total_seconds()
        return max(int(delta), 0)
    except Exception:
        return None


def extract_payload(data: dict) -> dict:
    rl = data.get("rate_limits") or {}
    five = rl.get("five_hour") or {}
    seven = rl.get("seven_day") or {}
    payload = {}
    if five.get("used_percentage") is not None:
        payload["session_pct"] = float(five["used_percentage"])
        payload["session_valid"] = True
    sec = to_seconds_until(five.get("resets_at"))
    if sec is not None:
        payload["session_reset_in_sec"] = sec
    if seven.get("used_percentage") is not None:
        payload["week_pct"] = float(seven["used_percentage"])
        payload["week_valid"] = True
    sec = to_seconds_until(seven.get("resets_at"))
    if sec is not None:
        payload["week_reset_in_sec"] = sec
    return payload


def push(payload: dict) -> None:
    if not payload:
        return
    body = json.dumps(payload).encode()
    req = urllib.request.Request(
        DEVICE_URL + PUSH_PATH,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        urllib.request.urlopen(req, timeout=POST_TIMEOUT_SEC).read()
    except Exception:
        # Network failures must never break the statusline.
        pass


def format_statusline(data: dict, payload: dict) -> str:
    model = (data.get("model") or {}).get("display_name") or "Claude"
    ws = data.get("workspace") or {}
    cwd = ws.get("current_dir") or data.get("cwd") or ""
    folder = os.path.basename(cwd.rstrip("/")) if cwd else ""
    parts = [model]
    if folder:
        parts.append(folder)
    if "session_pct" in payload:
        parts.append(f"S {payload['session_pct']:.0f}%")
    if "week_pct" in payload:
        parts.append(f"W {payload['week_pct']:.0f}%")
    return "  ".join(parts)


def main() -> None:
    data = read_input()
    payload = extract_payload(data)
    push(payload)
    print(format_statusline(data, payload))


if __name__ == "__main__":
    main()
