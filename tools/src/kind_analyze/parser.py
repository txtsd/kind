"""Parse Kind log files into SQLite database."""

import re
from datetime import datetime, timedelta
from pathlib import Path
from sqlite3 import Connection
from urllib.parse import urlparse

# Log line format: [YYYY-MM-DD HH:MM:SS.mmm] [subsystem] [level] message
# Note: message may have leading whitespace (gateway subsystem does this).
LOG_PATTERN = re.compile(
  r"\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\] "
  r"\[(\w+)\] "
  r"\[(\w+)\] "
  r"\s*(.*)"
)

# Image event patterns (adapted to actual Kind log format)
# "image request: URL (queue=N)"
IMG_REQUEST = re.compile(r"image request: (\S+) \(queue=(\d+)\)")
# "image downloading: URL (active=N/M)" or enhanced "image downloading: URL (active=N/M, queued=Q)"
IMG_DOWNLOADING = re.compile(
  r"image downloading: (\S+) \(active=(\d+)/(\d+)(?:, queued=(\d+))?\)"
)
# "image downloaded: URL (NKB, etag=..., modified=...)"
IMG_DOWNLOADED = re.compile(
  r"image downloaded: (\S+) \((\d+)KB, etag=([^,]*), modified=(.*)\)"
)
# "image saved to disk: URL"
IMG_SAVED = re.compile(r"image saved to disk: (\S+)")
# "image ready: URL"
IMG_READY = re.compile(r"image ready: (\S+)")
# "image not modified (304): URL"
IMG_304 = re.compile(r"image not modified \(304\): (\S+)")
# "image disk hit: URL"
IMG_DISK_HIT = re.compile(r"image disk hit: (\S+)")
# "image memory hit: URL" (enhanced logging)
IMG_MEMORY_HIT = re.compile(r"image memory hit: (\S+)")
# "image memory miss: URL" (enhanced logging)
IMG_MEMORY_MISS = re.compile(r"image memory miss: (\S+)")
# "image evicted: URL (cache_size=N)" (enhanced logging)
IMG_EVICTED = re.compile(r"image evicted: (\S+)")
# "image memory cache: N items, X.XMB total, added WxH (X.XKB) for URL"
IMG_MEMORY_CACHE = re.compile(
  r"image memory cache: (\d+) items, [\d.]+MB total, added .+ for (\S+)"
)
# "image decode from disk: Nms (NKB)" or "image decode from network: Nms (NKB, URL)"
IMG_DECODE = re.compile(r"image decode from (\w+): (\d+)ms \((\d+)KB")

# REST patterns (adapted to actual Kind log format)
# "-> METHOD PATH"
REST_REQUEST = re.compile(r"-> (GET|POST|PUT|PATCH|DELETE) (\S+)")
# "<- METHOD PATH STATUS (NNNms)"
REST_RESPONSE = re.compile(
  r"<- (GET|POST|PUT|PATCH|DELETE) (\S+) (\d+) \((\d+)ms\)"
)
# "<- METHOD PATH STATUS (NNNms, NNN bytes)" (future enhanced logging)
REST_RESPONSE_SIZE = re.compile(
  r"<- (GET|POST|PUT|PATCH|DELETE) (\S+) (\d+) \((\d+)ms, (\d+) bytes\)"
)
# "rate limited METHOD PATH, delaying Nms (type)"
REST_RATE_LIMIT = re.compile(
  r"rate limited (GET|POST|PUT|PATCH|DELETE) (\S+), delaying (\d+)ms"
)

# Gateway patterns (adapted to actual Kind log format, messages have leading spaces stripped)
GW_CONNECTING = re.compile(r"connecting to (\S+)")
GW_HELLO = re.compile(r"received HELLO, heartbeat interval (\d+)ms")
GW_READY = re.compile(r"READY, session_id=(\S+)")
# Matches both "heartbeat ACK received" and enhanced "heartbeat ACK received (Nms)"
GW_HEARTBEAT_ACK = re.compile(r"heartbeat ACK received(?:\s*\((\d+)ms\))?")
GW_DISPATCH = re.compile(r"dispatch event=(\w+) seq=(\d+)")
GW_SENDING = re.compile(r"sending (\w+)")
GW_WS_CONNECTED = re.compile(r"WebSocket connected")
GW_RECONNECT = re.compile(
  r"reconnect.*?attempt.*?(\d+).*?delay.*?(\d+)", re.IGNORECASE
)
GW_CLOSE = re.compile(r"close.*?code.*?(\d+)")
GW_RESUME = re.compile(r"resum", re.IGNORECASE)
GW_INVALID = re.compile(r"invalid session")
GW_HEARTBEAT_MISS = re.compile(r"heartbeat.*miss|no ACK", re.IGNORECASE)

