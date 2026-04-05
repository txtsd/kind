"""Terminal formatting with 24-bit true color support."""

import os
import re
import sys
from dataclasses import dataclass


@dataclass
class Theme:
  """Color theme using RGB tuples."""
  # Status colors
  healthy: tuple[int, int, int] = (80, 200, 120)
  warning: tuple[int, int, int] = (240, 180, 50)
  problem: tuple[int, int, int] = (230, 70, 70)

  # Text colors
  heading: tuple[int, int, int] = (140, 170, 240)
  subheading: tuple[int, int, int] = (180, 180, 210)
  muted: tuple[int, int, int] = (120, 120, 140)
  value: tuple[int, int, int] = (220, 220, 240)
  label: tuple[int, int, int] = (170, 170, 190)

  # Bar colors
  bar_filled: tuple[int, int, int] = (100, 160, 230)
  bar_empty: tuple[int, int, int] = (60, 60, 80)

  # Sparkline gradient (low to high)
  spark_low: tuple[int, int, int] = (80, 200, 120)
  spark_mid: tuple[int, int, int] = (240, 180, 50)
  spark_high: tuple[int, int, int] = (230, 70, 70)


class Formatter:
  """Terminal output formatter with true color support."""

  SPARK_CHARS = "▁▂▃▄▅▆▇█"
  BAR_FILLED = "▰"
  BAR_EMPTY = "▱"

  def __init__(self, *, no_color: bool = False, theme: Theme | None = None):
    self.theme = theme or Theme()
    self.color_enabled = not no_color and self._detect_color_support()

  def _detect_color_support(self) -> bool:
    """Detect 24-bit color support."""
    if not hasattr(sys.stdout, "isatty") or not sys.stdout.isatty():
      return False
    colorterm = os.getenv("COLORTERM", "")
    if colorterm in ("truecolor", "24bit"):
      return True
    # Fallback: check TERM for known modern terminals
    term = os.getenv("TERM", "")
    return any(t in term for t in ("kitty", "alacritty", "256color"))

  def fg(self, text: str, rgb: tuple[int, int, int]) -> str:
    """Apply foreground color to text."""
    if not self.color_enabled:
      return text
    r, g, b = rgb
    return f"\033[38;2;{r};{g};{b}m{text}\033[0m"

  def bold(self, text: str) -> str:
    """Apply bold formatting."""
    if not self.color_enabled:
      return text
    return f"\033[1m{text}\033[0m"

  def bold_fg(self, text: str, rgb: tuple[int, int, int]) -> str:
    """Apply bold + foreground color."""
    if not self.color_enabled:
      return text
    r, g, b = rgb
    return f"\033[1;38;2;{r};{g};{b}m{text}\033[0m"

  # ── Report structure ────────────────────────────────────

  def header(self, title: str, subtitle: str = "", detail: str = "") -> str:
    """Render a report header box."""
    width = 62
    lines = [
      "╔" + "═" * width + "╗",
      "║  " + self.bold_fg(title.ljust(width - 2), self.theme.heading) + "║",
    ]
    if subtitle:
      lines.append(
        "║  " + self.fg(subtitle.ljust(width - 2), self.theme.muted) + "║"
      )
    if detail:
      lines.append(
        "║  " + self.fg(detail.ljust(width - 2), self.theme.muted) + "║"
      )
    lines.append("╚" + "═" * width + "╝")
    return "\n".join(lines)

  def section(self, title: str) -> str:
    """Render a section divider."""
    line = "─" * (60 - len(title) - 1)
    return f"\n── {self.bold_fg(title, self.theme.subheading)} {self.fg(line, self.theme.muted)}"

  # ── Data visualization ──────────────────────────────────

  def bar(self, value: float, max_value: float, width: int = 18) -> str:
    """Render a diamond bar (▰▱)."""
    if max_value <= 0:
      filled = 0
    else:
      filled = min(width, max(0, round(value / max_value * width)))
    empty = width - filled
    bar_str = self.BAR_FILLED * filled + self.BAR_EMPTY * empty
    if self.color_enabled:
      filled_part = self.fg(self.BAR_FILLED * filled, self.theme.bar_filled)
      empty_part = self.fg(self.BAR_EMPTY * empty, self.theme.bar_empty)
      return filled_part + empty_part
    return bar_str

  def sparkline(self, values: list[float], width: int = 20) -> str:
    """Render a sparkline distribution.

    Requires 8+ values; returns empty string otherwise.
    Buckets values into `width` bins and maps to ▁▂▃▄▅▆▇█.
    """
    if len(values) < 8:
      return ""

    # Bucket into width bins
    lo, hi = min(values), max(values)
    if hi == lo:
      return self.SPARK_CHARS[4] * min(len(values), width)

    bin_size = (hi - lo) / width
    bins = [0] * width
    for v in values:
      idx = min(width - 1, int((v - lo) / bin_size))
      bins[idx] += 1

    # Normalize bins to 0-7 range
    max_bin = max(bins) or 1
    chars = []
    for count in bins:
      level = int(count / max_bin * 7)
      char = self.SPARK_CHARS[level]
      if self.color_enabled:
        # Gradient color based on level
        t = level / 7
        if t < 0.5:
          rgb = self._lerp_color(self.theme.spark_low, self.theme.spark_mid, t * 2)
        else:
          rgb = self._lerp_color(self.theme.spark_mid, self.theme.spark_high, (t - 0.5) * 2)
        chars.append(self.fg(char, rgb))
      else:
        chars.append(char)

    return "".join(chars)

  def _lerp_color(
    self,
    a: tuple[int, int, int],
    b: tuple[int, int, int],
    t: float,
  ) -> tuple[int, int, int]:
    """Linear interpolation between two RGB colors."""
    return (
      int(a[0] + (b[0] - a[0]) * t),
      int(a[1] + (b[1] - a[1]) * t),
      int(a[2] + (b[2] - a[2]) * t),
    )

  # ── Tables ──────────────────────────────────────────────

  def table(
    self,
    headers: list[str],
    rows: list[list[str]],
    alignments: list[str] | None = None,
  ) -> str:
    """Render a table with box-drawing characters.

    alignments: list of 'l' (left) or 'r' (right) per column.
    """
    if not rows:
      return ""

    cols = len(headers)
    if not alignments:
      alignments = ["l"] * cols

    # Calculate column widths (accounting for ANSI escape codes)
    widths = [len(h) for h in headers]
    for row in rows:
      for i, cell in enumerate(row):
        visible = self._visible_len(cell)
        widths[i] = max(widths[i], visible)

    # Add padding
    widths = [w + 2 for w in widths]

    def fmt_cell(text: str, width: int, align: str) -> str:
      visible = self._visible_len(text)
      pad = width - visible
      if align == "r":
        return " " * pad + text
      return text + " " * pad

    # Build table
    top = "┌" + "┬".join("─" * w for w in widths) + "┐"
    mid = "├" + "┼".join("─" * w for w in widths) + "┤"
    bot = "└" + "┴".join("─" * w for w in widths) + "┘"

    header_cells = [
      fmt_cell(f" {self.bold(h)} ", widths[i], alignments[i])
      for i, h in enumerate(headers)
    ]
    header_line = "│" + "│".join(header_cells) + "│"

    data_lines = []
    for row in rows:
      cells = [
        fmt_cell(f" {cell} ", widths[i], alignments[i])
        for i, cell in enumerate(row)
      ]
      data_lines.append("│" + "│".join(cells) + "│")

    return "\n".join([top, header_line, mid] + data_lines + [bot])

  def _visible_len(self, text: str) -> int:
    """Length of text excluding ANSI escape sequences."""
    return len(re.sub(r"\033\[[^m]*m", "", text))

  # ── Suggestions ─────────────────────────────────────────

  def healthy(self, message: str) -> str:
    """Format a healthy suggestion."""
    marker = self.fg("✓", self.theme.healthy)
    return f"  {marker} {message}"

  def warning(self, message: str) -> str:
    """Format a warning suggestion."""
    marker = self.fg("⚠", self.theme.warning)
    return f"  {marker} {message}"

  def problem(self, message: str) -> str:
    """Format a problem suggestion."""
    marker = self.fg("✗", self.theme.problem)
    return f"  {marker} {message}"

  # ── Utilities ───────────────────────────────────────────

  def duration(self, ms: float) -> str:
    """Format a duration in human-readable form."""
    if ms < 1:
      return f"{ms * 1000:.0f}us"
    if ms < 1000:
      return f"{ms:.0f}ms"
    if ms < 60000:
      return f"{ms / 1000:.1f}s"
    return f"{ms / 60000:.1f}m"

  def size(self, kb: int) -> str:
    """Format a size in human-readable form."""
    if kb < 1024:
      return f"{kb}KB"
    return f"{kb / 1024:.1f}MB"
