"""Image download and cache performance analyzer."""

import re
from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Thresholds
DOWNLOAD_WARN_MS = 1000
DOWNLOAD_PROBLEM_MS = 2000
CACHE_HIT_WARN_PCT = 40
CACHE_HIT_PROBLEM_PCT = 10
CONTENTION_WARN = 3
CONTENTION_PROBLEM = 5

SIZE_BUCKETS = [
  (0, 50, "0-50KB"),
  (50, 200, "50-200KB"),
  (200, 500, "200-500KB"),
  (500, 1000, "500KB-1MB"),
  (1000, 5000, "1-5MB"),
  (5000, 100000, "5MB+"),
]

# SQL expression for download duration in milliseconds.
_DUR = (
  "(JULIANDAY(download_finished_at) - JULIANDAY(download_started_at))"
  " * 86400000"
)

# Base WHERE clause for rows with valid (positive) download durations.
# Some rows have mismatched timestamps from log stitching, so we filter
# out anything where the finish predates the start.
_VALID = (
  "download_started_at IS NOT NULL"
  " AND download_finished_at IS NOT NULL"
  " AND download_finished_at > download_started_at"
)


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
  """Run the image download and cache performance analysis."""
  # Time range
  row = db.execute(
    "SELECT"
    " MIN(COALESCE(queued_at, download_started_at)),"
    " MAX(COALESCE(ready_at, download_finished_at, saved_at, queued_at))"
    " FROM image_downloads"
  ).fetchone()
  time_start = row[0] if row else None
  time_end = row[1] if row else None

  # Total counts
  total = db.execute("SELECT COUNT(*) FROM image_downloads").fetchone()[0]
  total_304 = db.execute(
    "SELECT COUNT(*) FROM image_downloads WHERE was_304 = 1"
  ).fetchone()[0]
  total_valid = db.execute(
    f"SELECT COUNT(*) FROM image_downloads WHERE {_VALID}"
  ).fetchone()[0]

  if total == 0:
    print(fmt.header("Image Analysis", "No image download data found"))
    return

  # Period string
  period = ""
  if time_start and time_end:
    period = f"{time_start} to {time_end}"

  print(fmt.header("Image Analysis", f"Period: {period}" if period else ""))
  print()

  # ── Download Performance ──────────────────────────────
  print(fmt.section("Download Performance"))
  print()

  rate_304 = (total_304 / total * 100) if total > 0 else 0
  print(f"  Total downloads:  {fmt.bold(str(total))}")
  print(f"  304 (Not Modified): {fmt.bold(str(total_304))} ({rate_304:.1f}%)")
  print()

  # Size bucket table
  headers = ["Range", "Count", "Avg", "Min", "Max"]
  rows = []
  for lo, hi, label in SIZE_BUCKETS:
    bucket = db.execute(
      f"SELECT COUNT(*), AVG({_DUR}), MIN({_DUR}), MAX({_DUR})"
      f" FROM image_downloads"
      f" WHERE size_kb >= ? AND size_kb < ? AND {_VALID}",
      (lo, hi),
    ).fetchone()
    count = bucket[0] or 0
    if count == 0:
      continue
    avg_ms = bucket[1] or 0
    min_ms = bucket[2] or 0
    max_ms = bucket[3] or 0
    rows.append([
      label,
      str(count),
      fmt.duration(avg_ms),
      fmt.duration(min_ms),
      fmt.duration(max_ms),
    ])

  if rows:
    print(fmt.table(headers, rows, ["l", "r", "r", "r", "r"]))
    print()

  # Overall distribution sparkline with p50/p95
  all_durations = db.execute(
    f"SELECT {_DUR} AS dur FROM image_downloads"
    f" WHERE {_VALID} ORDER BY dur"
  ).fetchall()
  dur_values = [r[0] for r in all_durations if r[0] is not None and r[0] >= 0]

  if dur_values:
    dur_values.sort()
    p50 = _percentile(dur_values, 50)
    p95 = _percentile(dur_values, 95)
    spark = fmt.sparkline(dur_values)
    if spark:
      print(f"  Distribution: {spark}")
    print(
      f"  p50: {fmt.bold(fmt.duration(p50))}  "
      f"p95: {fmt.bold(fmt.duration(p95))}"
    )
    print()

  # ── CDN Performance ───────────────────────────────────
  print(fmt.section("CDN Performance"))
  print()

  domain_rows = db.execute(
    f"SELECT domain, COUNT(*) AS cnt, AVG({_DUR}) AS avg_dur"
    f" FROM image_downloads"
    f" WHERE domain IS NOT NULL AND {_VALID}"
    f" GROUP BY domain"
    f" ORDER BY cnt DESC"
  ).fetchall()

  if domain_rows:
    max_avg = max(
      (r[2] for r in domain_rows if r[2] is not None and r[2] > 0),
      default=1,
    )

    for row in domain_rows:
      domain = row[0]
      count = row[1]
      avg_ms = row[2] or 0
      bar = fmt.bar(avg_ms, max_avg, width=18)
      avg_str = fmt.duration(avg_ms)
      domain_display = domain[:30].ljust(30)
      print(
        f"  {fmt.fg(domain_display, fmt.theme.label)} "
        f"{avg_str:>7s}  {bar}  "
        f"{fmt.fg(f'({count})', fmt.theme.muted)}"
      )

    print()

    # Per-domain sparklines for domains with 8+ samples
    has_sparklines = False
    for row in domain_rows:
      domain = row[0]
      count = row[1]
      if count < 8:
        continue

      domain_durs = db.execute(
        f"SELECT {_DUR} AS dur FROM image_downloads"
        f" WHERE domain = ? AND {_VALID}"
        f" ORDER BY download_started_at",
        (domain,),
      ).fetchall()
      vals = [r[0] for r in domain_durs if r[0] is not None and r[0] >= 0]
      spark = fmt.sparkline(vals)
      if spark:
        has_sparklines = True
        domain_display = domain[:30].ljust(30)
        print(
          f"  {fmt.fg(domain_display, fmt.theme.label)} {spark}"
        )

    if has_sparklines:
      print()

  # ── Suggestions ───────────────────────────────────────
  print(fmt.section("Suggestions"))
  print()

  # Discord CDN health
  discord_domains = [
    "cdn.discordapp.com",
    "media.discordapp.net",
  ]
  for cdn_domain in discord_domains:
    cdn_row = db.execute(
      f"SELECT COUNT(*), AVG({_DUR})"
      f" FROM image_downloads"
      f" WHERE domain = ? AND {_VALID}",
      (cdn_domain,),
    ).fetchone()
    cdn_count = cdn_row[0] or 0
    if cdn_count == 0:
      continue
    cdn_avg = cdn_row[1] or 0

    if cdn_avg >= DOWNLOAD_PROBLEM_MS:
      print(fmt.problem(
        f"{cdn_domain}: avg {fmt.duration(cdn_avg)} "
        f"across {cdn_count} downloads (very slow)"
      ))
    elif cdn_avg >= DOWNLOAD_WARN_MS:
      print(fmt.warning(
        f"{cdn_domain}: avg {fmt.duration(cdn_avg)} "
        f"across {cdn_count} downloads (slow)"
      ))
    else:
      print(fmt.healthy(
        f"{cdn_domain}: avg {fmt.duration(cdn_avg)} "
        f"across {cdn_count} downloads"
      ))

  # Slow third-party domains
  third_party = db.execute(
    f"SELECT domain, AVG({_DUR}) AS avg_dur, COUNT(*) AS cnt"
    f" FROM image_downloads"
    f" WHERE domain IS NOT NULL"
    f" AND domain NOT IN ('cdn.discordapp.com', 'media.discordapp.net')"
    f" AND {_VALID}"
    f" GROUP BY domain"
    f" HAVING cnt >= 2"
    f" ORDER BY avg_dur DESC"
    f" LIMIT 5"
  ).fetchall()

  for tp in third_party:
    tp_domain = tp[0]
    tp_avg = tp[1] or 0
    tp_count = tp[2]
    if tp_avg >= DOWNLOAD_PROBLEM_MS:
      print(fmt.problem(
        f"{tp_domain}: avg {fmt.duration(tp_avg)} "
        f"across {tp_count} downloads (very slow third-party)"
      ))
    elif tp_avg >= DOWNLOAD_WARN_MS:
      print(fmt.warning(
        f"{tp_domain}: avg {fmt.duration(tp_avg)} "
        f"across {tp_count} downloads (slow third-party)"
      ))

  # 304 rate assessment
  if total > 0:
    if rate_304 >= CACHE_HIT_WARN_PCT:
      print(fmt.healthy(
        f"304 hit rate: {rate_304:.1f}% (good cache reuse)"
      ))
    elif rate_304 >= CACHE_HIT_PROBLEM_PCT:
      print(fmt.warning(
        f"304 hit rate: {rate_304:.1f}% (consider longer cache TTL)"
      ))
    else:
      print(fmt.problem(
        f"304 hit rate: {rate_304:.1f}% (very low, most downloads are fresh)"
      ))

  # Contention check: peak concurrent downloads from log events
  if total_valid > 0:
    queue_rows = db.execute(
      "SELECT message FROM log_events"
      " WHERE subsystem = 'cache'"
      " AND message LIKE '%image downloading%'"
    ).fetchall()

    if queue_rows:
      max_active = 0
      for qr in queue_rows:
        m = re.search(r"active=(\d+)/", qr[0])
        if m:
          active = int(m.group(1))
          max_active = max(max_active, active)

      if max_active >= CONTENTION_PROBLEM:
        print(fmt.problem(
          f"Peak concurrent downloads: {max_active} (high contention)"
        ))
      elif max_active >= CONTENTION_WARN:
        print(fmt.warning(
          f"Peak concurrent downloads: {max_active} (moderate contention)"
        ))
      elif max_active > 0:
        print(fmt.healthy(
          f"Peak concurrent downloads: {max_active}"
        ))

  print()
