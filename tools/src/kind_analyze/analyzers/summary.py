"""One-page health overview pulling headline stats from each domain."""

import re
from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Duration SQL helpers (same as other analyzers)
_IMG_DUR = (
  "(JULIANDAY(download_finished_at) - JULIANDAY(download_started_at))"
  " * 86400000"
)
_IMG_VALID = (
  "download_started_at IS NOT NULL"
  " AND download_finished_at IS NOT NULL"
  " AND download_finished_at > download_started_at"
)
_RENDER_MS = (
  "(JULIANDAY(first_render_at) - JULIANDAY(switch_at))"
  " * 86400000"
)

# Thresholds
IMG_AVG_WARN_MS = 1000
IMG_AVG_PROBLEM_MS = 2000
NET_P95_WARN_MS = 1500
NET_P95_PROBLEM_MS = 3000
HB_MISS_WARN = 3
HB_MISS_PROBLEM = 8
RENDER_WARN_MS = 500
RENDER_PROBLEM_MS = 1000
CACHE_HIT_WARN_PCT = 40
CACHE_HIT_PROBLEM_PCT = 10


def _percentile(sorted_vals: list[float], p: float) -> float:
  """Compute the p-th percentile from a sorted list of values."""
  if not sorted_vals:
    return 0.0
  k = (len(sorted_vals) - 1) * (p / 100)
  lo = int(k)
  hi = min(lo + 1, len(sorted_vals) - 1)
  frac = k - lo
  return sorted_vals[lo] + frac * (sorted_vals[hi] - sorted_vals[lo])


def _status(fmt: Formatter, label: str, value: str, level: str) -> str:
  """Format a single health line with status marker."""
  if level == "healthy":
    marker = fmt.fg("✓", fmt.theme.healthy)
  elif level == "warning":
    marker = fmt.fg("⚠", fmt.theme.warning)
  else:
    marker = fmt.fg("✗", fmt.theme.problem)
  label_display = label.ljust(22)
  return f"  {marker} {fmt.fg(label_display, fmt.theme.label)} {value}"


