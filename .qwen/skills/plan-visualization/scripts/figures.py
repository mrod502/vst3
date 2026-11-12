#!/usr/bin/env python3
"""Regenerate the figures in templates/palette.md: OKLCH L/C, relative luminance,
and WCAG contrast against the near-black label colour and white.

    python3 scripts/figures.py                      # the shipped state palette
    python3 scripts/figures.py '#3f9d5c,#d0821f'    # any set, same table
"""

import sys

PALETTE = [
    ("done", "#3f9d5c", "\u2713"),
    ("record", "#8f6fc4", "\u25a3"),
    ("wip", "#d0821f", "\u25b6"),
    ("blocked", "#d05a52", "\u2715"),
    ("todo", "#4f86c6", "\u25cb"),
    ("context", "#7b8794", "\u25a2"),
]
LABEL = "#0b1220"


def _lin(byte):
    c = byte / 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def _rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def luminance(h):
    r, g, b = (_lin(v) for v in _rgb(h))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def oklch(h):
    r, g, b = (_lin(v) for v in _rgb(h))
    l = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1 / 3)
    m = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1 / 3)
    s = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1 / 3)
    L = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s
    a = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s
    bb = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s
    return L, (a * a + bb * bb) ** 0.5


def contrast(a, b):
    hi, lo = sorted((luminance(a), luminance(b)), reverse=True)
    return (hi + 0.05) / (lo + 0.05)


def main():
    rows = PALETTE
    if len(sys.argv) > 1:
        rows = [(h, h, "?") for h in sys.argv[1].split(",")]

    print(f"{'state':9s} {'hex':9s} {'g':1s} {'L':>6s} {'C':>6s} {'Y':>6s} "
          f"{'vs label':>9s} {'vs white':>9s}")
    for name, hexv, glyph in rows:
        L, C = oklch(hexv)
        print(f"{name:9s} {hexv:9s} {glyph:1s} {L:6.3f} {C:6.3f} "
              f"{luminance(hexv):6.3f} {contrast(hexv, LABEL):8.2f}:1 "
              f"{contrast(hexv, '#ffffff'):8.2f}:1")

    print("\npairs a reader cannot separate in greyscale (luminance ratio < 1.15):")
    flagged = [(contrast(rows[i][1], rows[j][1]), rows[i][0], rows[j][0])
               for i in range(len(rows)) for j in range(i + 1, len(rows))
               if contrast(rows[i][1], rows[j][1]) < 1.15]
    if not flagged:
        print("  none")
    for c, a, b in sorted(flagged):
        print(f"  {a} / {b}: {c:.2f}:1")
    print("hue collapse is not computed here: run the dataviz validator for the "
          "colourblind DeltaE list.")


main()
