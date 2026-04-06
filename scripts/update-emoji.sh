#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
OUTPUT="$REPO_ROOT/data/emoji.json"

if ! command -v deno &>/dev/null; then
  echo "Error: deno is not installed" >&2
  echo "Install: https://docs.deno.com/runtime/getting_started/installation/" >&2
  exit 1
fi

echo "Fetching Discord emoji shortcodes via discord-emoji..."
deno eval "
import * as emojis from 'npm:discord-emoji';
const flat = Object.assign({}, ...Object.values(emojis));
console.log(JSON.stringify(flat, null, 2));
" > "$OUTPUT"

count=$(grep -c '": "' "$OUTPUT")
echo "Wrote $count shortcodes to $OUTPUT"
