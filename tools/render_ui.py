#!/usr/bin/env python3
"""
Render the kiln controller screens using the exact firmware layout and the
M5GFX Font0 5x7 GLCD font (Adafruit GFX / M5GFX, BSD license).

The output is a 240x135 RGB565-quantized screen, upscaled 3x with nearest
neighbour, so it matches what the DinMeter TFT shows.
"""

from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "images"
OUT.mkdir(parents=True, exist_ok=True)

# --- M5GFX Font0 (5 columns x 8 rows per glyph, 6 px advance) -------------
FONT_HEX = (
    "00000000003e5b4f5b3e3e6b4f6b3e1c3e7c3e1c183c7e3c181c577d571c1c5e7f5e1c00183c1800ffe7c3e7ff0018241800ffe7dbe7ff30483a060e"
    "2629792926407f050507407f05253f5a3ce73c5a7f3e1c1c08081c1c3e7f14227f22145f5f005f5f06097f017f006689956a606060606094a2ffa294"
    "08047e040810207e201008082a1c08081c2a08081e101010100c1e0c1e0c30383e3830060e3e0e06000000000000005f00000007000700147f147f14"
    "242a7f2a12231308646236495620500008070300001c2241000041221c002a1c7f1c2a08083e08080080703000080808080800006060002010080402"
    "3e5149453e00427f400072494949462141494d331814127f1027454545393c4a49493141211109073649494936464949291e00001400000040340000"
    "00081422411414141414004122140802015909063e415d594e7c1211127c7f494949363e414141227f4141413e7f494949417f090909013e41415173"
    "7f0808087f00417f41002040413f017f081422417f404040407f021c027f7f0408107f3e4141413e7f090909063e4151215e7f091929462649494932"
    "03017f01033f4040403f1f2040201f3f4038403f631408146303047804036159494d43007f4141410204081020004141417f04020102044040404040"
    "000307080020545478407f284444383844444428384444287f385454541800087e090218a4a49c787f0804047800447d40002040403d007f10284400"
    "00417f40007c047804787c080404783844444438fc1824241818242418fc7c08040408485454542404043f44243c4040207c1c2040201c3c4030403c"
    "44281028444c9090907c4464544c4400083641000000770000004136080002010204023c2623263c1ea1a161123a4040207a38545455592155557941"
    "2254547842215554784020545579400c1e5272123955555559395454545939555454580000457c410002457d420001457c407d1211127df0282528f0"
    "7c545545002054547c547c0a097f4932494949323a4444443a324a4848303a4141217a3a42402078009da0a07d3d4242423d3d4040403d3c24ff2424"
    "487e4943662b2ffc2f2bff0929f620c0887e090320545479410000447d413048484a32384040227a007a0a0a727d0d19317d2629292f282629292926"
    "30484d4020380808080808080808382f10c8acba2f102834fa00007b000008142a142222142a14085500550055aa55aa55aaff55ff55ff000000ff00"
    "101010ff00141414ff001010ff00ff1010f010f0141414fc001414f700ff0000ff00ff1414f404fc141417101f10101f101f1414141f00101010f000"
    "0000001f101010101f10101010f010000000ff101010101010101010ff10000000ff140000ff00ff00001f10170000fc04f414141710171414f404f4"
    "0000ff00f714141414141414f700f7141414171410101f101f141414f4141010f010f000001f101f0000001f14000000fc140000f010f01010ff10ff"
    "141414ff141010101f00000000f010fffffffffff0f0f0f0f0ffffff0000000000ffff0f0f0f0f0f3844443844fc4a4a4a347e02020606027e027e02"
    "63554941633844443c04407e201e2006027e020299a5e7a5991c2a492a1c4c7201724c304a4d4d303048784830bc625a463d3e494949007e0101017e"
    "2a2a2a2a2a44445f444440514a444040444a51400000ff0103e080ff000008086b6b083612362436060f090f06000018180000001010003040ff0101"
    "001f01011e00191d1712003c3c3c3c0000000000"
)
FONT = bytes.fromhex(FONT_HEX)
assert len(FONT) == 1280