def run(db: Connection, fmt: Formatter) -> None:
  """Run the one-page health overview."""
  print(fmt.header("Health Summary"))
  print()

  # ── Health Overview ────────────────────────────────
  print(fmt.section("Health Overview"))
  print()

  # Images: avg download time
  img_row = db.execute(
    f"SELECT COUNT(*), AVG({_IMG_DUR})"
    f" FROM image_downloads"
    f" WHERE {_IMG_VALID}"
  ).fetchone()
  img_count = img_row[0] or 0
  img_avg = img_row[1]

  if img_count > 0 and img_avg is not None:
    if img_avg >= IMG_AVG_PROBLEM_MS:
      level = "problem"
    elif img_avg >= IMG_AVG_WARN_MS:
      level = "warning"
    else:
      level = "healthy"
    print(_status(
      fmt, "Images",
      f"{img_count} downloads, avg {fmt.duration(img_avg)}",
      level,
    ))
  else:
    print(_status(fmt, "Images", "no data", "healthy"))

  # Network: total requests, p95 latency
  net_total = db.execute(
    "SELECT COUNT(*) FROM rest_requests"
  ).fetchone()[0]

  lat_rows = db.execute(
    "SELECT elapsed_ms FROM rest_requests"
    " WHERE elapsed_ms IS NOT NULL AND elapsed_ms >= 0"
    " ORDER BY elapsed_ms"
  ).fetchall()
  lat_values = [r[0] for r in lat_rows]

  if lat_values:
    lat_values.sort()
    p95 = _percentile(lat_values, 95)
    if p95 >= NET_P95_PROBLEM_MS:
      level = "problem"
    elif p95 >= NET_P95_WARN_MS:
      level = "warning"
    else:
      level = "healthy"
    print(_status(
      fmt, "Network",
      f"{net_total} requests, p95 {fmt.duration(p95)}",
      level,
    ))
  elif net_total > 0:
    print(_status(fmt, "Network", f"{net_total} requests, no latency data", "warning"))
  else:
    print(_status(fmt, "Network", "no data", "healthy"))

  # Gateway: heartbeat ACK and miss counts
  hb_sent = db.execute(
    "SELECT COUNT(*) FROM gateway_events"
    " WHERE (event_type = 'sending' AND detail = 'heartbeat')"
    " OR event_type = 'heartbeat'"
  ).fetchone()[0]
  hb_ack = db.execute(
    "SELECT COUNT(*) FROM gateway_events WHERE event_type = 'heartbeat_ack'"
  ).fetchone()[0]
  hb_miss = max(0, hb_sent - hb_ack)

  if hb_sent > 0:
    if hb_miss >= HB_MISS_PROBLEM:
      level = "problem"
    elif hb_miss >= HB_MISS_WARN:
      level = "warning"
    else:
      level = "healthy"
    print(_status(
      fmt, "Gateway",
      f"{hb_ack} ACKs, {hb_miss} missed",
      level,
    ))
  else:
    gw_total = db.execute(
      "SELECT COUNT(*) FROM gateway_events"
    ).fetchone()[0]
    if gw_total > 0:
      print(_status(fmt, "Gateway", "events present, no heartbeat data", "warning"))
    else:
      print(_status(fmt, "Gateway", "no data", "healthy"))

  # Channels: count, avg first render
  ch_count = db.execute(
    "SELECT COUNT(*) FROM channel_switches"
  ).fetchone()[0]

  avg_render_row = db.execute(
    f"SELECT AVG({_RENDER_MS})"
    f" FROM channel_switches"
    f" WHERE first_render_at IS NOT NULL AND switch_at IS NOT NULL"
  ).fetchone()
  avg_render = avg_render_row[0] if avg_render_row else None

  if ch_count > 0:
    if avg_render is not None:
      if avg_render >= RENDER_PROBLEM_MS:
        level = "problem"
      elif avg_render >= RENDER_WARN_MS:
        level = "warning"
      else:
        level = "healthy"
      print(_status(
        fmt, "Channels",
        f"{ch_count} switches, avg render {fmt.duration(avg_render)}",
        level,
      ))
    else:
      print(_status(fmt, "Channels", f"{ch_count} switches, no render data", "healthy"))
  else:
    print(_status(fmt, "Channels", "no data", "healthy"))

  # Cache: hit rate
  mem_ops = db.execute(
    "SELECT operation, COUNT(*) AS cnt"
    " FROM cache_events"
    " WHERE cache_type = 'image_memory'"
    " GROUP BY operation"
  ).fetchall()

  mem_counts: dict[str, int] = {}
  for r in mem_ops:
    mem_counts[r[0]] = r[1]

  hits = mem_counts.get("hit", 0)
  misses = mem_counts.get("miss", 0)
  mem_total = hits + misses

  if mem_total > 0:
    hit_rate = hits / mem_total * 100
    if hit_rate < CACHE_HIT_PROBLEM_PCT:
      level = "problem"
    elif hit_rate < CACHE_HIT_WARN_PCT:
      level = "warning"
    else:
      level = "healthy"
    print(_status(
      fmt, "Cache",
      f"{hit_rate:.1f}% hit rate ({hits}/{mem_total})",
      level,
    ))
  else:
    print(_status(fmt, "Cache", "no memory cache data", "healthy"))

  print()

  # ── Quick Stats ────────────────────────────────────
  print(fmt.section("Quick Stats"))
  print()

  total_img = db.execute(
    "SELECT COUNT(*) FROM image_downloads"
  ).fetchone()[0]
  total_gw = db.execute(
    "SELECT COUNT(*) FROM gateway_events"
  ).fetchone()[0]
  total_cache = db.execute(
    "SELECT COUNT(*) FROM cache_events"
  ).fetchone()[0]

  print(f"  Image downloads:   {fmt.bold(str(total_img))}")
  print(f"  REST requests:     {fmt.bold(str(net_total))}")
  print(f"  Gateway events:    {fmt.bold(str(total_gw))}")
  print(f"  Channel switches:  {fmt.bold(str(ch_count))}")
  print(f"  Cache events:      {fmt.bold(str(total_cache))}")
  print()
