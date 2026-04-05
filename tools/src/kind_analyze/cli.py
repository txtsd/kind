"""CLI entry point for kind-analyze."""

import argparse
import sys
from pathlib import Path

from kind_analyze.account import discover_accounts, find_log_files, select_account


def resolve_log_files(args: argparse.Namespace) -> list[Path]:
  """Resolve log files from CLI arguments."""
  if args.log_file:
    paths = []
    for f in args.log_file:
      p = Path(f)
      if not p.exists():
        print(f"Error: log file not found: {p}", file=sys.stderr)
        sys.exit(1)
      paths.append(p)
    return paths

  if args.log_dir:
    d = Path(args.log_dir)
    if not d.is_dir():
      print(f"Error: log directory not found: {d}", file=sys.stderr)
      sys.exit(1)
    files = sorted(f for f in d.iterdir() if f.name.startswith("kind-") and f.name.endswith(".log"))
    if not files:
      print(f"Error: no log files in {d}", file=sys.stderr)
      sys.exit(1)
    return files

  accounts = discover_accounts()
  account = select_account(accounts, args.account)
  if not account:
    print("Error: no account found", file=sys.stderr)
    sys.exit(1)

  files = find_log_files(account["path"])
  if not files:
    print(f"Error: no log files for account {account['label']}", file=sys.stderr)
    sys.exit(1)

  print(f"Account: {account['label']}")
  print(f"Log files: {len(files)} ({', '.join(f.name for f in files)})")
  return files


def main() -> None:
  parser = argparse.ArgumentParser(
    prog="kind-analyze",
    description="Log analysis and diagnostics for Kind",
  )
  subparsers = parser.add_subparsers(dest="command", help="Analysis command")

  parser.add_argument("--account", help="Account ID (skip interactive selection)")
  parser.add_argument("--log-file", nargs="+", help="Use specific log file(s)")
  parser.add_argument("--log-dir", help="Use all log files in a directory")
  parser.add_argument("--save-db", help="Persist SQLite database to this path")
  parser.add_argument("--last", help="Only analyze last N duration (e.g. 30m, 2h)")
  parser.add_argument("--no-color", action="store_true", help="Disable colors")

  for name in [
    "images", "network", "gateway", "startup", "channels",
    "cache", "summary", "responsiveness", "cdn-health", "all",
  ]:
    subparsers.add_parser(name)

  args = parser.parse_args()
  if not args.command:
    parser.print_help()
    sys.exit(1)

  log_files = resolve_log_files(args)

  from kind_analyze.db import create_db
  from kind_analyze.parser import parse_log_files

  db = create_db(args.save_db)
  stats = parse_log_files(db, log_files, last=args.last)
  print(f"Parsed {stats['lines_parsed']} lines from {stats['files']} file(s)")
  if stats["lines_skipped"]:
    print(f"Skipped {stats['lines_skipped']} lines (before --last cutoff)")

  # Dispatch to analyzer
  from importlib import import_module

  from kind_analyze.fmt import Formatter

  formatter = Formatter(no_color=args.no_color)

  analyzers = {
    "summary": "kind_analyze.analyzers.summary",
    "startup": "kind_analyze.analyzers.startup",
    "images": "kind_analyze.analyzers.images",
    "network": "kind_analyze.analyzers.network",
    "gateway": "kind_analyze.analyzers.gateway",
    "channels": "kind_analyze.analyzers.channels",
    "cache": "kind_analyze.analyzers.cache",
    "responsiveness": "kind_analyze.analyzers.responsiveness",
    "cdn-health": "kind_analyze.analyzers.cdn_health",
  }

  if args.command == "all":
    for module_path in analyzers.values():
      import_module(module_path).run(db, formatter)
  elif args.command in analyzers:
    import_module(analyzers[args.command]).run(db, formatter)
  else:
    print(f"Unknown command: {args.command}", file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
  main()
