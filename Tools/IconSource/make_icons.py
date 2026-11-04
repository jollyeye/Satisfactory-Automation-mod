# Automated Machines: draws the Wi-Fi icon (white on transparent) used by the mod.
#   Tools/Icons/T_AM_TabIcon.png     64 x 64, the Automation tab button (like the game's tab icons)
#   Tools/Icons/T_AM_MarkerIcon.png  256 x 256, the marker above automated machines
#   Tools/Icons/T_AM_DropArrow.png   32 x 32, the arrow of our dropdowns
#   Tools/Icons/T_AM_Trash.png       32 x 32, the remove button
# Run with normal Python (needs Pillow): python make_icons.py
# The outputs are committed; Tools/create_assets.py imports them into the mod.
import math
import os

from PIL import Image, ImageDraw

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Icons")
SUPERSAMPLE = 8


def wifi(size, margin, stroke, gap, outline=0):
    """A dot and three arcs opening upwards. All lengths are fractions of the size.
    outline > 0 draws a dark rim around the white shapes (readable on bright backgrounds)."""
    big = size * SUPERSAMPLE
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    m = margin * big
    stroke_px = stroke * big
    gap_px = gap * big
    dot_r = stroke_px * 0.85
    # The whole symbol fits in a square of side (big - 2m); its centre point is near the bottom.
    cx = big / 2.0
    cy = big - m - dot_r
    radii = [dot_r + gap_px + stroke_px / 2.0 + i * (stroke_px + gap_px) for i in range(3)]
    # Arcs span 90 degrees (45 on each side of straight up).
    start, end = 225, 315

    def shapes(colour, extra):
        r_dot = dot_r + extra
        draw.ellipse([cx - r_dot, cy - r_dot, cx + r_dot, cy + r_dot], fill=colour)
        width = stroke_px + 2 * extra
        for r in radii:
            box = [cx - r - width / 2, cy - r - width / 2, cx + r + width / 2, cy + r + width / 2]
            draw.arc(box, start, end, fill=colour, width=int(round(width)))
            # Round caps at both ends.
            for a in (start, end):
                ax = cx + r * math.cos(math.radians(a))
                ay = cy + r * math.sin(math.radians(a))
                draw.ellipse([ax - width / 2, ay - width / 2, ax + width / 2, ay + width / 2], fill=colour)

    if outline > 0:
        shapes((20, 20, 20, 255), outline * big)
    shapes((255, 255, 255, 255), 0)
    return img.resize((size, size), Image.LANCZOS)


def fit_vertically(img):
    """Moves the symbol so it is centred vertically (it is drawn from the bottom up)."""
    box = img.getbbox()
    if not box:
        return img
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    top = (img.size[1] - (box[3] - box[1])) // 2
    out.paste(img.crop(box), ((img.size[0] - (box[2] - box[0])) // 2, top))
    return out


def drop_arrow(size):
    """A downward triangle, like the game's dropdown arrows."""
    big = size * SUPERSAMPLE
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.polygon([(big * 0.12, big * 0.28), (big * 0.88, big * 0.28), (big * 0.5, big * 0.78)], fill=(255, 255, 255, 255))
    return img.resize((size, size), Image.LANCZOS)


def trash(size):
    """A bin: lid with a handle, and a body with three slots, like the game's delete icon."""
    big = size * SUPERSAMPLE
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    white = (255, 255, 255, 255)
    s = big / 32.0
    draw.rectangle([12 * s, 3 * s, 20 * s, 6 * s], fill=white)              # handle
    draw.rectangle([5 * s, 6 * s, 27 * s, 9 * s], fill=white)               # lid
    draw.polygon([(7 * s, 11 * s), (25 * s, 11 * s), (23.5 * s, 29 * s), (8.5 * s, 29 * s)], fill=white)  # body
    for x in (11.5, 15, 18.5):                                              # slots
        draw.rectangle([x * s, 14 * s, (x + 2) * s, 26 * s], fill=(0, 0, 0, 0))
    return img.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    tab = fit_vertically(wifi(64, margin=0.06, stroke=0.085, gap=0.075))
    tab.save(os.path.join(OUT_DIR, "T_AM_TabIcon.png"))
    marker = fit_vertically(wifi(256, margin=0.08, stroke=0.075, gap=0.07, outline=0.018))
    marker.save(os.path.join(OUT_DIR, "T_AM_MarkerIcon.png"))
    drop_arrow(32).save(os.path.join(OUT_DIR, "T_AM_DropArrow.png"))
    trash(32).save(os.path.join(OUT_DIR, "T_AM_Trash.png"))
    print("written to", os.path.abspath(OUT_DIR))


if __name__ == "__main__":
    main()
