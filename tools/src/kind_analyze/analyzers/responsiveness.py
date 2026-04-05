"""Cross-silo analyzer: channel switch responsiveness vs image pipeline."""

from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Thresholds
IN_FLIGHT_WARN = 3
IN_FLIGHT_PROBLEM = 5

# SQL expression for completion time (last image ready minus switch time).
_COMPLETION_MS = (
  "(JULIANDAY(cs.last_image_ready_at) - JULIANDAY(cs.switch_at))"
  " * 86400000"
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
  """Run the responsiveness analysis: channel switches vs image pipeline."""
  total = db.execute("SELECT COUNT(*) FROM channel_switches").fetchone()[0]

  if total == 0:
    print(fmt.header("Responsiveness Analysis", "No channel switch data found"))
    return

  row = db.execute(
    "SELECT MIN(switch_at), MAX(switch_at) FROM channel_switches"
    " WHERE switch_at IS NOT NULL"
  ).fetchone()
  time_start = row[0] if row else None
  time_end = row[1] if row else None

  period = ""
  if time_start and time_end:
    period = f"{time_start} to {time_end}"

  print(fmt.header(
    "Responsiveness Analysis",
    f"Period: {period}" if period else "",
    f"{total} channel switches",
  ))
  print()

  # ── Switch Pipeline ──────────────────────────────────
  print(fmt.section("Switch Pipeline"))
  print()

  switches = db.execute(
    f"SELECT cs.channel_id, cs.dwell_ms,"
    f"  CASE WHEN cs.last_image_ready_at IS NOT NULL"
    f"    THEN {_COMPLETION_MS} END AS completion_ms,"
    f"  cs.image_ready_count,"
    f"  (SELECT COUNT(*) FROM image_downloads id"
    f"   WHERE id.download_started_at < cs.switch_at"
    f"   AND id.download_started_at >= ("
    f"     SELECT COALESCE(MAX(ge.timestamp), '1970-01-01')"
    f"     FROM gateway_events ge"
    f"     WHERE ge.event_type = 'connecting'"
    f"     AND ge.timestamp <= cs.switch_at"
    f"   )"
    f"   AND (id.download_finished_at IS NULL"
    f"        OR id.download_finished_at > cs.switch_at)"
    f"  ) AS in_flight_at_switch"
    f" FROM channel_switches cs"
    f" ORDER BY cs.switch_at"
  ).fetchall()

  headers = ["Channel", "Dwell", "Completion", "Images", "In-flight"]
  rows = []
  dwell_faster_count = 0
  total_with_completion = 0

  for sw in switches:
    ch_id = sw[0] or "?"
    ch_display = f"..{ch_id[-4:]}" if len(ch_id) > 4 else ch_id
    dwell = sw[1]
    completion = sw[2]
    img_count = sw[3] if sw[3] is not None else 0
    in_flight = sw[4] if sw[4] is not None else 0

    dwell_str = fmt.duration(dwell) if dwell is not None and dwell >= 0 else "-"
    completion_str = fmt.duration(completion) if completion is not None and completion >= 0 else "-"
    img_str = str(img_count)
    in_flight_str = str(in_flight)

    # Color in-flight if high
    if in_flight >= IN_FLIGHT_PROBLEM:
      in_flight_str = fmt.fg(in_flight_str, fmt.theme.problem)
    elif in_flight >= IN_FLIGHT_WARN:
      in_flight_str = fmt.fg(in_flight_str, fmt.theme.warning)

    # Check if dwell < completion (switching faster than loading)
    if dwell is not None and completion is not None and dwell > 0 and completion > 0:
      total_with_completion += 1
      if dwell < completion:
        dwell_faster_count += 1
        dwell_str = fmt.fg(dwell_str, fmt.theme.warning)

    rows.append([ch_display, dwell_str, completion_str, img_str, in_flight_str])

  if rows:
    print(fmt.table(headers, rows, ["l", "r", "r", "r", "r"]))
    print()

  # ── Contention Analysis ────────────────────────────────
  print(fmt.section("Contention Analysis"))
  print()

  in_flight_values = [sw[4] for sw in switches if sw[4] is not None]
  warn_count = sum(1 for v in in_flight_values if v >= IN_FLIGHT_WARN)
  problem_count = sum(1 for v in in_flight_values if v >= IN_FLIGHT_PROBLEM)

  print(f"  Switches with in-flight > {IN_FLIGHT_WARN}: {fmt.bold(str(warn_count))}")
  print(f"  Switches with in-flight > {IN_FLIGHT_PROBLEM}: {fmt.bold(str(problem_count))}")

  if in_flight_values:
    avg_in_flight = sum(in_flight_values) / len(in_flight_values)
    max_in_flight = max(in_flight_values)
    print(f"  Avg in-flight at switch:  {fmt.bold(f'{avg_in_flight:.1f}')}")
    print(f"  Max in-flight at switch:  {fmt.bold(str(max_in_flight))}")

  print()

  # ── Suggestions ────────────────────────────────────────
  print(fmt.section("Suggestions"))
  print()

  # Switching speed vs load completion
  if total_with_completion > 0:
    faster_pct = dwell_faster_count / total_with_completion * 100
    if faster_pct > 50:
      print(fmt.problem(
        f"{dwell_faster_count}/{total_with_completion} switches "
        f"({faster_pct:.0f}%) had dwell shorter than image completion "
        f"(switching much faster than loading)"
      ))
    elif faster_pct > 20:
      print(fmt.warning(
        f"{dwell_faster_count}/{total_with_completion} switches "
        f"({faster_pct:.0f}%) had dwell shorter than image completion"
      ))
    else:
      print(fmt.healthy(
        f"{dwell_faster_count}/{total_with_completion} switches "
        f"({faster_pct:.0f}%) had dwell shorter than image completion"
      ))
  elif total > 0:
    print(fmt.healthy("No switches had both dwell and completion data to compare"))

  # Queue contention
  if problem_count > 0:
    print(fmt.problem(
      f"{problem_count} switch(es) had > {IN_FLIGHT_PROBLEM} downloads in flight "
      f"(heavy queue contention, images from prior channels still downloading)"
    ))
  elif warn_count > 0:
    print(fmt.warning(
      f"{warn_count} switch(es) had > {IN_FLIGHT_WARN} downloads in flight "
      f"(moderate queue contention)"
    ))
  else:
    print(fmt.healthy("No queue contention detected at switch time"))

  print()
