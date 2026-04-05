"""Gateway websocket health and heartbeat timing analyzer."""

import re
from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Thresholds
RECONNECT_WARN = 2
RECONNECT_PROBLEM = 5
HEARTBEAT_MISS_WARN = 3
HEARTBEAT_MISS_PROBLEM = 8
RTT_P95_WARN_MS = 500
RTT_P95_PROBLEM_MS = 1000


def _percentile(sorted_vals: list[float], p: float) -> float:
  """Compute the p-th percentile from a sorted list of values."""
  if not sorted_vals:
    return 0.0
  k = (len(sorted_vals) - 1) * (p / 100)
  lo = int(k)
  hi = min(lo + 1, len(sorted_vals) - 1)
  frac = k - lo
  return sorted_vals[lo] + frac * (sorted_vals[hi] - sorted_vals[lo])


def run(db: Connection, fmt: Formatter) -> None:
  """Run the gateway websocket health analysis."""
  total = db.execute("SELECT COUNT(*) FROM gateway_events").fetchone()[0]

  if total == 0:
    print(fmt.header("Gateway Analysis", "No gateway event data found"))
    return

  # Time range
  row = db.execute(
    "SELECT MIN(timestamp), MAX(timestamp) FROM gateway_events"
  ).fetchone()
  time_start = row[0] if row else None
  time_end = row[1] if row else None

  period = ""
  if time_start and time_end:
    period = f"{time_start} to {time_end}"

  print(fmt.header("Gateway Analysis", f"Period: {period}" if period else ""))
  print()

  # ── Connection Lifecycle ─────────────────────────────
  print(fmt.section("Connection Lifecycle"))
  print()

  type_rows = db.execute(
    "SELECT event_type, COUNT(*) AS cnt"
    " FROM gateway_events"
    " GROUP BY event_type"
    " ORDER BY cnt DESC"
  ).fetchall()

  if type_rows:
    headers = ["Event Type", "Count"]
    rows = []
    for r in type_rows:
      rows.append([r[0], str(r[1])])
    print(fmt.table(headers, rows, ["l", "r"]))
    print()

  # ── Heartbeat Health ─────────────────────────────────
  print(fmt.section("Heartbeat Health"))
  print()

  heartbeat_sent = db.execute(
    "SELECT COUNT(*) FROM gateway_events"
    " WHERE (event_type = 'sending' AND detail = 'heartbeat')"
    " OR event_type = 'heartbeat'"
  ).fetchone()[0]
  heartbeat_ack = db.execute(
    "SELECT COUNT(*) FROM gateway_events WHERE event_type = 'heartbeat_ack'"
  ).fetchone()[0]
  heartbeat_miss = max(0, heartbeat_sent - heartbeat_ack)

  print(f"  Heartbeats sent:  {fmt.bold(str(heartbeat_sent))}")
  print(f"  ACKs received:    {fmt.bold(str(heartbeat_ack))}")
  print(f"  Missed:           {fmt.bold(str(heartbeat_miss))}")
  print()

  # RTT data from heartbeat_ack detail field
  rtt_rows = db.execute(
    "SELECT detail FROM gateway_events"
    " WHERE event_type = 'heartbeat_ack'"
    " AND detail LIKE '%rtt=%'"
  ).fetchall()

  rtt_values: list[float] = []
  for r in rtt_rows:
    m = re.search(r"rtt=(\d+)", r[0])
    if m:
      rtt_values.append(float(m.group(1)))

  if rtt_values:
    rtt_values.sort()
    rtt_min = rtt_values[0]
    rtt_max = rtt_values[-1]
    rtt_avg = sum(rtt_values) / len(rtt_values)
    rtt_p50 = _percentile(rtt_values, 50)
    rtt_p95 = _percentile(rtt_values, 95)

    print(f"  RTT samples: {fmt.bold(str(len(rtt_values)))}")
    print(
      f"  Min: {fmt.bold(fmt.duration(rtt_min))}  "
      f"Avg: {fmt.bold(fmt.duration(rtt_avg))}  "
      f"Max: {fmt.bold(fmt.duration(rtt_max))}"
    )
    print(
      f"  p50: {fmt.bold(fmt.duration(rtt_p50))}  "
      f"p95: {fmt.bold(fmt.duration(rtt_p95))}"
    )

    spark = fmt.sparkline(rtt_values)
    if spark:
      print(f"  Distribution: {spark}")
    print()
  else:
    print(
      f"  {fmt.fg('No RTT data available. Consider adding rtt= to heartbeat_ack detail logging.', fmt.theme.muted)}"
    )
    print()

  # ── Reconnections ────────────────────────────────────
  reconnect_types = ("reconnect", "invalid_session", "close")
  placeholders = ", ".join("?" for _ in reconnect_types)
  reconnect_rows = db.execute(
    f"SELECT timestamp, event_type, detail"
    f" FROM gateway_events"
    f" WHERE event_type IN ({placeholders})"
    f" ORDER BY timestamp",
    reconnect_types,
  ).fetchall()

  if reconnect_rows:
    print(fmt.section("Reconnections"))
    print()

    headers = ["Timestamp", "Event", "Detail"]
    rows = []
    for r in reconnect_rows:
      detail = r[2] or ""
      # Truncate long details for table readability
      if len(detail) > 40:
        detail = detail[:37] + "..."
      rows.append([r[0], r[1], detail])

    print(fmt.table(headers, rows, ["l", "l", "l"]))
    print()

  # ── Suggestions ──────────────────────────────────────
  print(fmt.section("Suggestions"))
  print()

  reconnect_count = len(reconnect_rows)
  if reconnect_count >= RECONNECT_PROBLEM:
    print(fmt.problem(
      f"Reconnections: {reconnect_count} (frequent disconnects, check network stability)"
    ))
  elif reconnect_count >= RECONNECT_WARN:
    print(fmt.warning(
      f"Reconnections: {reconnect_count} (some instability detected)"
    ))
  elif reconnect_count > 0:
    print(fmt.healthy(
      f"Reconnections: {reconnect_count} (occasional, within normal range)"
    ))
  else:
    print(fmt.healthy("No reconnections detected"))

  if heartbeat_miss >= HEARTBEAT_MISS_PROBLEM:
    print(fmt.problem(
      f"Heartbeat misses: {heartbeat_miss} (severe, likely causing disconnects)"
    ))
  elif heartbeat_miss >= HEARTBEAT_MISS_WARN:
    print(fmt.warning(
      f"Heartbeat misses: {heartbeat_miss} (may indicate network latency issues)"
    ))
  elif heartbeat_miss > 0:
    print(fmt.healthy(
      f"Heartbeat misses: {heartbeat_miss} (minimal)"
    ))
  else:
    print(fmt.healthy("No heartbeat misses"))

  if rtt_values:
    rtt_p95 = _percentile(rtt_values, 95)
    if rtt_p95 >= RTT_P95_PROBLEM_MS:
      print(fmt.problem(
        f"RTT p95: {fmt.duration(rtt_p95)} (very high latency to Discord gateway)"
      ))
    elif rtt_p95 >= RTT_P95_WARN_MS:
      print(fmt.warning(
        f"RTT p95: {fmt.duration(rtt_p95)} (elevated latency to Discord gateway)"
      ))
    else:
      print(fmt.healthy(
        f"RTT p95: {fmt.duration(rtt_p95)} (healthy)"
      ))

  print()
