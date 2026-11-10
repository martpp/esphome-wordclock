#!/usr/bin/env python3
"""Renders the README images (animated GIFs) from the real frames of wordclock.h.

  ./test/run_tests.sh export > build/scenes.json
  python3 tools/make_gifs.py build/scenes.json docs/images      # needs Pillow (pip install pillow)
"""
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

FRONT = ["ESKISTAFÜNF", "ZEHNZWANZIG", "DREIVIERTEL", "VORWIFINACH", "HALBAELFÜNF",
         "EINSXAMZWEI", "DREIAUJVIER", "SECHSNLACHT", "SIEBENZWÖLF", "ZEHNEUNKUHR"]

# scene name -> (file name, frame step, cell size in px)
SCENES = {
    "Color set Rainbow": ("color-set-rainbow.gif", 1, 30),
    "Timer letters, word clock (5 min time-lapse)": ("timer-letters.gif", 1, 30),
    "Timer ring, word clock (5 min time-lapse)": ("timer-ring.gif", 1, 30),
    "Timer pie, analog clock (5 min time-lapse)": ("timer-pie-analog.gif", 1, 30),
    "Analog clock, 12 hours time-lapse": ("analog-clock.gif", 1, 30),
    "Animation Door": ("animation-door.gif", 2, 20),
    "Animation Doorbell": ("animation-doorbell.gif", 2, 20),
    "Animation 3D printer": ("animation-printer.gif", 2, 20),
    "Animation Washer": ("animation-washer.gif", 2, 20),
    "Animation Mail": ("animation-mail.gif", 2, 20),
    "Animation Warning": ("animation-warning.gif", 2, 20),
}

FONT_CANDIDATES = ["/usr/share/fonts/google-noto-vf/NotoSansMono[wght].ttf",
                   "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", "DejaVuSansMono-Bold.ttf"]


def load_font(size):
    for path in FONT_CANDIDATES:
        try:
            font = ImageFont.truetype(path, size)
            try:
                font.set_variation_by_name("Bold")
            except (OSError, ValueError, AttributeError):
                pass
            return font
        except OSError:
            continue
    return ImageFont.load_default()


def led_index(row, col):
    return row * 11 + (col if row % 2 == 0 else 10 - col)


def render(frame, cell, font):
    pad = cell
    w, h = 11 * cell + 2 * pad, 10 * cell + 2 * pad
    light = Image.new("RGB", (w, h), (0, 0, 0))
    base = Image.new("RGB", (w, h), (6, 6, 8))
    d_light, d_base = ImageDraw.Draw(light), ImageDraw.Draw(base)
    for row in range(10):
        for col in range(11):
            hexc = frame[led_index(row, col) * 6:led_index(row, col) * 6 + 6]
            xy = (pad + col * cell + cell / 2, pad + row * cell + cell / 2)
            ch = FRONT[row][col]
            if hexc == "000000":
                d_base.text(xy, ch, fill=(34, 35, 40), font=font, anchor="mm")
            else:
                d_light.text(xy, ch, fill=tuple(int(hexc[i:i + 2], 16) for i in (0, 2, 4)), font=font, anchor="mm")
    # four minute dots in a row centered below the letters, LEDs 110..113 from left to right
    for i, (x, y) in enumerate([(w / 2 + (k - 1.5) * cell, h - pad / 2) for k in range(4)]):
        hexc = frame[(110 + i) * 6:(110 + i) * 6 + 6]
        r = max(2, cell // 9)
        col = (34, 35, 40) if hexc == "000000" else tuple(int(hexc[j:j + 2], 16) for j in (0, 2, 4))
        (d_base if hexc == "000000" else d_light).ellipse((x - r, y - r, x + r, y + r), fill=col)
    glow = light.filter(ImageFilter.GaussianBlur(cell / 6))
    out = Image.composite(light, base, light.convert("L").point(lambda v: 255 if v > 8 else 0))
    return Image.blend(out, Image.composite(glow, out, glow.convert("L")), 0.5)


def main():
    scenes = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, (file_name, step, cell) in SCENES.items():
        scene = scenes[name]
        font = load_font(int(cell * 0.62))
        frames = [render(f, cell, font) for f in scene["frames"][::step]]
        duration = int(1000 * step / scene["fps"])
        frames[0].save(out_dir / file_name, save_all=True, append_images=frames[1:], duration=duration,
                       loop=0, optimize=True)
        print(f"{file_name}: {len(frames)} frames, {(out_dir / file_name).stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