# GUI patterns (adapted to actual Kind log format)
# "switch_channel: channel=ID, N messages"
GUI_SWITCH_CHANNEL = re.compile(
  r"switch_channel: channel=(\d+), (\d+) messages"
)
# "set_messages: N messages" (gui subsystem)
GUI_SET_MESSAGES = re.compile(r"set_messages: (\d+) messages$")

# Store patterns
# "set_messages: N messages for channel ID"
STORE_SET_MESSAGES = re.compile(
  r"set_messages: (\d+) messages for channel (\d+)"
)

# Cache patterns
CACHE_DB_WRITE = re.compile(r"DB write: (.+)")
CACHE_DB_FLUSH = re.compile(r"DB flush requested")


def parse_duration(spec: str) -> timedelta:
  """Parse a duration string like '30m' or '2h' into a timedelta."""
  m = re.match(r"(\d+)\s*([mhds])", spec.lower())
  if not m:
    raise ValueError(f"Invalid duration: {spec}")
  val = int(m.group(1))
  unit = m.group(2)
  if unit == "m":
    return timedelta(minutes=val)
  elif unit == "h":
    return timedelta(hours=val)
  elif unit == "d":
    return timedelta(days=val)
  elif unit == "s":
    return timedelta(seconds=val)
  raise ValueError(f"Unknown unit: {unit}")


def extract_domain(url: str) -> str:
  """Extract domain from a URL."""
  try:
    return urlparse(url).netloc
  except Exception:
    return "unknown"


def extract_channel_id(url: str) -> str | None:
  """Extract channel ID from Discord attachment URLs."""
  m = re.search(r"/attachments/(\d+)/", url)
  return m.group(1) if m else None


def extract_filename(url: str) -> str:
  """Extract filename from URL path."""
  try:
    path = urlparse(url).path
    return path.rsplit("/", 1)[-1] if "/" in path else path
  except Exception:
    return "unknown"


def parse_log_files(
  db: Connection,
  log_files: list[Path],
  last: str | None = None,
) -> dict:
  """Parse log files into the SQLite database.

  Returns a stats dict with parse metadata.
  """
  # First pass: determine time range if --last is specified
  cutoff = None
  if last:
    duration = parse_duration(last)
    # Scan last file for final timestamp
    last_ts = None
    for line in _read_lines_reversed(log_files[0]):
      m = LOG_PATTERN.match(line)
      if m:
        last_ts = datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S.%f")
        break
    if last_ts:
      cutoff = last_ts - duration

  # Tracking state for stitching multi-line events
  image_state: dict[str, dict] = {}  # url -> partial row data
  switch_rows: list[dict] = []
  line_count = 0
  skipped = 0

  # Parse all files oldest to newest
  for log_file in log_files:
    for line in _read_lines(log_file):
      m = LOG_PATTERN.match(line)
      if not m:
        continue

      ts_str, subsystem, level, message = m.groups()

      if cutoff:
        ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S.%f")
        if ts < cutoff:
          skipped += 1
          continue

      line_count += 1

      # Insert raw event
      db.execute(
        "INSERT INTO log_events (timestamp, subsystem, level, message) "
        "VALUES (?, ?, ?, ?)",
        (ts_str, subsystem, level, message),
      )

      # Detect session boundary: gateway connecting means app (re)starting.
      # Flush all pending image state to avoid cross-session stitching.
      if subsystem == "gateway" and GW_CONNECTING.search(message):
        if image_state:
          _flush_image_state(db, image_state)
          image_state.clear()

      # Classify and extract structured data
      if subsystem == "cache":
        _parse_cache_event(db, ts_str, message, image_state, switch_rows)
      elif subsystem == "gui":
        _parse_gui_event(db, ts_str, message, switch_rows)
      elif subsystem == "store":
        _parse_store_event(db, ts_str, message, switch_rows)
      elif subsystem == "rest":
        _parse_rest_event(db, ts_str, message)
      elif subsystem == "gateway":
        _parse_gateway_event(db, ts_str, message)

  # Finalize: flush image_state and switch_rows to DB
  _flush_image_state(db, image_state)
  _flush_channel_switches(db, switch_rows)

  db.commit()

  return {
    "lines_parsed": line_count,
    "lines_skipped": skipped,
    "files": len(log_files),
  }


