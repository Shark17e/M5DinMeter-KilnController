#!/usr/bin/env python3
"""Generate the simplified block diagram used in the user manual."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import reportlab

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "images" / "wiring.png"
FONT_DIR = Path(reportlab.__file__).parent / "fonts"

BG = (248, 250, 252)
BOX = (255, 255, 255)
NAVY = (15, 23, 42)
CYAN = (8, 145, 178)
AMBER = (217, 119, 6)
RED = (220, 38, 38)
MUTED = (100, 116, 139)
WHITE = (255, 255, 255)


def font(size, bold=False):
    name = "VeraBd.ttf" if bold else "Vera.ttf"
    return ImageFont.truetype(str(FONT_DIR / name), size)


def center_text(d, xy, text, f, fill):
    lines = text.split("\n")
    heights = [d.textbbox((0, 0), line, font=f)[3] for line in lines]
    total_h = sum(heights) + (len(lines) - 1) * 6
    y = xy[1] - total_h / 2
    for line, h in zip(lines, heights):
        bbox = d.textbbox((0, 0), line, font=f)
        w = bbox[2] - bbox[0]
        d.text((xy[0] - w / 2, y), line, font=f, fill=fill)
        y += h + 6


def box(d, x, y, w, h, title, subtitle="", border=NAVY, fill=BOX, title_color=NAVY):
    d.rounded_rectangle([x, y, x + w, y + h], radius=18, fill=fill, outline=border, width=4)
    f1 = font(30, True)
    f2 = font(22)
    lines = title.split("\n")
    # title block, centered; subtitle below
    total_h = len(lines) * 36 + (16 if subtitle else 0)
    ty = y + (h - total_h) / 2 + 18
    for line in lines:
        bbox = d.textbbox((0, 0), line, font=f1)
        d.text((x + (w - (bbox[2] - bbox[0])) / 2, ty), line, font=f1, fill=title_color)
        ty += 36
    if subtitle:
        bbox = d.textbbox((0, 0), subtitle, font=f2)
        d.text((x + (w - (bbox[2] - bbox[0])) / 2, ty + 4), subtitle, font=f2, fill=MUTED)


def arrow(d, x0, y0, x1, y1, label, color=CYAN):
    d.line([x0, y0, x1, y1], fill=color, width=4)
    # arrow head
    import math
    ang = math.atan2(y1 - y0, x1 - x0)
    size = 14
    for sign in (-1, 1):
        d.line(
            [x1, y1, x1 - size * math.cos(ang + sign * 0.45), y1 - size * math.sin(ang + sign * 0.45)],
            fill=color,
            width=4,
        )
    f = font(20, True)
    bbox = d.textbbox((0, 0), label, font=f)
    tw = bbox[2] - bbox[0]
    mx = (x0 + x1) / 2
    my = (y0 + y1) / 2
    d.rounded_rectangle([mx - tw / 2 - 8, my - 16, mx + tw / 2 + 8, my + 14], radius=6, fill=BG)
    d.text((mx - tw / 2, my - 14), label, font=f, fill=color)


def main():
    img = Image.new("RGB", (1600, 820), BG)
    d = ImageDraw.Draw(img)

    d.text((70, 30), "Schema a blocchi semplificato", font=font(40, True), fill=NAVY)
    d.text((70, 86), "Controllo SSR e lettura termocoppia", font=font(24), fill=MUTED)

    # row 1
    box(d, 60, 150, 330, 130, "M5Stack DinMeter", "ESP32-S3")
    box(d, 470, 150, 330, 130, "Driver SSR", "NPN 2N2222 / BC547")
    box(d, 880, 150, 330, 130, "SSR-40 DA", "zero-cross 40 A")
    box(d, 1290, 150, 250, 130, "Resistenze", "forno 230 V AC", border=AMBER, title_color=AMBER)

    arrow(d, 390, 215, 470, 215, "GPIO1")
    arrow(d, 800, 215, 880, 215, "3-32 VDC")
    arrow(d, 1210, 215, 1290, 215, "carico", color=AMBER)

    # row 2
    box(d, 60, 430, 330, 130, "Termocoppia K", "sonda forno")
    box(d, 470, 430, 330, 130, "KMeterISO", "I2C 0x66")
    box(d, 880, 430, 330, 130, "DinMeter I2C", "SDA 13 / SCL 15")

    arrow(d, 390, 495, 470, 495, "tipo K")
    arrow(d, 800, 495, 880, 495, "I2C")

    # power note (no arrow: avoids crossing the thermocouple block)
    d.text((60, 650), "Alimentazione: USB 5 V consigliata durante la cottura; LiPo di riserva.", font=font(22), fill=NAVY)

    d.text(
        (70, 780),
        "Nota: diagramma a blocchi, non sostituisce lo schema elettrico di progetto (docs/01_hardware.md).",
        font=font(18),
        fill=MUTED,
    )
    img.save(OUT, optimize=True)
    print("written", OUT)


if __name__ == "__main__":
    main()