def rgb565(r, g, b):
    """Quantize to RGB565 and expand back, like the panel does."""
    c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    r5 = (c >> 11) & 0x1F
    g6 = (c >> 5) & 0x3F
    b5 = c & 0x1F
    r8 = (r5 << 3) | (r5 >> 2)
    g8 = (g6 << 2) | (g6 >> 4)
    b8 = (b5 << 3) | (b5 >> 2)
    return (r8, g8, b8)


C = {
    "bg":       rgb565(8, 12, 20),
    "surface":  rgb565(18, 26, 38),
    "surface2": rgb565(30, 41, 59),
    "line":     rgb565(51, 65, 85),
    "text":     rgb565(241, 245, 249),
    "muted":    rgb565(148, 163, 184),
    "cyan":     rgb565(34, 211, 238),
    "amber":    rgb565(251, 191, 36),
    "green":    rgb565(52, 211, 153),
    "red":      rgb565(248, 113, 113),
}

SCALE = 3


class Screen:
    def __init__(self):
        self.img = Image.new("RGB", (240, 135), C["bg"])
        self.d = ImageDraw.Draw(self.img)

    def fill(self, color):
        self.d.rectangle([0, 0, 239, 134], fill=color)

    def rect(self, x, y, w, h, color):
        self.d.rectangle([x, y, x + w - 1, y + h - 1], fill=color)

    def rrect(self, x, y, w, h, r, fill, outline=None):
        self.d.rounded_rectangle(
            [x, y, x + w - 1, y + h - 1],
            radius=r,
            fill=fill,
            outline=outline,
            width=1,
        )

    def hline(self, x, y, w, color):
        self.d.line([x, y, x + w - 1, y], fill=color, width=1)

    def vline(self, x, y, h, color):
        self.d.line([x, y, x, y + h - 1], fill=color, width=1)

    def line(self, x0, y0, x1, y1, color, width=1):
        self.d.line([x0, y0, x1, y1], fill=color, width=width)

    def circle(self, x, y, r, color, fill=False):
        box = [x - r, y - r, x + r, y + r]
        if fill:
            self.d.ellipse(box, fill=color)
        else:
            self.d.ellipse(box, outline=color, width=1)

    def triangle(self, p0, p1, p2, color):
        self.d.polygon([p0, p1, p2], fill=color)

    def text(self, x, y, s, color, size=1):
        for i, ch in enumerate(s):
            code = ord(ch)
            if code < 32 or code > 255:
                code = ord("?")
            base = code * 5
            gx = x + i * 6 * size
            for col in range(5):
                b = FONT[base + col]
                for bit in range(8):
                    if b & (1 << bit):
                        self.rect(gx + col * size, y + bit * size, size, size, color)

    @staticmethod
    def text_width(s, size=1):
        return len(s) * 6 * size

    def battery(self, x, y, pct):
        col = C["muted"] if pct < 0 else (C["green"] if pct > 60 else (C["amber"] if pct > 25 else C["red"]))
        self.rrect(x, y, 13, 8, 2, None, C["muted"])
        self.rect(x + 13, y + 2, 2, 4, C["muted"])
        if pct > 0:
            w = max(1, pct * 9 // 100)
            self.rect(x + 2, y + 2, w, 4, col)
        self.text(x + 19, y, f"{pct}%" if pct >= 0 else "--", col, 1)

    def top_bar(self, title, pct=78):
        self.rect(0, 0, 240, 17, C["surface"])
        self.text(6, 4, title, C["cyan"], 1)
        self.battery(192, 4, pct)
        self.hline(0, 17, 240, C["line"])

    def save(self, name):
        out = self.img.resize((240 * SCALE, 135 * SCALE), Image.NEAREST)
        out.save(OUT / name, optimize=True)


# ---------------------------------------------------------------------------
# Main menu + action popup
# ---------------------------------------------------------------------------
PROGRAMS = ["Prova", "Default", "Vecchio"]
SELECTED = 1
TEMP = 23
BATTERY = 78


def main_menu(sc):
    sc.fill(C["bg"])
    sc.top_bar("M5 KILN", BATTERY)

    # hero
    sc.rect(0, 18, 240, 48, C["bg"])
    sc.text(10, 23, "TEMP FORNO", C["muted"], 1)
    tstr = "23"
    sc.text(10, 33, tstr, C["green"], 4)
    sc.text(10 + sc.text_width(tstr, 4) + 4, 51, "C", C["muted"], 2)
    sc.rrect(176, 28, 58, 30, 6, C["surface"])
    sc.text(184, 32, "SENSORE", C["muted"], 1)
    sc.text(184, 43, "OK", C["green"], 1)

    # list
    line_h = 17
    center_y = 92
    max_idx = len(PROGRAMS) + 2
    for i in range(max_idx + 1):
        y = center_y + (i - SELECTED) * line_h - 8
        if y < 66 or y > 118:
            continue
        sel = i == SELECTED
        if sel:
            sc.rrect(6, y, 228, 16, 4, C["surface2"])
            sc.rect(6, y, 3, 16, C["cyan"])
        col = C["text"] if sel else C["muted"]
        sc.text(14, y + 4, PROGRAMS[i] if i < len(PROGRAMS) else ("Manuale" if i == len(PROGRAMS) else ("Aggiungi Programma" if i == len(PROGRAMS) + 1 else "Spegni")), col, 1)
        if sel:
            sc.text(226, y + 4, ">", C["cyan"], 1)


def program_actions(sc):
    main_menu(sc)
    sc.rrect(18, 20, 208, 112, 8, C["bg"])
    sc.rrect(16, 18, 208, 112, 8, C["surface"], C["line"])
    sc.text(24, 22, "AZIONI", C["muted"], 1)
    sc.text(24, 32, "Default", C["text"], 1)

    labels = ["Avvia", "Modifica curva", "Rinomina", "Cancella programma", "Annulla"]
    selected = 2
    for i, label in enumerate(labels):
        y = 44 + i * 15
        sel = i == selected
        sc.rrect(22, y, 196, 13, 4, C["cyan"] if sel else C["surface2"], None if sel else C["line"])
        sc.text(30, y + 3, label, C["bg"] if sel else C["text"], 1)

    hint = "GIRA  SHORT OK"
    sc.text((240 - sc.text_width(hint, 1)) // 2, 120, hint, C["muted"], 1)


# ---------------------------------------------------------------------------
# RUNNING
# ---------------------------------------------------------------------------
def running(sc):
    sc.fill(C["bg"])
    sc.top_bar("Biscotto", BATTERY)

    target = 675.0
    current = 668.2
    elapsed = 5000
    total = 14400
    duty = 65
    ssr_on = True

    # row 2
    sc.rect(0, 18, 240, 18, C["bg"])
    sc.text(6, 22, "Tgt", C["muted"], 1)
    sc.text(26, 22, f"{target:.0f}C", C["amber"], 1)
    sc.text(82, 22, "Tsens", C["muted"], 1)
    sc.text(114, 22, f"{current:.1f}C", C["green"], 1)
    tstr = "1h23m/4h"
    sc.text(234 - sc.text_width(tstr, 1), 22, tstr, C["text"], 1)

    # graph card
    sc.rrect(4, 36, 232, 97, 6, C["surface"])

    # graph header
    sc.text(6, 38, "RAMPA 2/4", C["cyan"], 1)
    ssr_txt = "SSR ON 65%"
    tw = sc.text_width(ssr_txt, 1)
    sc.rrect(230 - (tw + 12), 37, tw + 12, 13, 4, C["green"])
    sc.text(230 - (tw + 12) + 6, 39, ssr_txt, C["bg"], 1)

    gx, gy, gw, gh = 28, 52, 202, 64
    tmin, tmax = 0.0, float(total)
    window = tmax - tmin

    def mx(sec):
        sec = max(tmin, min(tmax, sec))
        return gx + int((sec - tmin) / window * gw)

    def my(temp):
        temp = max(0.0, min(1200.0, temp))
        return gy + gh - 1 - int((gh - 1) * temp / 1200.0)

    # grid
    for val in (300, 600, 900):
        yy = my(val)
        sc.hline(gx, yy, gw, C["line"])
    for tick in (3600, 7200, 10800):
        xx = mx(tick)
        sc.vline(xx, gy, gh, C["line"])
    sc.hline(gx, gy + gh, gw, C["line"])
    sc.vline(gx, gy, gh, C["line"])

    # Y labels
    for val in (0, 300, 600, 900, 1200):
        yy = my(val)
        buf = f"{val:.0f}"
        tw = sc.text_width(buf, 1)
        tx = max(4, gx - tw - 2)
        ty = yy - 9 if val == 0 else yy - 4
        sc.text(tx, ty, buf, C["muted"], 1)

    # curve
    curve = [(0, 23), (3600, 500), (7200, 950), (10800, 950), (14400, 500)]
    for i in range(len(curve) - 1):
        x0, y0 = mx(curve[i][0]), my(curve[i][1])
        x1, y1 = mx(curve[i + 1][0]), my(curve[i + 1][1])
        sc.line(x0, y0, x1, y1, C["amber"], 2)

    # history (smooth plausible measured trace)
    import math
    pts = []
    for t in range(0, elapsed + 1, 40):
        if t <= 3600:
            ref = 23 + (500 - 23) * t / 3600
        else:
            ref = 500 + (950 - 500) * (t - 3600) / 3600
        hist = 23 + (ref - 23) * 0.985 + 1.2 * math.sin(t / 170.0)
        pts.append((mx(t), my(hist)))
    for i in range(len(pts) - 1):
        sc.line(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], C["green"], 1)

    # position dot
    px, py = mx(elapsed), my(target)
    sc.circle(px, py, 3, C["text"], True)
    sc.circle(px, py, 4, C["cyan"], False)

    # X labels
    for tick in (0, 3600, 7200, 10800, 14400):
        xx = mx(tick)
        sc.vline(xx, gy + gh, 3, C["line"])
        lbl = {0: "0s", 3600: "1h", 7200: "2h", 10800: "3h", 14400: "4h"}[tick]
        lx = max(gx, min(xx - 10, gx + gw - sc.text_width(lbl, 1)))
        sc.text(lx, gy + gh + 3, lbl, C["muted"], 1)


# ---------------------------------------------------------------------------
# Editor
# ---------------------------------------------------------------------------
def editor(sc):
    sc.fill(C["bg"])
    sc.top_bar("MODIFICA", BATTERY)

    sc.rrect(4, 18, 232, 62, 6, C["surface"])
    gx, gy, gw, gh = 30, 22, 204, 52
    total = 14400
    points = [(3600, 500), (7200, 950), (10800, 950), (14400, 500)]
    selected = 1
    start_temp = 23

    def xof(s):
        return gx + int(gw * s / total)

    def yof(t):
        y = gy + gh - int(gh * t / 1200.0)
        return max(gy, min(gy + gh, y))

    for val in (300, 600, 900):
        sc.hline(gx, yof(val), gw, C["line"])
    sc.hline(gx, gy + gh, gw, C["line"])
    sc.vline(gx, gy, gh, C["line"])

    p0 = (xof(0), yof(start_temp))
    sc.circle(p0[0], p0[1], 2, C["muted"], True)

    prev = p0
    for i, (s, t) in enumerate(points):
        p = (xof(s), yof(t))
        sc.line(prev[0], prev[1], p[0], p[1], C["cyan"], 2)
        if i == selected:
            sc.circle(p[0], p[1], 3, C["amber"], True)
            sc.circle(p[0], p[1], 5, C["amber"], False)
        else:
            sc.circle(p[0], p[1], 2, C["cyan"], True)
        prev = p

    # value row
    sc.rrect(4, 82, 232, 16, 4, C["surface"])
    sc.text(10, 86, "P2", C["muted"], 1)
    sc.text(34, 86, "T 950C", C["text"], 1)
    sc.text(104, 86, "t 2h", C["text"], 1)

    # buttons
    labels = ["<", "T", "t", ">", "+P", "-P"]
    for i, label in enumerate(labels):
        x = 2 + i * 39
        sel = i == 1
        sc.rrect(x, 100, 38, 16, 4, C["cyan"] if sel else C["surface"], None if sel else C["line"])
        tw = sc.text_width(label, 1)
        sc.text(x + (38 - tw) // 2, 104, label, C["bg"] if sel else C["text"], 1)
    for i, label in enumerate(("Salva", "Annulla")):
        x = 2 + i * 118
        sel = False
        sc.rrect(x, 118, 116, 16, 4, C["surface"], C["line"])
        tw = sc.text_width(label, 1)
        sc.text(x + (116 - tw) // 2, 122, label, C["text"], 1)


# ---------------------------------------------------------------------------
# Manual
# ---------------------------------------------------------------------------
def manual(sc):
    sc.fill(C["bg"])
    sc.top_bar("MANUALE", BATTERY)
    sc.rrect(12, 26, 216, 54, 8, C["surface"])
    sc.circle(38, 53, 11, C["green"], True)
    sc.text(58, 40, "SSR ON", C["green"], 3)
    sc.rect(12, 88, 216, 22, C["bg"])
    sc.text(16, 90, "Tsens 668.2C", C["green"], 2)
    sc.text(12, 120, "SHORT  Esci", C["muted"], 1)
    sc.text(132, 120, "LONG  SSR", C["muted"], 1)


# ---------------------------------------------------------------------------
# Slow rise
# ---------------------------------------------------------------------------
def slow_rise(sc):
    sc.fill(C["bg"])
    sc.rrect(6, 6, 228, 30, 6, C["surface"])
    sc.triangle((25, 11), (14, 30), (36, 30), C["amber"])
    sc.rect(24, 17, 2, 7, C["surface"])
    sc.rect(24, 26, 2, 2, C["surface"])
    sc.text(38, 14, "SALITA LENTA", C["amber"], 2)
    sc.battery(190, 12, BATTERY)

    sc.text(10, 42, "Forno 16C sotto la curva da 1h", C["text"], 1)
    options = ["Aspetta che recuperi", "Allunga di 30 min", "Annulla cottura"]
    for i, label in enumerate(options):
        y = 56 + i * 22
        sel = i == 0
        sc.rrect(8, y, 224, 20, 4, C["cyan"] if sel else C["surface"], None if sel else C["line"])
        sc.text(16, y + 6, label, C["bg"] if sel else C["text"], 1)
    sc.text(10, 124, "poi riprende la curva", C["muted"], 1)


# ---------------------------------------------------------------------------
# Rename
# ---------------------------------------------------------------------------
def rename(sc):
    sc.fill(C["bg"])
    sc.top_bar("RINOMINA", BATTERY)
    sc.text(10, 24, "ATTUALE: Default", C["muted"], 1)
    sc.rrect(8, 34, 224, 20, 4, C["surface"])
    sc.text(14, 40, "NUOVO", C["muted"], 1)
    sc.text(56, 40, "Biscotto", C["text"], 1)

    sc.rrect(8, 62, 46, 46, 8, C["surface2"])
    tw = sc.text_width("C", 4)
    sc.text(8 + (46 - tw) // 2, 69, "C", C["cyan"], 4)
    sc.text(64, 66, "CARATTERE", C["muted"], 1)
    sc.text(64, 78, "RUOTA per cambiare", C["text"], 1)
    sc.text(10, 116, "RUOTA cambia  SHORT aggiunge", C["muted"], 1)
    sc.text(10, 126, "LONG conferma  vuoto=annulla", C["muted"], 1)


def main():
    for name, fn in (
        ("main_menu.png", main_menu),
        ("program_actions.png", program_actions),
        ("running.png", running),
        ("editor.png", editor),
        ("manual.png", manual),
        ("slow_rise.png", slow_rise),
        ("rename.png", rename),
    ):
        sc = Screen()
        fn(sc)
        sc.save(name)
        print("written", OUT / name)


if __name__ == "__main__":
    main()