def _read_lines(path: Path):
  """Read lines from a log file."""
  with open(path, errors="replace") as f:
    yield from f


def _read_lines_reversed(path: Path):
  """Read lines from end of file (for finding last timestamp)."""
  with open(path, "rb") as f:
    f.seek(0, 2)
    size = f.tell()
    block_size = min(8192, size)
    f.seek(size - block_size)
    data = f.read().decode("utf-8", errors="replace")
    for line in reversed(data.splitlines()):
      if line.strip():
        yield line


def _parse_cache_event(
  db: Connection,
  ts: str,
  message: str,
  image_state: dict[str, dict],
  switch_rows: list[dict],
) -> None:
  """Parse cache subsystem events."""
  # Image request (queued)
  m = IMG_REQUEST.search(message)
  if m:
    url = m.group(1)
    state = image_state.setdefault(url, {})
    state["queued_at"] = ts
    state["url"] = url
    state["domain"] = extract_domain(url)
    state["channel_id"] = extract_channel_id(url)
    state["filename"] = extract_filename(url)
    return

  # Image downloading
  m = IMG_DOWNLOADING.search(message)
  if m:
    url = m.group(1)
    # If this URL already has a completed download, flush it first to avoid
    # cross-session stitching (same URL re-downloaded after app restart)
    if url in image_state and image_state[url].get("download_finished_at"):
      _flush_single_image(db, url, image_state.pop(url))
    state = image_state.setdefault(url, {})
    state["download_started_at"] = ts
    state["url"] = url
    state["domain"] = extract_domain(url)
    state["channel_id"] = extract_channel_id(url)
    state["filename"] = extract_filename(url)
    return

  # Image downloaded
  m = IMG_DOWNLOADED.search(message)
  if m:
    url = m.group(1)
    state = image_state.setdefault(url, {})
    state["download_finished_at"] = ts
    state["url"] = url
    state["size_kb"] = int(m.group(2))
    state["domain"] = extract_domain(url)
    state["channel_id"] = extract_channel_id(url)
    state["filename"] = extract_filename(url)
    etag = m.group(3).strip()
    if etag:
      state["etag"] = etag
    modified = m.group(4).strip()
    if modified:
      state["last_modified"] = modified
    state["status"] = "completed"
    return

  # Image 304 Not Modified
  m = IMG_304.search(message)
  if m:
    url = m.group(1)
    state = image_state.setdefault(url, {})
    state["url"] = url
    state["domain"] = extract_domain(url)
    state["channel_id"] = extract_channel_id(url)
    state["filename"] = extract_filename(url)
    state["download_finished_at"] = ts
    state["was_304"] = True
    state["status"] = "304"
    return

  # Image saved
  m = IMG_SAVED.search(message)
  if m:
    url = m.group(1)
    if url in image_state:
      image_state[url]["saved_at"] = ts
    return

  # Image ready (in cache subsystem, not gui)
  m = IMG_READY.search(message)
  if m:
    url = m.group(1)
    if url in image_state:
      image_state[url]["ready_at"] = ts
    if switch_rows:
      sw = switch_rows[-1]
      sw["image_ready_count"] += 1
      if sw["first_image_ready_at"] is None:
        sw["first_image_ready_at"] = ts
      sw["last_image_ready_at"] = ts
    return

  # Image disk hit
  m = IMG_DISK_HIT.search(message)
  if m:
    db.execute(
      "INSERT INTO cache_events (timestamp, cache_type, operation, detail) "
      "VALUES (?, 'image_disk', 'hit', ?)",
      (ts, m.group(1)),
    )
    return

  # Image memory hit (enhanced logging)
  m = IMG_MEMORY_HIT.search(message)
  if m:
    db.execute(
      "INSERT INTO cache_events (timestamp, cache_type, operation, detail) "
      "VALUES (?, 'image_memory', 'hit', ?)",
      (ts, m.group(1)),
    )
    return

  # Image memory miss (enhanced logging)
  m = IMG_MEMORY_MISS.search(message)
  if m:
    db.execute(
      "INSERT INTO cache_events (timestamp, cache_type, operation, detail) "
      "VALUES (?, 'image_memory', 'miss', ?)",
      (ts, m.group(1)),
    )
    return

  # Image evicted (enhanced logging)
  m = IMG_EVICTED.search(message)
  if m:
    db.execute(
      "INSERT INTO cache_events (timestamp, cache_type, operation, detail) "
      "VALUES (?, 'image_memory', 'evict', ?)",
      (ts, m.group(1)),
    )
    return

  # Image decode timing (enhanced logging)
  m = IMG_DECODE.search(message)
  if m:
    source = m.group(1)  # "disk" or "network"
    db.execute(
      "INSERT INTO cache_events (timestamp, cache_type, operation, detail) "
      "VALUES (?, 'image_decode', ?, ?)",
      (ts, source, f"{m.group(2)}ms ({m.group(3)}KB)"),
    )
    return

  # Image memory cache add
  m = IMG_MEMORY_CACHE.search(message)
  if m:
    db.execute(
      "INSERT INTO cache_events (timestamp, cache_type, operation, detail) "
      "VALUES (?, 'image_memory', 'add', ?)",
      (ts, m.group(2)),
    )
    return

  # DB flush
  if CACHE_DB_FLUSH.search(message):
    db.execute(
      "INSERT INTO cache_events (timestamp, cache_type, operation, detail) "
      "VALUES (?, 'database', 'flush', NULL)",
      (ts,),
    )
    return

  # DB write events
  m = CACHE_DB_WRITE.search(message)
  if m:
    db.execute(
      "INSERT INTO cache_events (timestamp, cache_type, operation, detail) "
      "VALUES (?, 'database', 'write', ?)",
      (ts, m.group(1)),
    )
    return


