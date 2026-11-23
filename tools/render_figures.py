#!/usr/bin/env python3
"""Render the SRMS plates from docs/DESIGN.md and a live --report.

Fonts: IBM Plex Sans and IBM Plex Mono. One paper palette, one accent.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "assets"
SOCIAL = ROOT / ".github" / "social-preview.png"
BINARY = ROOT / "build" / "srms"
SAMPLE = ROOT / "data" / "sample_cohort.srms"

SANS = Path("/home/ubuntu/.local/share/fonts/ibm-plex/IBMPlexSans-Regular.ttf")
SANS_MED = Path("/home/ubuntu/.local/share/fonts/ibm-plex/IBMPlexSans-Medium.ttf")
MONO = Path("/home/ubuntu/.local/share/fonts/ibm-plex/IBMPlexMono-Regular.ttf")
MONO_MED = Path("/home/ubuntu/.local/share/fonts/ibm-plex/IBMPlexMono-Medium.ttf")

LIGHT = {
    "paper": (0xF3, 0xF0, 0xE8),
    "ink": (0x1A, 0x18, 0x14),
    "muted": (0x6E, 0x68, 0x5F),
    "rule": (0xC8, 0xC1, 0xB4),
    "accent": (0xB8, 0x43, 0x1A),
}
DARK = {
    "paper": (0x14, 0x13, 0x11),
    "ink": (0xE8, 0xE4, 0xDA),
    "muted": (0xA3, 0x9C, 0x90),
    "rule": (0x3C, 0x38, 0x32),
    "accent": (0xE0, 0x6A, 0x3C),
}

FACE = {
    "sans": SANS,
    "sans-med": SANS_MED,
    "mono": MONO,
    "mono-med": MONO_MED,
}


def hex_of(rgb: tuple[int, int, int]) -> str:
    return "#{:02X}{:02X}{:02X}".format(*rgb)


def load_report() -> dict:
    raw = subprocess.check_output(
        [str(BINARY), "--file", str(SAMPLE), "--report", "-"],
        text=True,
    )
    data: dict = {
        "grades": {},
        "records": [],
        "top": [],
    }
    for line in raw.splitlines():
        parts = line.split("\t")
        key = parts[0]
        if key in ("count",):
            data["count"] = int(parts[1])
        elif key == "average_tenths":
            data["average"] = int(parts[1]) / 10.0
        elif key == "median_tenths":
            data["median"] = int(parts[1]) / 10.0
        elif key in (
            "highest_name",
            "highest_programme",
            "highest_roll",
            "highest_grade",
            "lowest_name",
            "lowest_programme",
            "lowest_roll",
            "lowest_grade",
        ):
            data[key] = parts[1]
        elif key in ("highest_tenths", "lowest_tenths"):
            data[key] = int(parts[1]) / 10.0
        elif key == "grade":
            data["grades"][parts[1]] = int(parts[2])
        elif key == "record":
            data["records"].append(
                {
                    "roll": int(parts[1]),
                    "name": parts[2],
                    "programme": parts[3],
                    "year": int(parts[4]),
                    "pct": int(parts[5]) / 10.0,
                    "grade": parts[6],
                }
            )
        elif key == "top":
            data["top"].append(
                {
                    "rank": int(parts[1]),
                    "roll": int(parts[2]),
                    "name": parts[3],
                    "programme": parts[4],
                    "pct": int(parts[5]) / 10.0,
                    "grade": parts[6],
                }
            )
    if len(data["records"]) != 24:
        raise SystemExit(f"expected 24 records, got {len(data['records'])}")
    return data


class Outliner:
    def __init__(self, path: Path) -> None:
        self.font = TTFont(str(path))
        self.glyphs = self.font.getGlyphSet()
        self.cmap = self.font.getBestCmap()
        self.hmtx = self.font["hmtx"]
        self.upem = self.font["head"].unitsPerEm

    def paths(self, text: str, x: float, y_baseline: float, size: float) -> list[str]:
        scale = size / self.upem
        pen_x = x
        commands: list[str] = []
        for ch in text:
            name = self.cmap.get(ord(ch))
            if name is None:
                pen_x += size * 0.5
                continue
            pen = SVGPathPen(self.glyphs)
            transform = TransformPen(pen, (scale, 0, 0, -scale, pen_x, y_baseline))
            self.glyphs[name].draw(transform)
            command = pen.getCommands()
            if command:
                commands.append(command)
            advance, _ = self.hmtx[name]
            pen_x += advance * scale
        return commands

    def width(self, text: str, size: float) -> float:
        scale = size / self.upem
        total = 0.0
        for ch in text:
            name = self.cmap.get(ord(ch))
            if name is None:
                total += size * 0.5
                continue
            advance, _ = self.hmtx[name]
            total += advance * scale
        return total


OUTLINERS = {key: Outliner(path) for key, path in FACE.items()}


class Plate:
    """PNG plate. Coordinates are 1x logical pixels; the file is 2x."""

    def __init__(self, width: int, height: int, theme: dict, scale: int = 2) -> None:
        self.w = width
        self.h = height
        self.theme = theme
        self.scale = scale
        self.image = Image.new("RGB", (width * scale, height * scale), theme["paper"])
        self.draw = ImageDraw.Draw(self.image)
        self._fonts: dict[tuple[str, int], ImageFont.FreeTypeFont] = {}

    def font(self, face: str, size: int) -> ImageFont.FreeTypeFont:
        key = (face, size)
        if key not in self._fonts:
            self._fonts[key] = ImageFont.truetype(str(FACE[face]), size * self.scale)
        return self._fonts[key]

    def _p(self, value: float) -> float:
        return value * self.scale

    def text(self, x: float, y: float, text: str, face: str, size: int, fill: str, anchor: str = "lt") -> None:
        self.draw.text(
            (self._p(x), self._p(y)),
            text,
            font=self.font(face, size),
            fill=self.theme[fill],
            anchor=anchor,
        )

    def text_width(self, text: str, face: str, size: int) -> float:
        return self.font(face, size).getlength(text) / self.scale

    def tracked(self, x: float, y: float, text: str, face: str, size: int, fill: str, em: float = 0.08) -> float:
        extra = size * em
        cursor = x
        for ch in text:
            self.text(cursor, y, ch, face, size, fill)
            cursor += self.text_width(ch, face, size) + extra
        return cursor

    def line(self, x1: float, y1: float, x2: float, y2: float, fill: str = "ink", width: float = 1) -> None:
        stroke = max(1, int(round(width * self.scale)))
        self.draw.line(
            [(self._p(x1), self._p(y1)), (self._p(x2), self._p(y2))],
            fill=self.theme[fill],
            width=stroke,
        )

    def rect(self, x: float, y: float, w: float, h: float, fill: str | None = None, outline: str | None = None, width: float = 1) -> None:
        box = [self._p(x), self._p(y), self._p(x + w), self._p(y + h)]
        self.draw.rectangle(
            box,
            fill=self.theme[fill] if fill else None,
            outline=self.theme[outline] if outline else None,
            width=max(1, int(round(width * self.scale))) if outline else 1,
        )

    def head(self, number: str, title: str, note: str = "") -> None:
        self.text(32, 28, number, "mono", 13, "accent")
        gap = self.text_width(number, "mono", 13) + 16
        self.text(32 + gap, 28, title, "mono", 13, "ink")
        if note:
            self.text(self.w - 32, 28, note, "mono", 13, "muted", anchor="rt")
        self.line(32, 52, self.w - 32, 52, "rule", 1)

    def save(self, path: Path, size: tuple[int, int] | None = None) -> None:
        image = self.image
        if size is not None:
            image = image.resize(size, Image.Resampling.LANCZOS)
        path.parent.mkdir(parents=True, exist_ok=True)
        image.save(path, "PNG")


class SvgPlate:
    """Same coordinate system at 1x, text converted to outlines."""

    def __init__(self, width: int, height: int, theme: dict) -> None:
        self.w = width
        self.h = height
        self.theme = theme
        self.scale = 1
        self.shapes: list[str] = []
        self._measure: dict[tuple[str, int], ImageFont.FreeTypeFont] = {}

    def font(self, face: str, size: int) -> ImageFont.FreeTypeFont:
        key = (face, size)
        if key not in self._measure:
            self._measure[key] = ImageFont.truetype(str(FACE[face]), size)
        return self._measure[key]

    def text_width(self, text: str, face: str, size: int) -> float:
        return float(self.font(face, size).getlength(text))

    def text(self, x: float, y: float, text: str, face: str, size: int, fill: str, anchor: str = "lt") -> None:
        if not text:
            return
        width = self.text_width(text, face, size)
        ascent, _descent = self.font(face, size).getmetrics()
        if anchor == "mm":
            x = x - width / 2
            y = y - (ascent / 2)
        elif anchor == "mt":
            x = x - width / 2
        elif anchor == "rt":
            x = x - width
        elif anchor == "rm":
            x = x - width
            y = y - ascent / 2
        baseline = y + ascent
        # Pillow's advance includes hinting differences. Scale the outline
        # width to the measured width so the SVG matches the PNG.
        raw = OUTLINERS[face].width(text, size)
        scale_x = (width / raw) if raw else 1.0
        pen_x = x
        commands: list[str] = []
        upem = OUTLINERS[face].upem
        glyph_scale = size / upem
        for ch in text:
            name = OUTLINERS[face].cmap.get(ord(ch))
            if name is None:
                pen_x += size * 0.5
                continue
            pen = SVGPathPen(OUTLINERS[face].glyphs)
            transform = TransformPen(
                pen,
                (glyph_scale * scale_x, 0, 0, -glyph_scale, pen_x, baseline),
            )
            OUTLINERS[face].glyphs[name].draw(transform)
            command = pen.getCommands()
            if command:
                commands.append(command)
            advance, _lsb = OUTLINERS[face].hmtx[name]
            pen_x += advance * glyph_scale * scale_x
        if commands:
            path = " ".join(commands)
            self.shapes.append(
                f'<path d="{path}" fill="{hex_of(self.theme[fill])}"/>'
            )

    def tracked(self, x: float, y: float, text: str, face: str, size: int, fill: str, em: float = 0.08) -> float:
        extra = size * em
        cursor = x
        for ch in text:
            self.text(cursor, y, ch, face, size, fill)
            cursor += self.text_width(ch, face, size) + extra
        return cursor

    def line(self, x1: float, y1: float, x2: float, y2: float, fill: str = "ink", width: float = 1) -> None:
        self.shapes.append(
            f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
            f'stroke="{hex_of(self.theme[fill])}" stroke-width="{width}" shape-rendering="crispEdges"/>'
        )

    def rect(self, x: float, y: float, w: float, h: float, fill: str | None = None, outline: str | None = None, width: float = 1) -> None:
        fill_attr = hex_of(self.theme[fill]) if fill else "none"
        stroke_attr = hex_of(self.theme[outline]) if outline else "none"
        stroke_width = width if outline else 0
        self.shapes.append(
            f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" '
            f'fill="{fill_attr}" stroke="{stroke_attr}" stroke-width="{stroke_width}" shape-rendering="crispEdges"/>'
        )

    def head(self, number: str, title: str, note: str = "") -> None:
        self.text(32, 28, number, "mono", 13, "accent")
        gap = self.text_width(number, "mono", 13) + 16
        self.text(32 + gap, 28, title, "mono", 13, "ink")
        if note:
            self.text(self.w - 32, 28, note, "mono", 13, "muted", anchor="rt")
        self.line(32, 52, self.w - 32, 52, "rule", 1)

    def save(self, path: Path, size: tuple[int, int] | None = None) -> None:
        del size
        body = "\n".join(self.shapes)
        svg = (
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
            f'viewBox="0 0 {self.w} {self.h}">\n'
            f'<rect width="100%" height="100%" fill="{hex_of(self.theme["paper"])}"/>\n'
            f"{body}\n</svg>\n"
        )
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(svg, encoding="utf-8")


def banner(plate: Plate, data: dict) -> None:
    plate.tracked(32, 36, "SRMS", "sans-med", 48, "ink", 0.08)
    plate.text(plate.w - 32, 58, "1.0.0", "mono", 13, "muted", anchor="rt")
    plate.rect(32, 100, 64, 2, fill="accent")
    plate.text(32, 124, "Student records,", "sans", 36, "ink")
    plate.text(32, 172, "kept in order.", "sans", 36, "ink")
    plate.text(32, 248, "A terminal register for one cohort of examination records.", "sans", 15, "ink")
    plate.text(32, 272, "The smallest free roll is assigned. The file carries a checksum.", "sans", 15, "muted")
    plate.line(32, plate.h - 48, plate.w - 32, plate.h - 48, "rule", 1)
    plate.text(32, plate.h - 36, "Prafull Joshi", "sans", 13, "ink")
    plate.text(plate.w - 32, plate.h - 36, "C11", "mono", 13, "muted", anchor="rt")

    rows = data["top"]
    tx = 760
    plate.text(tx, 140, "Roll", "mono", 11, "muted")
    plate.text(tx + 64, 140, "Name", "mono", 11, "muted")
    plate.text(tx + 280, 140, "Pct", "mono", 11, "muted", anchor="rt")
    plate.line(tx, 160, tx + 280, 160, "rule", 1)
    y = 172
    for row in rows:
        plate.text(tx, y, f"{row['roll']:04d}", "mono", 13, "ink")
        plate.text(tx + 64, y, row["name"], "sans", 13, "ink")
        plate.text(tx + 280, y, f"{row['pct']:.1f}", "mono", 13, "ink", anchor="rt")
        y += 24
    plate.text(tx, y + 8, "Highest three, sample cohort", "sans", 11, "muted")


def grade_chart(plate: Plate, data: dict) -> None:
    plate.head("Fig. 1", "Grade distribution", "n = 24")
    labels = ["A", "B", "C", "D", "E", "F"]
    counts = [data["grades"][label] for label in labels]
    maximum = max(counts)
    left = 80
    right = plate.w - 80
    top = 88
    row = 48
    bar_h = 18
    span = right - left
    for index, label in enumerate(labels):
        y = top + index * row
        count = counts[index]
        plate.text(56, y + 1, label, "sans-med", 15, "ink", anchor="mt")
        width = 0 if maximum == 0 else span * (count / 8)
        # Scale is fixed at 8, the top of the sample, so the axis does not move.
        plate.rect(left, y, width, bar_h, fill="ink")
        plate.text(left + width + 8, y + 1, str(count), "mono", 13, "ink")
    axis_y = top + len(labels) * row + 8
    plate.line(left, axis_y, left + span, axis_y, "ink", 1)
    for tick in (0, 2, 4, 6, 8):
        x = left + span * (tick / 8)
        plate.line(x, axis_y, x, axis_y + 6, "ink", 1)
        plate.text(x, axis_y + 10, str(tick), "mono", 11, "muted", anchor="mt")
    plate.text(left + span / 2, plate.h - 28, "students", "sans", 11, "muted", anchor="mt")


def percentage_chart(plate: Plate, data: dict) -> None:
    plate.head("Fig. 2", "Percentage by rank", "")
    mean_label = f"mean {data['average']:.1f}"
    plate.text(plate.w - 32, 28, mean_label, "mono", 13, "accent", anchor="rt")
    mean_w = plate.text_width(mean_label, "mono", 13)
    plate.text(plate.w - 32 - mean_w - 16, 28, "n = 24", "mono", 13, "muted", anchor="rt")
    records = sorted(data["records"], key=lambda row: (-row["pct"], row["name"], row["roll"]))
    left = 200
    right = plate.w - 72
    top = 72
    plot_h = 24 * 24
    mean = data["average"]
    for index, row in enumerate(records):
        y = top + index * 24
        plate.text(32, y + 4, row["name"], "sans", 13, "ink")
        bar_w = (right - left) * (row["pct"] / 100.0)
        plate.rect(left, y + 6, bar_w, 8, fill="ink")
        plate.text(plate.w - 32, y + 4, f"{row['pct']:.1f}", "mono", 13, "ink", anchor="rt")
    axis_y = top + plot_h + 8
    plate.line(left, axis_y, right, axis_y, "ink", 1)
    for tick in (0, 25, 50, 75, 100):
        x = left + (right - left) * (tick / 100.0)
        plate.line(x, axis_y, x, axis_y + 6, "ink", 1)
        plate.text(x, axis_y + 10, str(tick), "mono", 11, "muted", anchor="mt")
    mean_x = left + (right - left) * (mean / 100.0)
    plate.line(mean_x, 68, mean_x, axis_y, "accent", 2)
    plate.text(left + (right - left) / 2, plate.h - 24, "percentage", "sans", 11, "muted", anchor="mt")


def summary_table(plate: Plate, data: dict) -> None:
    plate.head("Fig. 3", "Cohort summary", "sample")
    rows = [
        ("Students", f"{data['count']}"),
        ("Average", f"{data['average']:.1f}"),
        ("Median", f"{data['median']:.1f}"),
        (
            "Highest",
            f"{data['highest_tenths']:.1f}    {data['highest_grade']}    {data['highest_name']}",
        ),
        (
            "Lowest",
            f"{data['lowest_tenths']:.1f}    {data['lowest_grade']}    {data['lowest_name']}",
        ),
    ]
    y = 80
    plate.text(32, y, "Measure", "sans", 13, "muted")
    plate.text(240, y, "Value", "sans", 13, "muted")
    plate.line(32, y + 22, plate.w - 32, y + 22, "ink", 1)
    y = 116
    for label, value in rows:
        plate.text(32, y, label, "sans", 15, "ink")
        plate.text(240, y, value, "mono", 15, "ink")
        y += 36
        plate.line(32, y - 10, plate.w - 32, y - 10, "rule", 1)
    plate.text(32, plate.h - 32, "Grades  A from 90   B from 80   C from 70   D from 60   E from 50   F below 50", "sans", 13, "muted")


def modules(plate: Plate) -> None:
    plate.head("Fig. 4", "Modules", "one process")
    boxes = [
        (400, 78, "main", "main.c"),
        (400, 168, "app", "app.c"),
        (48, 280, "validate", "validate.c"),
        (232, 280, "registry", "registry.c"),
        (416, 280, "stats", "stats.c"),
        (600, 280, "ui", "ui.c"),
        (784, 280, "csv", "csv_io.c"),
        (156, 400, "student", "student.c"),
        (320, 400, "storage", "storage.c"),
    ]
    centers = {}
    for x, y, name, filename in boxes:
        accent = name == "registry"
        plate.rect(x, y, 148, 64, outline="accent" if accent else "ink", width=2 if accent else 1)
        plate.text(x + 74, y + 16, name, "sans", 15, "ink", anchor="mt")
        plate.text(x + 74, y + 38, filename, "mono", 11, "muted", anchor="mt")
        centers[name] = (x + 74, y, y + 64, x, x + 148)

    def vlink(name_a: str, name_b: str) -> None:
        ax, atop, abot, _al, _ar = centers[name_a]
        bx, btop, _bbot, _bl, _br = centers[name_b]
        plate.line(ax, abot, bx, btop, "ink", 1)

    def hstub(name_a: str, name_b: str, y: float) -> None:
        ax, _at, _ab, _al, _ar = centers[name_a]
        bx, _bt, _bb, _bl, _br = centers[name_b]
        plate.line(ax, y, bx, y, "ink", 1)

    vlink("main", "app")
    # Bus under app, down to the five siblings.
    bus_y = 248
    plate.line(centers["app"][0], centers["app"][2], centers["app"][0], bus_y, "ink", 1)
    hstub("validate", "csv", bus_y)
    for name in ("validate", "registry", "stats", "ui", "csv"):
        plate.line(centers[name][0], bus_y, centers[name][0], centers[name][1], "ink", 1)
    bus_y = 376
    plate.line(centers["registry"][0], centers["registry"][2], centers["registry"][0], bus_y, "ink", 1)
    plate.line(centers["student"][0], bus_y, centers["storage"][0], bus_y, "ink", 1)
    plate.line(centers["student"][0], bus_y, centers["student"][0], centers["student"][1], "ink", 1)
    plate.line(centers["storage"][0], bus_y, centers["storage"][0], centers["storage"][1], "ink", 1)


def data_flow(plate: Plate) -> None:
    plate.head("Fig. 5", "Data flow", "one session")
    boxes = [
        (32, 150, 150, "operator", "keyboard"),
        (230, 150, 150, "validate", "fields"),
        (428, 150, 160, "registry", "linked list"),
        (660, 88, 250, "students.srms", "version, count, CRC-32"),
        (660, 230, 250, "cohort.csv", "RFC 4180"),
    ]
    info = {}
    for x, y, w, name, sub in boxes:
        accent = name == "registry"
        plate.rect(x, y, w, 72, outline="accent" if accent else "ink", width=2 if accent else 1)
        plate.text(x + w / 2, y + 18, name, "sans", 15, "ink", anchor="mt")
        plate.text(x + w / 2, y + 40, sub, "mono", 11, "muted", anchor="mt")
        info[name] = (x, y, w, 72)

    def arrow(x1: float, y1: float, x2: float, y2: float) -> None:
        plate.line(x1, y1, x2, y2, "ink", 1)
        plate.line(x2 - 8, y2 - 4, x2, y2, "ink", 1)
        plate.line(x2 - 8, y2 + 4, x2, y2, "ink", 1)

    def elbow(x1: float, y1: float, x2: float, y2: float) -> None:
        join = x1 + 36
        plate.line(x1, y1, join, y1, "ink", 1)
        plate.line(join, y1, join, y2, "ink", 1)
        arrow(join, y2, x2, y2)

    op, va, reg, store, csv = (info[k] for k in ("operator", "validate", "registry", "students.srms", "cohort.csv"))

    def mid_y(box: tuple) -> float:
        return box[1] + box[3] / 2

    arrow(op[0] + op[2], mid_y(op), va[0], mid_y(va))
    arrow(va[0] + va[2], mid_y(va), reg[0], mid_y(reg))
    elbow(reg[0] + reg[2], reg[1] + 20, store[0], mid_y(store))
    elbow(reg[0] + reg[2], reg[1] + reg[3] - 20, csv[0], mid_y(csv))
    plate.text(300, 128, "checked text", "sans", 11, "muted")
    plate.text(reg[0] + reg[2] + 8, reg[1] + 4, "atomic write", "sans", 11, "muted")
    plate.text(reg[0] + reg[2] + 8, reg[1] + reg[3] + 6, "export", "sans", 11, "muted")
    plate.text(32, plate.h - 36, "Statistics are computed from the list in memory. They are not stored.", "sans", 13, "muted")


def render_pair(width: int, height: int, draw, stem: str, data: dict, svg: bool = False) -> None:
    for theme, suffix in ((LIGHT, "light"), (DARK, "dark")):
        plate = Plate(width, height, theme)
        draw(plate, data)
        plate.save(OUT / f"{stem}-{suffix}.png")
    if svg:
        vector = SvgPlate(width, height, LIGHT)
        draw(vector, data)
        vector.save(OUT / f"{stem}.svg")


def main() -> None:
    for path in FACE.values():
        if not path.is_file():
            raise SystemExit(f"missing font {path}")
    if not BINARY.is_file():
        raise SystemExit("build ./build/srms first")
    data = load_report()
    OUT.mkdir(parents=True, exist_ok=True)

    for theme, suffix in ((LIGHT, "light"), (DARK, "dark")):
        plate = Plate(1200, 440, theme)
        banner(plate, data)
        plate.save(OUT / f"banner-{suffix}.png")

    social = Plate(1280, 640, LIGHT)
    banner_social(social, data)
    social.save(SOCIAL, size=(1280, 640))

    render_pair(880, 460, lambda plate, report: grade_chart(plate, report), "fig-01-grades", data)
    render_pair(880, 760, lambda plate, report: percentage_chart(plate, report), "fig-02-percentage", data)
    render_pair(880, 380, lambda plate, report: summary_table(plate, report), "fig-03-summary", data)
    render_pair(960, 520, lambda plate, _report: modules(plate), "fig-04-modules", data, svg=True)
    render_pair(960, 380, lambda plate, _report: data_flow(plate), "fig-05-flow", data, svg=True)

    contact(data)
    print(f"wrote plates under {OUT}")


def banner_social(plate: Plate, data: dict) -> None:
    plate.tracked(48, 48, "SRMS", "sans-med", 48, "ink", 0.08)
    plate.rect(48, 112, 64, 2, fill="accent")
    plate.text(48, 136, "Student records,", "sans", 36, "ink")
    plate.text(48, 184, "kept in order.", "sans", 36, "ink")
    plate.text(48, 260, "A terminal register for one cohort", "sans", 18, "ink")
    plate.text(48, 288, "of examination records.", "sans", 18, "ink")
    plate.line(48, 560, plate.w - 48, 560, "rule", 1)
    plate.text(48, 584, "Prafull Joshi", "sans", 15, "ink")
    plate.text(plate.w - 48, 584, "C11", "mono", 15, "muted", anchor="rt")

    plate.text(720, 56, "Fig. 1", "mono", 13, "accent")
    plate.text(784, 56, "Grade distribution", "mono", 13, "ink")
    plate.text(plate.w - 48, 56, "n = 24", "mono", 13, "muted", anchor="rt")
    labels = ["A", "B", "C", "D", "E", "F"]
    left = 760
    span = 400
    for index, label in enumerate(labels):
        y = 100 + index * 64
        count = data["grades"][label]
        plate.text(720, y, label, "sans-med", 15, "ink")
        plate.rect(left, y, span * (count / 8.0), 16, fill="ink")
        plate.text(left + span * (count / 8.0) + 8, y - 1, str(count), "mono", 13, "ink")


def contact(_data: dict) -> None:
    names = [
        "banner-light.png",
        "fig-01-grades-light.png",
        "fig-02-percentage-light.png",
        "fig-03-summary-light.png",
        "fig-04-modules-light.png",
        "fig-05-flow-light.png",
        "banner-dark.png",
        "fig-01-grades-dark.png",
        "fig-02-percentage-dark.png",
        "fig-03-summary-dark.png",
        "fig-04-modules-dark.png",
        "fig-05-flow-dark.png",
    ]
    thumbs = []
    for name in names:
        image = Image.open(OUT / name).convert("RGB")
        image.thumbnail((640, 400), Image.Resampling.LANCZOS)
        thumbs.append(image)
    width = 40 + 3 * (640 + 16)
    height = 40 + 4 * (400 + 16)
    sheet = Image.new("RGB", (width, height), (255, 255, 255))
    for index, image in enumerate(thumbs):
        col = index % 3
        row = index // 3
        sheet.paste(image, (20 + col * (640 + 16), 20 + row * (400 + 16)))
    sheet.save("/tmp/srms-contact.png", "PNG")


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        sys.exit(1)
