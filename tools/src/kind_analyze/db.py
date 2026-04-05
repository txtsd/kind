"""SQLite schema definition and query helpers."""

import sqlite3

SCHEMA = """
CREATE TABLE IF NOT EXISTS log_events (
  id INTEGER PRIMARY KEY,
  timestamp TEXT NOT NULL,
  subsystem TEXT NOT NULL,
  level TEXT NOT NULL,
  message TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS image_downloads (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  url TEXT NOT NULL,
  domain TEXT,
  channel_id TEXT,
  filename TEXT,
  queued_at TEXT,
  download_started_at TEXT,
  download_finished_at TEXT,
  saved_at TEXT,
  ready_at TEXT,
  size_kb INTEGER,
  etag TEXT,
  last_modified TEXT,
  was_304 INTEGER DEFAULT 0,
  status TEXT DEFAULT 'unknown'
);

CREATE TABLE IF NOT EXISTS rest_requests (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp TEXT NOT NULL,
  method TEXT,
  path TEXT,
  status_code INTEGER,
  elapsed_ms REAL,
  body_size INTEGER,
  response_size INTEGER
);

CREATE TABLE IF NOT EXISTS rate_limits (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp TEXT NOT NULL,
  route TEXT,
  retry_after_ms REAL,
  is_global INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS gateway_events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp TEXT NOT NULL,
  event_type TEXT NOT NULL,
  detail TEXT
);

CREATE TABLE IF NOT EXISTS channel_switches (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  switch_at TEXT NOT NULL,
  channel_id TEXT,
  guild_id TEXT,
  initial_message_count INTEGER,
  first_render_at TEXT,
  rest_arrival_at TEXT,
  rest_render_at TEXT,
  first_image_ready_at TEXT,
  last_image_ready_at TEXT,
  image_ready_count INTEGER DEFAULT 0,
  dwell_ms REAL
);

CREATE TABLE IF NOT EXISTS cache_events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  timestamp TEXT NOT NULL,
  cache_type TEXT NOT NULL,
  operation TEXT NOT NULL,
  detail TEXT
);

CREATE INDEX IF NOT EXISTS idx_log_events_subsystem ON log_events(subsystem);
CREATE INDEX IF NOT EXISTS idx_log_events_timestamp ON log_events(timestamp);
CREATE INDEX IF NOT EXISTS idx_image_downloads_domain ON image_downloads(domain);
CREATE INDEX IF NOT EXISTS idx_channel_switches_channel ON channel_switches(channel_id);
CREATE INDEX IF NOT EXISTS idx_rest_requests_path ON rest_requests(path);
"""


def create_db(path: str | None = None) -> sqlite3.Connection:
  """Create a SQLite database with the analysis schema.

  If path is None, creates an in-memory database.
  """
  db = sqlite3.connect(path or ":memory:")
  db.row_factory = sqlite3.Row
  db.executescript(SCHEMA)
  return db