def _parse_gui_event(
  db: Connection,
  ts: str,
  message: str,
  switch_rows: list[dict],
) -> None:
  """Parse GUI subsystem events."""
  # Channel switch
  m = GUI_SWITCH_CHANNEL.search(message)
  if m:
    # Finalize previous switch dwell time
    if switch_rows:
      prev = switch_rows[-1]
      if prev.get("switch_at"):
        prev_ts = datetime.strptime(prev["switch_at"], "%Y-%m-%d %H:%M:%S.%f")
        cur_ts = datetime.strptime(ts, "%Y-%m-%d %H:%M:%S.%f")
        prev["dwell_ms"] = (cur_ts - prev_ts).total_seconds() * 1000

    switch_rows.append({
      "switch_at": ts,
      "channel_id": m.group(1),
      "guild_id": None,
      "initial_message_count": int(m.group(2)),
      "first_render_at": None,
      "rest_arrival_at": None,
      "rest_render_at": None,
      "first_image_ready_at": None,
      "last_image_ready_at": None,
      "image_ready_count": 0,
      "dwell_ms": None,
    })
    return

  # GUI set_messages (rendering event)
  m = GUI_SET_MESSAGES.search(message)
  if m and switch_rows:
    sw = switch_rows[-1]
    if sw["first_render_at"] is None:
      sw["first_render_at"] = ts
    elif sw.get("rest_arrival_at") and sw["rest_render_at"] is None:
      sw["rest_render_at"] = ts
    return


def _parse_store_event(
  db: Connection,
  ts: str,
  message: str,
  switch_rows: list[dict],
) -> None:
  """Parse store subsystem events."""
  m = STORE_SET_MESSAGES.search(message)
  if m and switch_rows:
    channel_id = m.group(2)
    current_switch = switch_rows[-1]
    if current_switch.get("channel_id") == channel_id:
      if current_switch["rest_arrival_at"] is None:
        current_switch["rest_arrival_at"] = ts


def _parse_rest_event(db: Connection, ts: str, message: str) -> None:
  """Parse REST subsystem events."""
  # Response with size (enhanced logging, future)
  m = REST_RESPONSE_SIZE.search(message)
  if m:
    db.execute(
      "INSERT INTO rest_requests "
      "(timestamp, method, path, status_code, elapsed_ms, response_size) "
      "VALUES (?, ?, ?, ?, ?, ?)",
      (ts, m.group(1), m.group(2), int(m.group(3)),
       float(m.group(4)), int(m.group(5))),
    )
    return

  # Response without size (current logging)
  m = REST_RESPONSE.search(message)
  if m:
    db.execute(
      "INSERT INTO rest_requests "
      "(timestamp, method, path, status_code, elapsed_ms) "
      "VALUES (?, ?, ?, ?, ?)",
      (ts, m.group(1), m.group(2), int(m.group(3)), float(m.group(4))),
    )
    return

  # Rate limit: "rate limited METHOD PATH, delaying Nms (type)"
  m = REST_RATE_LIMIT.search(message)
  if m:
    is_global = "global" in message.lower()
    route = f"{m.group(1)} {m.group(2)}"
    db.execute(
      "INSERT INTO rate_limits (timestamp, route, retry_after_ms, is_global) "
      "VALUES (?, ?, ?, ?)",
      (ts, route, float(m.group(3)), int(is_global)),
    )
    return


