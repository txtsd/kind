"""Account detection and log file discovery."""

import os
import sys
from pathlib import Path


# Hardcoded test account IDs (client never produces these)
TEST_ACCOUNT_IDS = {"100"}


def state_dir() -> Path:
  """Return Kind's state directory using platform-specific XDG paths."""
  if sys.platform == "win32":
    base = os.getenv("LOCALAPPDATA", "")
    return Path(base) / "kind"
  elif sys.platform == "darwin":
    return Path.home() / "Library" / "Application Support" / "kind"
  else:
    xdg = os.getenv("XDG_STATE_HOME", str(Path.home() / ".local" / "state"))
    return Path(xdg) / "kind"


def discover_accounts() -> list[dict]:
  """Find all account directories and label them.

  Returns list of dicts with keys: id, path, label, is_test, log_modified.
  Sorted by most recently modified log first.
  """
  accounts_dir = state_dir() / "accounts"
  if not accounts_dir.is_dir():
    return []

  results = []
  for entry in accounts_dir.iterdir():
    if not entry.is_dir():
      continue
    account_id = entry.name
    log_dir = entry / "logs"

    is_test = account_id in TEST_ACCOUNT_IDS
    if not is_test and log_dir.is_dir():
      # Check log content for integration test markers
      for log_file in log_dir.iterdir():
        if not log_file.name.startswith("kind-") or not log_file.name.endswith(".log"):
          continue
        try:
          with open(log_file) as f:
            head = f.read(4096)
            if "integration_test" in head:
              is_test = True
              break
        except OSError:
          pass

    label = f"{account_id} (test)" if is_test else account_id
    # Use the most recently modified session log for sorting
    modified = 0
    if log_dir.is_dir():
      for f in log_dir.iterdir():
        if f.name.startswith("kind-") and f.name.endswith(".log"):
          modified = max(modified, f.stat().st_mtime)

    results.append({
      "id": account_id,
      "path": entry,
      "label": label,
      "is_test": is_test,
      "log_modified": modified,
    })

  results.sort(key=lambda x: x["log_modified"], reverse=True)
  return results


def find_log_files(account_path: Path) -> list[Path]:
  """Find and sort all session log files for an account.

  Session files: kind-2026-04-05T10-59-42.log, kind-2026-04-05T10-59-42.1.log, ...

  Also includes matching pre-login logs from the global log directory.
  Pre-login logs contain startup events (initial channel switch, gateway
  connecting) that occur before the log sink is reinitialized to the
  account directory.

  Returns paths sorted chronologically (oldest first).
  """
  log_dir = account_path / "logs"
  if not log_dir.is_dir():
    return []

  files = []
  for f in log_dir.iterdir():
    if f.name.startswith("kind-") and f.name.endswith(".log"):
      files.append(f)

  if not files:
    return []

  # Include matching pre-login logs. The pre-login log directory shares the
  # same session timestamp filenames but contains events from before the
  # account-scoped log sink is initialized.
  pre_login_dir = state_dir() / "logs"
  if pre_login_dir.is_dir():
    account_names = {f.name for f in files}
    for f in pre_login_dir.iterdir():
      if f.name in account_names:
        files.append(f)

  def sort_key(p: Path) -> tuple[str, int, int]:
    # Pre-login logs sort before account logs for the same session.
    is_account = 1 if "accounts" in str(p) else 0

    # kind-TIMESTAMP.log or kind-TIMESTAMP.N.log
    # The timestamp sorts lexicographically. Rotated files (.N.) sort after base.
    stem = p.stem  # e.g. "kind-2026-04-05T10-59-42" or "kind-2026-04-05T10-59-42.1"
    parts = stem.split(".")
    timestamp = parts[0]  # "kind-TIMESTAMP"
    rotation = int(parts[1]) if len(parts) >= 2 and parts[1].isdigit() else 0
    # Rotated files are older than the base, so higher rotation = older.
    # We want oldest first, so: sort by timestamp asc, then rotation desc,
    # then pre-login before account.
    return (timestamp, -rotation, is_account)

  files.sort(key=sort_key)
  return files


def select_account(accounts: list[dict], requested_id: str | None = None) -> dict | None:
  """Select an account, either by ID or interactively."""
  if not accounts:
    return None

  if requested_id:
    for acc in accounts:
      if acc["id"] == requested_id:
        return acc
    return None

  # Filter to non-test accounts for auto-selection
  real = [a for a in accounts if not a["is_test"]]
  if len(real) == 1:
    return real[0]

  # Interactive selection
  print("Available accounts:")
  for i, acc in enumerate(accounts):
    print(f"  [{i + 1}] {acc['label']}")
  print()

  while True:
    try:
      choice = input("Select account: ").strip()
      idx = int(choice) - 1
      if 0 <= idx < len(accounts):
        return accounts[idx]
    except (ValueError, EOFError):
      pass
    print(f"Please enter 1-{len(accounts)}")
