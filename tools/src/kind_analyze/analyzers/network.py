"""REST API network performance analyzer."""

from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Thresholds
REST_WARN_P95_MS = 1500
REST_PROBLEM_P95_MS = 3000
ERROR_WARN_PCT = 5
ERROR_PROBLEM_PCT = 15


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
  """Run the REST API network performance analysis."""
  # Time range
  row = db.execute(
    "SELECT MIN(timestamp), MAX(timestamp)"
    " FROM rest_requests"
    " WHERE timestamp IS NOT NULL"
  ).fetchone()
  time_start = row[0] if row else None
  time_end = row[1] if row else None

  total = db.execute("SELECT COUNT(*) FROM rest_requests").fetchone()[0]

  if total == 0:
    print(fmt.header("Network Analysis", "No REST request data found"))
    return

  period = ""
  if time_start and time_end:
    period = f"{time_start} to {time_end}"

  print(fmt.header("Network Analysis", f"Period: {period}" if period else ""))
  print()

  # ── Request Overview ───────────────────────────────────
  print(fmt.section("Request Overview"))
  print()

  error_count = db.execute(
    "SELECT COUNT(*) FROM rest_requests"
    " WHERE status_code >= 400 OR status_code IS NULL"
  ).fetchone()[0]
  error_rate = (error_count / total * 100) if total > 0 else 0

  print(f"  Total requests: {fmt.bold(str(total))}")
  print(f"  Errors:         {fmt.bold(str(error_count))} ({error_rate:.1f}%)")
  print()

  # Method breakdown
  method_rows = db.execute(
    "SELECT COALESCE(method, '?') AS m, COUNT(*) AS cnt"
    " FROM rest_requests"
    " GROUP BY m"
    " ORDER BY cnt DESC"
  ).fetchall()

  if method_rows:
    max_count = max(r[1] for r in method_rows)
    for row in method_rows:
      method = row[0]
      count = row[1]
      bar = fmt.bar(count, max_count, width=18)
      method_display = method.ljust(8)
      print(
        f"  {fmt.fg(method_display, fmt.theme.label)} "
        f"{count:>6d}  {bar}"
      )
    print()

  # ── Latency by Endpoint ────────────────────────────────
  print(fmt.section("Latency by Endpoint"))
  print()

  endpoint_rows = db.execute(
    "SELECT COALESCE(path, '?') AS p, COUNT(*) AS cnt,"
    " AVG(elapsed_ms) AS avg_ms, MAX(elapsed_ms) AS max_ms"
    " FROM rest_requests"
    " WHERE elapsed_ms IS NOT NULL"
    " GROUP BY p"
    " ORDER BY cnt DESC"
    " LIMIT 15"
  ).fetchall()

  if endpoint_rows:
    headers = ["Endpoint", "Count", "Avg", "Max"]
    rows = []
    for row in endpoint_rows:
      path = row[0]
      # Truncate long paths
      if len(path) > 40:
        path = path[:37] + "..."
      count = row[1]
      avg_ms = row[2] or 0
      max_ms = row[3] or 0
      rows.append([
        path,
        str(count),
        fmt.duration(avg_ms),
        fmt.duration(max_ms),
      ])

    print(fmt.table(headers, rows, ["l", "r", "r", "r"]))
    print()

  # ── Latency Distribution ───────────────────────────────
  all_latencies = db.execute(
    "SELECT elapsed_ms FROM rest_requests"
    " WHERE elapsed_ms IS NOT NULL AND elapsed_ms >= 0"
    " ORDER BY elapsed_ms"
  ).fetchall()
  lat_values = [r[0] for r in all_latencies]

  if lat_values:
    lat_values.sort()
    p50 = _percentile(lat_values, 50)
    p95 = _percentile(lat_values, 95)
    spark = fmt.sparkline(lat_values)
    if spark:
      print(f"  Distribution: {spark}")
    print(
      f"  p50: {fmt.bold(fmt.duration(p50))}  "
      f"p95: {fmt.bold(fmt.duration(p95))}"
    )
    print()

  # ── Error Breakdown ────────────────────────────────────
  if error_count > 0:
    print(fmt.section("Error Breakdown"))
    print()

    error_rows = db.execute(
      "SELECT COALESCE(status_code, 0) AS sc,"
      " COALESCE(method, '?') AS m,"
      " COALESCE(path, '?') AS p,"
      " COUNT(*) AS cnt"
      " FROM rest_requests"
      " WHERE status_code >= 400 OR status_code IS NULL"
      " GROUP BY sc, m, p"
      " ORDER BY cnt DESC"
      " LIMIT 20"
    ).fetchall()

    if error_rows:
      headers = ["Status", "Method", "Path", "Count"]
      rows = []
      for row in error_rows:
        status = str(row[0]) if row[0] else "?"
        method = row[1]
        path = row[2]
        if len(path) > 35:
          path = path[:32] + "..."
        count = row[3]
        rows.append([status, method, path, str(count)])

      print(fmt.table(headers, rows, ["l", "l", "l", "r"]))
      print()

  # ── Rate Limits ────────────────────────────────────────
  rate_limit_count = db.execute(
    "SELECT COUNT(*) FROM rate_limits"
  ).fetchone()[0]

  if rate_limit_count > 0:
    print(fmt.section("Rate Limits"))
    print()

    rl_rows = db.execute(
      "SELECT COALESCE(route, '?') AS r, COUNT(*) AS cnt,"
      " AVG(retry_after_ms) AS avg_delay, MAX(retry_after_ms) AS max_delay"
      " FROM rate_limits"
      " GROUP BY r"
      " ORDER BY cnt DESC"
      " LIMIT 20"
    ).fetchall()

    if rl_rows:
      headers = ["Route", "Count", "Avg Delay", "Max Delay"]
      rows = []
      for row in rl_rows:
        route = row[0]
        if len(route) > 60:
          route = route[:57] + "..."
        count = row[1]
        avg_delay = row[2] or 0
        max_delay = row[3] or 0
        rows.append([
          route,
          str(count),
          fmt.duration(avg_delay),
          fmt.duration(max_delay),
        ])

      print(fmt.table(headers, rows, ["l", "r", "r", "r"]))
      print()

  # ── Suggestions ────────────────────────────────────────
  print(fmt.section("Suggestions"))
  print()

  # p95 latency assessment
  if lat_values:
    p95 = _percentile(lat_values, 95)
    if p95 >= REST_PROBLEM_P95_MS:
      print(fmt.problem(
        f"REST p95 latency: {fmt.duration(p95)} (very high, "
        f"threshold: {fmt.duration(REST_PROBLEM_P95_MS)})"
      ))
    elif p95 >= REST_WARN_P95_MS:
      print(fmt.warning(
        f"REST p95 latency: {fmt.duration(p95)} (elevated, "
        f"threshold: {fmt.duration(REST_WARN_P95_MS)})"
      ))
    else:
      print(fmt.healthy(
        f"REST p95 latency: {fmt.duration(p95)}"
      ))

  # Error rate assessment
  if total > 0:
    if error_rate >= ERROR_PROBLEM_PCT:
      print(fmt.problem(
        f"Error rate: {error_rate:.1f}% ({error_count}/{total} requests failed)"
      ))
    elif error_rate >= ERROR_WARN_PCT:
      print(fmt.warning(
        f"Error rate: {error_rate:.1f}% ({error_count}/{total} requests failed)"
      ))
    else:
      print(fmt.healthy(
        f"Error rate: {error_rate:.1f}%"
      ))

  # Rate limit status
  if rate_limit_count > 0:
    global_count = db.execute(
      "SELECT COUNT(*) FROM rate_limits WHERE is_global = 1"
    ).fetchone()[0]
    if global_count > 0:
      print(fmt.problem(
        f"Hit {global_count} global rate limit(s) "
        f"({rate_limit_count} total rate limits)"
      ))
    else:
      print(fmt.warning(
        f"Hit {rate_limit_count} rate limit(s)"
      ))
  else:
    print(fmt.healthy("No rate limits encountered"))

  print()