def _parse_gateway_event(db: Connection, ts: str, message: str) -> None:
  """Parse gateway subsystem events."""
  event_type = None
  detail = None

  if GW_CONNECTING.search(message):
    m = GW_CONNECTING.search(message)
    event_type = "connecting"
    detail = m.group(1) if m else None
  elif GW_WS_CONNECTED.search(message):
    event_type = "ws_connected"
  elif GW_HELLO.search(message):
    m = GW_HELLO.search(message)
    event_type = "hello"
    detail = f"interval={m.group(1)}ms" if m else None
  elif GW_READY.search(message):
    m = GW_READY.search(message)
    event_type = "ready"
    detail = f"session={m.group(1)}" if m else None
  elif GW_SENDING.search(message):
    m = GW_SENDING.search(message)
    event_type = "sending"
    detail = m.group(1) if m else None
  elif GW_HEARTBEAT_ACK.search(message):
    m = GW_HEARTBEAT_ACK.search(message)
    event_type = "heartbeat_ack"
    if m and m.group(1):
      detail = f"rtt={m.group(1)}ms"
  elif GW_HEARTBEAT_MISS.search(message):
    event_type = "heartbeat_miss"
  elif GW_DISPATCH.search(message):
    m = GW_DISPATCH.search(message)
    event_type = "dispatch"
    detail = f"{m.group(1)} seq={m.group(2)}" if m else None
  elif GW_RECONNECT.search(message):
    m = GW_RECONNECT.search(message)
    event_type = "reconnect"
    detail = f"attempt={m.group(1)}, delay={m.group(2)}ms" if m else None
  elif GW_INVALID.search(message):
    event_type = "invalid_session"
  elif GW_CLOSE.search(message):
    m = GW_CLOSE.search(message)
    event_type = "close"
    detail = f"code={m.group(1)}" if m else None
  elif GW_RESUME.search(message):
    event_type = "resume"

  if event_type:
    db.execute(
      "INSERT INTO gateway_events (timestamp, event_type, detail) "
      "VALUES (?, ?, ?)",
      (ts, event_type, detail),
    )


def _flush_single_image(db: Connection, url: str, state: dict) -> None:
  """Write a single image download record to the database."""
  db.execute(
    "INSERT INTO image_downloads "
    "(url, domain, channel_id, filename, queued_at, download_started_at, "
    "download_finished_at, saved_at, ready_at, size_kb, etag, "
    "last_modified, was_304, status) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
    (
      state.get("url", url),
      state.get("domain"),
      state.get("channel_id"),
      state.get("filename"),
      state.get("queued_at"),
      state.get("download_started_at"),
      state.get("download_finished_at"),
      state.get("saved_at"),
      state.get("ready_at"),
      state.get("size_kb"),
      state.get("etag"),
      state.get("last_modified"),
      int(state.get("was_304", False)),
      state.get("status", "unknown"),
    ),
  )


def _flush_image_state(db: Connection, image_state: dict[str, dict]) -> None:
  """Write accumulated image download records to the database."""
  for url, state in image_state.items():
    _flush_single_image(db, url, state)


def _flush_channel_switches(db: Connection, switch_rows: list[dict]) -> None:
  """Write accumulated channel switch records to the database."""
  for sw in switch_rows:
    db.execute(
      "INSERT INTO channel_switches "
      "(switch_at, channel_id, guild_id, initial_message_count, "
      "first_render_at, rest_arrival_at, rest_render_at, "
      "first_image_ready_at, last_image_ready_at, image_ready_count, "
      "dwell_ms) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
      (
        sw["switch_at"],
        sw["channel_id"],
        sw.get("guild_id"),
        sw["initial_message_count"],
        sw["first_render_at"],
        sw["rest_arrival_at"],
        sw["rest_render_at"],
        sw["first_image_ready_at"],
        sw["last_image_ready_at"],
        sw["image_ready_count"],
        sw.get("dwell_ms"),
      ),
    )
