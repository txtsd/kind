"""Channel switch latency and dwell time analyzer."""

from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Thresholds
FIRST_IMAGE_WARN_MS = 1000
FIRST_IMAGE_PROBLEM_MS = 2000

# SQL expressions for durations in milliseconds via JULIANDAY subtraction.
_RENDER_MS = (
  "(JULIANDAY(first_render_at) - JULIANDAY(switch_at))"
  " * 86400000"
)
_REST_MS = (
  "(JULIANDAY(rest_arrival_at) - JULIANDAY(switch_at))"
  " * 86400000"
)
_FIRST_IMAGE_MS = (
  "(JULIANDAY(first_image_ready_at) - JULIANDAY(switch_at))"
  " * 86400000"
)


def run(db: Connection, fmt: Formatter) -> None:
  """Run the channel switch latency and dwell time analysis."""
  total = db.execute("SELECT COUNT(*) FROM channel_switches").fetchone()[0]

  if total == 0:
    print(fmt.header("Channel Analysis", "No channel switch data found"))
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
    "Channel Analysis",
    f"Period: {period}" if period else "",
    f"{total} channel switches",
  ))
  print()

  # ── Switch Latency Breakdown ─────────────────────────
  print(fmt.section("Switch Latency Breakdown"))
  print()

  switches = db.execute(
    f"SELECT"
    f"  channel_id,"
    f"  initial_message_count,"
    f"  CASE WHEN first_render_at IS NOT NULL AND switch_at IS NOT NULL"
    f"    THEN {_RENDER_MS} END AS render_ms,"
    f"  CASE WHEN rest_arrival_at IS NOT NULL AND switch_at IS NOT NULL"
    f"    THEN {_REST_MS} END AS rest_ms,"
    f"  CASE WHEN first_image_ready_at IS NOT NULL AND switch_at IS NOT NULL"
    f"    THEN {_FIRST_IMAGE_MS} END AS img_ms,"
    f"  image_ready_count,"
    f"  dwell_ms"
    f" FROM channel_switches"
    f" ORDER BY switch_at"
  ).fetchall()

  headers = [
    "Channel", "Msgs", "Render", "REST", "1st Image", "Imgs", "Dwell",
  ]
  rows = []
  for sw in switches:
    ch_id = sw[0] or "?"
    ch_display = f"..{ch_id[-4:]}" if len(ch_id) > 4 else ch_id
    msgs = str(sw[1]) if sw[1] is not None else "-"
    render = fmt.duration(sw[2]) if sw[2] is not None and sw[2] >= 0 else "-"
    rest = fmt.duration(sw[3]) if sw[3] is not None and sw[3] >= 0 else "-"
    img = fmt.duration(sw[4]) if sw[4] is not None and sw[4] >= 0 else "-"
    img_count = str(sw[5]) if sw[5] is not None else "0"
    dwell = fmt.duration(sw[6]) if sw[6] is not None and sw[6] >= 0 else "-"
    rows.append([ch_display, msgs, render, rest, img, img_count, dwell])

  if rows:
    print(fmt.table(headers, rows, ["l", "r", "r", "r", "r", "r", "r"]))
    print()

  # ── Averages ─────────────────────────────────────────
  print(fmt.section("Averages"))
  print()

  avg_row = db.execute(
    f"SELECT"
    f"  AVG(CASE WHEN first_render_at IS NOT NULL"
    f"    THEN {_RENDER_MS} END),"
    f"  AVG(CASE WHEN rest_arrival_at IS NOT NULL"
    f"    THEN {_REST_MS} END),"
    f"  AVG(CASE WHEN first_image_ready_at IS NOT NULL"
    f"    THEN {_FIRST_IMAGE_MS} END),"
    f"  AVG(dwell_ms)"
    f" FROM channel_switches"
  ).fetchone()

  avg_render = avg_row[0]
  avg_rest = avg_row[1]
  avg_img = avg_row[2]
  avg_dwell = avg_row[3]

  if avg_render is not None:
    print(f"  Avg render time:      {fmt.bold(fmt.duration(avg_render))}")
  if avg_rest is not None:
    print(f"  Avg REST arrival:     {fmt.bold(fmt.duration(avg_rest))}")
  if avg_img is not None:
    print(f"  Avg first image:      {fmt.bold(fmt.duration(avg_img))}")
  if avg_dwell is not None:
    print(f"  Avg dwell time:       {fmt.bold(fmt.duration(avg_dwell))}")

  print()

  # ── Suggestions ──────────────────────────────────────
  print(fmt.section("Suggestions"))
  print()

  # DB render speed
  if avg_render is not None:
    if avg_render < 100:
      print(fmt.healthy(
        f"DB render: avg {fmt.duration(avg_render)} (fast)"
      ))
    elif avg_render < 500:
      print(fmt.warning(
        f"DB render: avg {fmt.duration(avg_render)} (could be faster)"
      ))
    else:
      print(fmt.problem(
        f"DB render: avg {fmt.duration(avg_render)} (slow, check message store)"
      ))

  # First image speed
  if avg_img is not None:
    if avg_img >= FIRST_IMAGE_PROBLEM_MS:
      print(fmt.problem(
        f"First image: avg {fmt.duration(avg_img)} "
        f"(slow, images take over 2s to appear)"
      ))
    elif avg_img >= FIRST_IMAGE_WARN_MS:
      print(fmt.warning(
        f"First image: avg {fmt.duration(avg_img)} "
        f"(images take over 1s to appear)"
      ))
    else:
      print(fmt.healthy(
        f"First image: avg {fmt.duration(avg_img)}"
      ))

  # Cold cache detection: switches where first image is very slow
  # compared to the average suggest cache misses
  if avg_img is not None and avg_img > 0:
    cold_count = db.execute(
      f"SELECT COUNT(*) FROM channel_switches"
      f" WHERE first_image_ready_at IS NOT NULL"
      f"   AND {_FIRST_IMAGE_MS} > ?",
      (FIRST_IMAGE_PROBLEM_MS,),
    ).fetchone()[0]
    img_switches = db.execute(
      "SELECT COUNT(*) FROM channel_switches"
      " WHERE first_image_ready_at IS NOT NULL"
    ).fetchone()[0]

    if img_switches > 0:
      cold_pct = cold_count / img_switches * 100
      if cold_pct > 50:
        print(fmt.problem(
          f"Cold cache: {cold_count}/{img_switches} switches "
          f"({cold_pct:.0f}%) had first image > 2s "
          f"(image cache may not be persisting)"
        ))
      elif cold_pct > 20:
        print(fmt.warning(
          f"Cold cache: {cold_count}/{img_switches} switches "
          f"({cold_pct:.0f}%) had first image > 2s"
        ))
      elif cold_count > 0:
        print(fmt.healthy(
          f"Cold cache: {cold_count}/{img_switches} switches "
          f"({cold_pct:.0f}%) had first image > 2s (occasional, normal)"
        ))

  print()
