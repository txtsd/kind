"""Cross-silo analyzer: CDN domain reliability and health."""

from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Thresholds
AVG_WARN_MS = 1000
AVG_PROBLEM_MS = 2000
MAX_AVG_RATIO_WARN = 5.0
MAX_AVG_RATIO_PROBLEM = 10.0

FIRST_PARTY_DOMAINS = frozenset({
  "cdn.discordapp.com",
  "media.discordapp.net",
  "images-ext-1.discordapp.net",
  "images-ext-2.discordapp.net",
  "discord.com",
})

# SQL expression for download duration in milliseconds.
_DUR = (
  "(JULIANDAY(download_finished_at) - JULIANDAY(download_started_at))"
  " * 86400000"
)

# Base WHERE clause for rows with valid download durations.
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
  """Run the CDN health and domain reliability analysis."""
  total = db.execute(
    f"SELECT COUNT(*) FROM image_downloads WHERE domain IS NOT NULL AND {_VALID}"
  ).fetchone()[0]

  if total == 0:
    print(fmt.header("CDN Health", "No image download data found"))
    return

  row = db.execute(
    "SELECT MIN(download_started_at), MAX(download_finished_at)"
    " FROM image_downloads"
    f" WHERE {_VALID}"
  ).fetchone()
  time_start = row[0] if row else None
  time_end = row[1] if row else None

  period = ""
  if time_start and time_end:
    period = f"{time_start} to {time_end}"

  print(fmt.header(
    "CDN Health",
    f"Period: {period}" if period else "",
    f"{total} downloads across domains",
  ))
  print()

  # ── Domain Reliability Ranking ───────────────────────
  print(fmt.section("Domain Reliability Ranking"))
  print()

  # Gather per-domain stats. We compute percentiles in Python since
  # SQLite lacks window-function percentile support out of the box.
  domains = db.execute(
    f"SELECT domain, COUNT(*) AS cnt, AVG({_DUR}) AS avg_dur, MAX({_DUR}) AS max_dur"
    f" FROM image_downloads"
    f" WHERE domain IS NOT NULL AND {_VALID}"
    f" GROUP BY domain"
    f" ORDER BY cnt DESC"
  ).fetchall()

  domain_stats: list[dict] = []
  for d in domains:
    domain = d[0]
    count = d[1]
    avg_ms = d[2] or 0
    max_ms = d[3] or 0

    # Fetch all durations for percentile calculation
    dur_rows = db.execute(
      f"SELECT {_DUR} AS dur FROM image_downloads"
      f" WHERE domain = ? AND {_VALID}"
      f" ORDER BY dur",
      (domain,),
    ).fetchall()
    dur_values = sorted(r[0] for r in dur_rows if r[0] is not None and r[0] >= 0)

    p50 = _percentile(dur_values, 50) if dur_values else 0
    p95 = _percentile(dur_values, 95) if dur_values else 0
    max_avg_ratio = (max_ms / avg_ms) if avg_ms > 0 else 0

    domain_stats.append({
      "domain": domain,
      "count": count,
      "avg": avg_ms,
      "p50": p50,
      "p95": p95,
      "max": max_ms,
      "ratio": max_avg_ratio,
      "durations": dur_values,
    })

  # Sort by reliability: low max/avg ratio first (most reliable)
  domain_stats.sort(key=lambda s: s["ratio"])

  if domain_stats:
    max_p95 = max((s["p95"] for s in domain_stats), default=1)

    headers = ["Domain", "Count", "Avg", "p50", "p95", "Max", "Max/Avg", ""]
    rows = []
    for s in domain_stats:
      domain_display = s["domain"][:28]
      ratio_str = f"{s['ratio']:.1f}x"

      # Color the ratio
      if s["ratio"] >= MAX_AVG_RATIO_PROBLEM:
        ratio_str = fmt.fg(ratio_str, fmt.theme.problem)
      elif s["ratio"] >= MAX_AVG_RATIO_WARN:
        ratio_str = fmt.fg(ratio_str, fmt.theme.warning)

      bar = fmt.bar(s["p95"], max_p95, width=12)

      rows.append([
        domain_display,
        str(s["count"]),
        fmt.duration(s["avg"]),
        fmt.duration(s["p50"]),
        fmt.duration(s["p95"]),
        fmt.duration(s["max"]),
        ratio_str,
        bar,
      ])

    print(fmt.table(headers, rows, ["l", "r", "r", "r", "r", "r", "r", "l"]))
    print()

    # Sparklines for domains with enough samples
    has_sparklines = False
    for s in domain_stats:
      if s["count"] < 8:
        continue
      spark = fmt.sparkline(s["durations"])
      if spark:
        has_sparklines = True
        domain_display = s["domain"][:30].ljust(30)
        print(f"  {fmt.fg(domain_display, fmt.theme.label)} {spark}")

    if has_sparklines:
      print()

  # ── First-Party vs Third-Party ─────────────────────────
  print(fmt.section("First-Party vs Third-Party"))
  print()

  fp_stats = {"count": 0, "durations": []}
  tp_stats = {"count": 0, "durations": []}

  for s in domain_stats:
    is_first_party = s["domain"] in FIRST_PARTY_DOMAINS
    target = fp_stats if is_first_party else tp_stats
    target["count"] += s["count"]
    target["durations"].extend(s["durations"])

  for label, stats in [("First-party", fp_stats), ("Third-party", tp_stats)]:
    count = stats["count"]
    durs = sorted(stats["durations"])
    if not durs:
      print(f"  {fmt.bold(label)}: no data")
      continue
    avg = sum(durs) / len(durs)
    p50 = _percentile(durs, 50)
    p95 = _percentile(durs, 95)
    print(
      f"  {fmt.bold(label):18s}  "
      f"n={fmt.fg(str(count), fmt.theme.value)}  "
      f"avg={fmt.bold(fmt.duration(avg))}  "
      f"p50={fmt.bold(fmt.duration(p50))}  "
      f"p95={fmt.bold(fmt.duration(p95))}"
    )

  print()

  # ── Suggestions ────────────────────────────────────────
  print(fmt.section("Suggestions"))
  print()

  # Slow third-party CDNs (require 3+ downloads to avoid noise from one-off outliers)
  slow_tp = [
    s for s in domain_stats
    if s["domain"] not in FIRST_PARTY_DOMAINS and s["avg"] >= AVG_WARN_MS and s["count"] >= 3
  ]
  slow_tp.sort(key=lambda s: s["avg"], reverse=True)

  for s in slow_tp[:5]:
    if s["avg"] >= AVG_PROBLEM_MS:
      print(fmt.problem(
        f"{s['domain']}: avg {fmt.duration(s['avg'])} "
        f"across {s['count']} downloads (very slow third-party CDN)"
      ))
    else:
      print(fmt.warning(
        f"{s['domain']}: avg {fmt.duration(s['avg'])} "
        f"across {s['count']} downloads (slow third-party CDN)"
      ))

  # Unreliable domains (high max/avg ratio)
  unreliable = [
    s for s in domain_stats
    if s["ratio"] >= MAX_AVG_RATIO_WARN and s["count"] >= 3
  ]
  unreliable.sort(key=lambda s: s["ratio"], reverse=True)

  for s in unreliable[:5]:
    if s["ratio"] >= MAX_AVG_RATIO_PROBLEM:
      print(fmt.problem(
        f"{s['domain']}: max/avg ratio {s['ratio']:.1f}x "
        f"(highly inconsistent, occasional extreme outliers)"
      ))
    else:
      print(fmt.warning(
        f"{s['domain']}: max/avg ratio {s['ratio']:.1f}x "
        f"(inconsistent response times)"
      ))

  # First-party health
  fp_durs = sorted(fp_stats["durations"])
  if fp_durs:
    fp_avg = sum(fp_durs) / len(fp_durs)
    if fp_avg >= AVG_PROBLEM_MS:
      print(fmt.problem(
        f"Discord CDN: avg {fmt.duration(fp_avg)} (very slow, may indicate network issues)"
      ))
    elif fp_avg >= AVG_WARN_MS:
      print(fmt.warning(
        f"Discord CDN: avg {fmt.duration(fp_avg)} (slow)"
      ))
    else:
      print(fmt.healthy(
        f"Discord CDN: avg {fmt.duration(fp_avg)} across {fp_stats['count']} downloads"
      ))

  if not slow_tp and not unreliable:
    print(fmt.healthy("All domains performing within expected parameters"))

  print()
