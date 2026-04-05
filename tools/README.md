# kind-analyze

Log analysis and diagnostics for Kind. Parses log files into an in-memory SQLite database and runs analyzers against the structured data.

Requires [uv](https://docs.astral.sh/uv/) and Python 3.14+.

## Usage

```
cd tools/
uv run kind-analyze <command> [options]
```

With no `--log-file` or `--log-dir`, the tool auto-discovers accounts from Kind's state directory and prompts for selection if multiple exist. Use `--account <id>` to skip the prompt.

### Commands

| Command | Description |
|---|---|
| `summary` | Single-screen health overview across all subsystems |
| `startup` | Timeline from gateway connect to first channel loaded |
| `images` | Download performance by size bucket, 304 hit rate, CDN latency |
| `network` | REST latency by endpoint, error rates, rate limit breakdown |
| `gateway` | Heartbeat health, RTT distribution, reconnection history |
| `channels` | Per-channel switch latency (render, REST, image timing) |
| `cache` | Memory hit rate, request resolution, eviction pressure |
| `responsiveness` | Download queue contention during channel switches |
| `cdn-health` | Per-domain CDN reliability and latency assessment |
| `all` | Run every analyzer sequentially |

### Options

| Flag | Description |
|---|---|
| `--account ID` | Use a specific account (skip interactive selection) |
| `--log-file PATH [PATH ...]` | Analyze specific log file(s) instead of auto-discovery |
| `--log-dir DIR` | Analyze all log files in a directory |
| `--last DURATION` | Only analyze the last N of log data (e.g. `30m`, `2h`, `1d`) |
| `--save-db PATH` | Persist the SQLite database to disk for manual querying |
| `--no-color` | Disable terminal colors |

### Examples

```sh
# Health overview for the most recent account
uv run kind-analyze summary

# Last 30 minutes of image download performance
uv run kind-analyze --last 30m images

# Full analysis across all subsystems
uv run kind-analyze all

# Analyze specific log files
uv run kind-analyze --log-file ~/.local/state/kind/accounts/123/logs/kind-2026-04-05T12-08-56.log gateway

# Dump the parsed database for ad-hoc SQL queries
uv run kind-analyze --save-db /tmp/kind.db all
sqlite3 /tmp/kind.db "SELECT path, COUNT(*) FROM rest_requests GROUP BY path ORDER BY COUNT(*) DESC LIMIT 10"
```

## Log file handling

Log files use session-based naming: `kind-YYYY-MM-DDTHH-MM-SS.log` with spdlog rotation producing `.1.log`, `.2.log` suffixes.

For account-scoped analysis, matching pre-login logs from the global log directory are automatically included since startup events (initial channel switch, gateway connecting) occur before the log sink is reinitialized to the account directory.

## Architecture

Log lines are parsed into a SQLite database with tables for raw events, image downloads, REST requests, rate limits, gateway events, channel switches, and cache events. Each analyzer queries this database independently, so `--save-db` lets you run arbitrary SQL against the same data the analyzers see.

The parser stitches multi-line image lifecycle events (request, downloading, downloaded, 304, saved, ready) into complete download records keyed by URL. Session boundaries (gateway connecting events) flush pending state to prevent cross-session data corruption.

## Adding an analyzer

1. Create `tools/src/kind_analyze/analyzers/your_analyzer.py` with a `run(db, fmt)` function
2. Register it in `cli.py`'s `analyzers` dict and `subparsers` list
3. Query the SQLite tables defined in `db.py`; use `fmt` for terminal output
