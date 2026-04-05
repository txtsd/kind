"""Startup timeline analyzer: launch-to-interactive pipeline."""

from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Thresholds (milliseconds)
GATEWAY_CONNECT_WARN_MS = 3000
GATEWAY_CONNECT_PROBLEM_MS = 6000
TOTAL_STARTUP_WARN_MS = 8000
TOTAL_STARTUP_PROBLEM_MS = 15000

# Startup stages in order, mapped to their gateway event types.
_STAGES = [
  ("connecting", "App launch / login start"),
  ("hello", "WebSocket established"),
  ("ready", "Gateway ready, session received"),
]


def _delta_ms(ts_a: str | None, ts_b: str | None, db: Connection) -> float | None:
  """Compute milliseconds between two ISO timestamps using SQLite JULIANDAY."""
  if not ts_a or not ts_b:
    return None
  row = db.execute(
    "SELECT (JULIANDAY(?) - JULIANDAY(?)) * 86400000",
    (ts_b, ts_a),
  ).fetchone()
  val = row[0] if row else None
  if val is not None and val < 0:
    return None
  return val


def run(db: Connection, fmt: Formatter) -> None:
  """Run the startup timeline analysis."""
  total = db.execute("SELECT COUNT(*) FROM gateway_events").fetchone()[0]

  if total == 0:
    print(fmt.header("Startup Analysis", "No gateway event data found"))
    return

  # Collect first occurrence of each stage
  stage_timestamps: dict[str, str | None] = {}
  for event_type, _desc in _STAGES:
    row = db.execute(
      "SELECT timestamp FROM gateway_events"
      " WHERE event_type = ?"
      " ORDER BY timestamp ASC"
      " LIMIT 1",
      (event_type,),
    ).fetchone()
    stage_timestamps[event_type] = row[0] if row else None

  # First channel switch
  ch_row = db.execute(
    "SELECT switch_at FROM channel_switches"
    " ORDER BY switch_at ASC"
    " LIMIT 1"
  ).fetchone()
  first_channel_ts = ch_row[0] if ch_row else None

  # Check we have at least the connecting event
  if not stage_timestamps.get("connecting"):
    print(fmt.header(
      "Startup Analysis",
      "No gateway connecting event in this log window (app was already connected)",
    ))
    return

  print(fmt.header("Startup Analysis"))
  print()

  # ── Startup Timeline ──────────────────────────────────
  print(fmt.section("Startup Timeline"))
  print()

  headers = ["Stage", "Timestamp", "Delta"]
  rows = []

  ordered_points: list[tuple[str, str | None]] = [
    (desc, stage_timestamps[event_type]) for event_type, desc in _STAGES
  ]
  ordered_points.append(("First channel loaded", first_channel_ts))

  prev_ts: str | None = None
  for label, ts in ordered_points:
    ts_display = ts or "-"
    if prev_ts and ts:
      delta = _delta_ms(prev_ts, ts, db)
      delta_display = fmt.duration(delta) if delta is not None else "-"
    else:
      delta_display = "-"
    rows.append([label, ts_display, delta_display])
    if ts:
      prev_ts = ts

  print(fmt.table(headers, rows, ["l", "l", "r"]))
  print()

  # Total startup time
  base_ts = stage_timestamps["connecting"]
  end_ts = first_channel_ts or stage_timestamps.get("ready")
  total_ms = _delta_ms(base_ts, end_ts, db)

  if total_ms is not None:
    print(f"  Total startup time: {fmt.bold(fmt.duration(total_ms))}")
    print()

  # ── Suggestions ──────────────────────────────────────
  print(fmt.section("Suggestions"))
  print()

  # Gateway connection speed (connecting -> hello)
  connect_ms = _delta_ms(
    stage_timestamps.get("connecting"),
    stage_timestamps.get("hello"),
    db,
  )
  if connect_ms is not None:
    if connect_ms >= GATEWAY_CONNECT_PROBLEM_MS:
      print(fmt.problem(
        f"Gateway connection: {fmt.duration(connect_ms)} "
        f"(very slow WebSocket handshake)"
      ))
    elif connect_ms >= GATEWAY_CONNECT_WARN_MS:
      print(fmt.warning(
        f"Gateway connection: {fmt.duration(connect_ms)} "
        f"(slow WebSocket handshake)"
      ))
    else:
      print(fmt.healthy(
        f"Gateway connection: {fmt.duration(connect_ms)}"
      ))

  # Total startup assessment
  if total_ms is not None:
    if total_ms >= TOTAL_STARTUP_PROBLEM_MS:
      print(fmt.problem(
        f"Total startup: {fmt.duration(total_ms)} "
        f"(very slow, investigate bottleneck stages above)"
      ))
    elif total_ms >= TOTAL_STARTUP_WARN_MS:
      print(fmt.warning(
        f"Total startup: {fmt.duration(total_ms)} "
        f"(slow, check network or gateway delays)"
      ))
    else:
      print(fmt.healthy(
        f"Total startup: {fmt.duration(total_ms)}"
      ))

  # Check if ready -> first channel is the bottleneck
  ready_to_channel = _delta_ms(
    stage_timestamps.get("ready"),
    first_channel_ts,
    db,
  )
  if ready_to_channel is not None and total_ms is not None and total_ms > 0:
    pct = ready_to_channel / total_ms * 100
    if pct > 60:
      print(fmt.warning(
        f"Channel load takes {pct:.0f}% of startup time "
        f"({fmt.duration(ready_to_channel)}), "
        f"consider prefetching channel data"
      ))

  print()
