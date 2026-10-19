"""Draws the app icon and splash screens (pure Python, no dependencies).

Usage: python make_art.py <output dir>
Writes icon.png (128x128), splash_tv.png (1280x720) and splash_drc.png (854x480).
"""

import math
import os
import struct
import sys
import zlib

NAVY_TOP = (27, 58, 107)
NAVY_BOTTOM = (15, 30, 56)
PAPER = (232, 238, 247)
PAPER_SHADE = (196, 208, 226)
ORANGE = (255, 165, 61)
DARK = (16, 31, 58)


def write_png(path, width, height, rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9))
    data += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(data)


def rounded_rect(x, y, x0, y0, x1, y1, r):
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r and x0 <= x <= x1 and y0 <= y <= y1


def logo_color(u, v):
    """Logo in unit coordinates; returns an RGB tuple or None (transparent)."""
    # lock badge on top of the folder
    du, dv = u - 0.70, v - 0.70
    d = math.hypot(du, dv)
    if d <= 0.19:
        if d >= 0.17:
            return NAVY_BOTTOM  # ring separating badge and folder
        if rounded_rect(u, v, 0.615, 0.675, 0.785, 0.80, 0.02):
            return DARK
        ring = math.hypot(u - 0.70, v - 0.675)
        if v <= 0.675 and 0.038 <= ring <= 0.062:
            return DARK
        return ORANGE
    # folder: tab and body
    if rounded_rect(u, v, 0.17, 0.25, 0.47, 0.40, 0.04):
        return PAPER_SHADE
    if rounded_rect(u, v, 0.17, 0.33, 0.83, 0.79, 0.06):
        return PAPER
    return None


def background(y, height):
    t = y / max(height - 1, 1)
    return tuple(int(NAVY_TOP[i] + (NAVY_BOTTOM[i] - NAVY_TOP[i]) * t) for i in range(3))


def render(width, height, logo_box, rounded_canvas, ss=3):
    lx, ly, ls = logo_box
    rows = []
    for y in range(height):
        row = []
        bg = background(y, height)
        for x in range(width):
            in_logo = lx - 1 <= x <= lx + ls + 1 and ly - 1 <= y <= ly + ls + 1
            samples = ss if in_logo or rounded_canvas else 1
            acc = [0, 0, 0, 0]
            for sy in range(samples):
                for sx in range(samples):
                    px = x + (sx + 0.5) / samples
                    py = y + (sy + 0.5) / samples
                    if rounded_canvas and not rounded_rect(px / width, py / height, 0, 0, 1, 1, 0.16):
                        continue
                    c = logo_color((px - lx) / ls, (py - ly) / ls) if in_logo else None
                    c = c or bg
                    acc[0] += c[0]
                    acc[1] += c[1]
                    acc[2] += c[2]
                    acc[3] += 255
            n = samples * samples
            row.extend([acc[0] // n if acc[3] else 0, acc[1] // n if acc[3] else 0, acc[2] // n if acc[3] else 0, acc[3] // n])
        rows.append(row)
    return rows


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    write_png(os.path.join(out, "icon.png"), 128, 128, render(128, 128, (0, 0, 128), True, 4))
    write_png(os.path.join(out, "splash_tv.png"), 1280, 720, render(1280, 720, (490, 210, 300), False))
    write_png(os.path.join(out, "splash_drc.png"), 854, 480, render(854, 480, (327, 140, 200), False))


if __name__ == "__main__":
    main()
