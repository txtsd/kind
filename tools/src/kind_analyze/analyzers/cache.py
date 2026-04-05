"""Cache health analyzer: hit/miss/eviction ratios and disk cache stats."""

from sqlite3 import Connection

from kind_analyze.fmt import Formatter

# Thresholds
HIT_RATE_WARN_PCT = 40
HIT_RATE_PROBLEM_PCT = 10


def run(db: Connection, fmt: Formatter) -> None:
  """Run the cache health analysis."""
  total = db.execute("SELECT COUNT(*) FROM cache_events").fetchone()[0]

  if total == 0:
    print(fmt.header("Cache Analysis", "No cache event data found"))
    return

  # Time range
  row = db.execute(
    "SELECT MIN(timestamp), MAX(timestamp) FROM cache_events"
  ).fetchone()
  time_start = row[0] if row else None
  time_end = row[1] if row else None

  period = ""
  if time_start and time_end:
    period = f"{time_start} to {time_end}"

  print(fmt.header("Cache Analysis", f"Period: {period}" if period else ""))
  print()

  # ── Image Memory Cache ─────────────────────────────
  print(fmt.section("Image Memory Cache"))
  print()

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
  evicts = mem_counts.get("evict", 0)
  mem_total = hits + misses

  if mem_total > 0:
    hit_rate = hits / mem_total * 100
    bar = fmt.bar(hits, mem_total, width=18)

    print(f"  Hits:    {fmt.bold(str(hits))}")
    print(f"  Misses:  {fmt.bold(str(misses))}")
    print(f"  Evicts:  {fmt.bold(str(evicts))}")
    print(f"  Hit rate: {fmt.bold(f'{hit_rate:.1f}%')}  {bar}")
  else:
    hit_rate = 0.0
    print(f"  {fmt.fg('No image memory cache events found', fmt.theme.muted)}")

  print()

  # ── Image Request Resolution ──────────────────────
  print(fmt.section("Image Request Resolution"))
  print()

  total_requests = db.execute(
    "SELECT COUNT(*) FROM image_downloads"
  ).fetchone()[0]
  # Served from cache: requested but never started a network download
  cache_served = db.execute(
    "SELECT COUNT(*) FROM image_downloads WHERE download_started_at IS NULL"
  ).fetchone()[0]
  # Started a network download
  network_started = db.execute(
    "SELECT COUNT(*) FROM image_downloads WHERE download_started_at IS NOT NULL"
  ).fetchone()[0]
  # Completed network downloads (have finished timestamp)
  network_completed = db.execute(
    "SELECT COUNT(*) FROM image_downloads WHERE download_finished_at IS NOT NULL"
  ).fetchone()[0]
  total_304 = db.execute(
    "SELECT COUNT(*) FROM image_downloads WHERE was_304 = 1"
  ).fetchone()[0]
  # Fresh completed downloads (not 304) need saving to disk
  fresh_completed = network_completed - total_304
  saved_downloads = db.execute(
    "SELECT COUNT(*) FROM image_downloads WHERE saved_at IS NOT NULL"
  ).fetchone()[0]
  network_interrupted = network_started - network_completed

  # Disk cache hit count from cache_events
  disk_hits = db.execute(
    "SELECT COUNT(*) FROM cache_events"
    " WHERE cache_type = 'image_disk' AND operation = 'hit'"
  ).fetchone()[0]

  if total_requests > 0:
    print(f"  Total requests:     {fmt.bold(str(total_requests))}")
    print(f"  Served from cache:  {fmt.bold(str(cache_served))}")
    if disk_hits > 0:
      print(f"    Disk hits:        {fmt.bold(str(disk_hits))}")
    if network_started > 0:
      print(f"  Network downloads:  {fmt.bold(str(network_started))}")
      if total_304 > 0:
        print(f"    304 Not Modified: {fmt.bold(str(total_304))}")
      print(f"    Completed:        {fmt.bold(str(network_completed))}")
      if network_interrupted > 0:
        print(f"    Interrupted:      {fmt.bold(str(network_interrupted))}")
      print(f"    Saved to disk:    {fmt.bold(str(saved_downloads))}")
      if fresh_completed > 0:
        save_rate = saved_downloads / fresh_completed * 100
        bar = fmt.bar(saved_downloads, fresh_completed, width=18)
        print(f"    Save rate: {fmt.bold(f'{save_rate:.1f}%')}  {bar}")
  else:
    print(f"  {fmt.fg('No image request data found', fmt.theme.muted)}")

  print()

  # ── Database Activity ──────────────────────────────
  print(fmt.section("Database Activity"))
  print()

  db_ops = db.execute(
    "SELECT operation, COUNT(*) AS cnt"
    " FROM cache_events"
    " WHERE cache_type = 'database'"
    " GROUP BY operation"
    " ORDER BY cnt DESC"
  ).fetchall()

  if db_ops:
    max_count = max(r[1] for r in db_ops)
    for r in db_ops:
      op = r[0]
      count = r[1]
      bar = fmt.bar(count, max_count, width=18)
      op_display = op.ljust(12)
      print(
        f"  {fmt.fg(op_display, fmt.theme.label)} "
        f"{count:>6d}  {bar}"
      )
  else:
    print(f"  {fmt.fg('No database cache events found', fmt.theme.muted)}")

  print()

  # ── Suggestions ────────────────────────────────────
  print(fmt.section("Suggestions"))
  print()

  if mem_total > 0:
    if hit_rate < HIT_RATE_PROBLEM_PCT:
      print(fmt.problem(
        f"Memory cache hit rate: {hit_rate:.1f}% "
        f"(very low, most lookups are misses)"
      ))
    elif hit_rate < HIT_RATE_WARN_PCT:
      print(fmt.warning(
        f"Memory cache hit rate: {hit_rate:.1f}% "
        f"(consider increasing memory cache size)"
      ))
    else:
      print(fmt.healthy(
        f"Memory cache hit rate: {hit_rate:.1f}%"
      ))
  else:
    print(fmt.fg(
      "  No memory cache data to assess",
      fmt.theme.muted,
    ))

  if evicts > 0 and mem_total > 0:
    evict_ratio = evicts / mem_total * 100
    if evict_ratio > 30:
      print(fmt.warning(
        f"Eviction pressure: {evict_ratio:.1f}% of lookups trigger evictions, "
        f"cache may be undersized"
      ))

  if fresh_completed > 0 and saved_downloads < fresh_completed:
    unsaved = fresh_completed - saved_downloads
    save_rate_pct = saved_downloads / fresh_completed * 100
    if save_rate_pct < 50:
      print(fmt.warning(
        f"Disk cache: {unsaved} completed downloads not persisted ({save_rate_pct:.1f}% save rate)"
      ))

  if network_interrupted > 0:
    print(fmt.warning(
      f"{network_interrupted} download(s) interrupted (app exited before completion)"
    ))

  print()
