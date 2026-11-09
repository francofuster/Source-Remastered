"""Generates the images of the on-screen touch controls.

Writes Android/data/touch/*.png. Gradle packs Android/data into the game data
of the APK, so the engine finds them as touch/<name> (cl_touch.c).

Shapes are white or grey so cl_touch.c can tint them with re.SetColor; the
discs carry their own dark fill. Run it again after changing a label:

    python Android/tools/make_touch_art.py
"""
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'data', 'touch')
SS = 4  # supersampling for smooth edges

FONT = r'C:\Windows\Fonts\bahnschrift.ttf'
FONT_STYLE = 'Bold SemiCondensed'
FALLBACK_FONT = r'C:\Windows\Fonts\segoeuib.ttf'

# Labels from the proposal (button -> text)
LABELS = {
    'ki': 'KI',
    'strong': 'STRONG',
    'z': 'Z',
    'guard': 'GUARD',
    'lock': 'LOCK',
    'aura': 'AURA',
    'skill': 'SKILL',
    # Contexts (phase F3): the same buttons change what they do
    'combo': 'COMBO',
    'break': 'BREAK',
    'launch': 'LAUNCH',
    'counter': 'COUNTER',
    'dodge': 'DODGE',
    'chase': 'CHASE',
    'back': 'RETURN',
    'brake': 'BRAKE',
    'recover': 'RECOVER',
    'push': 'PUSH',
}


def canvas(w, h):
    return Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))


def finish(img, w, h, name):
    img = img.resize((w, h), Image.LANCZOS)
    img.save(os.path.join(OUT, name + '.png'))


def disc():
    """Dark translucent fill, a little darker at the edge."""
    n = 256
    img = canvas(n, n)
    d = ImageDraw.Draw(img)
    c = n * SS / 2
    steps = 48
    for i in range(steps):
        t = i / (steps - 1)            # 0 at the edge, 1 at the center
        r = c * (1 - t) + 1
        a = int(165 - 50 * t)
        shade = int(14 + 16 * t)
        d.ellipse((c - r, c - r, c + r, c + r), fill=(shade, shade + 4, shade + 22, a))
    finish(img, n, n, 'disc')


def ring(width_frac=0.075, name='ring'):
    n = 256
    img = canvas(n, n)
    d = ImageDraw.Draw(img)
    s = n * SS
    w = int(s * width_frac)
    d.ellipse((w // 2, w // 2, s - w // 2, s - w // 2), outline=(255, 255, 255, 255), width=w)
    finish(img, n, n, name)


def glow():
    """Soft white halo for a pressed button."""
    n = 256
    img = Image.new('RGBA', (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse((40, 40, n - 40, n - 40), fill=(255, 255, 255, 200))
    img = img.filter(ImageFilter.GaussianBlur(26))
    img.save(os.path.join(OUT, 'glow.png'))


def knob():
    n = 256
    img = canvas(n, n)
    d = ImageDraw.Draw(img)
    c = n * SS / 2
    steps = 40
    for i in range(steps):
        t = i / (steps - 1)
        r = c * (1 - t) * 0.97 + 1
        v = int(205 + 50 * t)
        d.ellipse((c - r, c - r, c + r, c + r), fill=(v, v, v, 235))
    finish(img, n, n, 'knob')


def stickbase():
    """Ring with 8 ticks: with lock-on the stick reads 8 directions."""
    n = 256
    img = canvas(n, n)
    d = ImageDraw.Draw(img)
    s = n * SS
    c = s / 2
    d.ellipse((0, 0, s, s), fill=(16, 20, 40, 110))
    w = int(s * 0.03)
    d.ellipse((w, w, s - w, s - w), outline=(255, 255, 255, 200), width=w)
    import math
    for k in range(8):
        a = k * math.pi / 4
        r1, r2 = c * 0.80, c * 0.92
        x1, y1 = c + r1 * math.cos(a), c + r1 * math.sin(a)
        x2, y2 = c + r2 * math.cos(a), c + r2 * math.sin(a)
        d.line((x1, y1, x2, y2), fill=(255, 255, 255, 170), width=int(s * 0.022))
    finish(img, n, n, 'stickbase')


def pill():
    """VUELO: a capsule split in an upper and a lower half."""
    w, h = 128, 256
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    W, H = w * SS, h * SS
    lw = int(W * 0.07)
    d.rounded_rectangle((lw // 2, lw // 2, W - lw // 2, H - lw // 2), radius=W // 2,
                        fill=(16, 20, 40, 150), outline=(255, 255, 255, 230), width=lw)
    d.line((W * 0.18, H / 2, W * 0.82, H / 2), fill=(255, 255, 255, 150), width=int(lw * 0.6))
    finish(img, w, h, 'pill')


def arrow(up, name):
    n = 128
    img = canvas(n, n)
    d = ImageDraw.Draw(img)
    s = n * SS
    if up:
        pts = [(s * 0.5, s * 0.22), (s * 0.82, s * 0.72), (s * 0.18, s * 0.72)]
    else:
        pts = [(s * 0.5, s * 0.78), (s * 0.82, s * 0.28), (s * 0.18, s * 0.28)]
    d.polygon(pts, fill=(255, 255, 255, 255))
    finish(img, n, n, name)


def pause():
    n = 128
    img = canvas(n, n)
    d = ImageDraw.Draw(img)
    s = n * SS
    for x in (0.30, 0.58):
        d.rounded_rectangle((s * x, s * 0.26, s * (x + 0.12), s * 0.74), radius=s * 0.03,
                            fill=(255, 255, 255, 255))
    finish(img, n, n, 'pause')


def load_font(size):
    try:
        f = ImageFont.truetype(FONT, size)
        f.set_variation_by_name(FONT_STYLE)
        return f
    except Exception:
        return ImageFont.truetype(FALLBACK_FONT, size)


def label(key, text):
    """White text with a dark outline, centered on a 256x128 image."""
    w, h = 256, 128
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    size = 92 * SS if len(text) <= 2 else 64 * SS
    font = load_font(size)
    while True:
        box = d.textbbox((0, 0), text, font=font, stroke_width=6 * SS)
        if box[2] - box[0] <= w * SS * 0.94 or size < 20 * SS:
            break
        size -= 2 * SS
        font = load_font(size)
    tw, th = box[2] - box[0], box[3] - box[1]
    x = (w * SS - tw) / 2 - box[0]
    y = (h * SS - th) / 2 - box[1]
    d.text((x, y), text, font=font, fill=(255, 255, 255, 255),
           stroke_width=5 * SS, stroke_fill=(0, 0, 0, 150))
    finish(img, w, h, 'label_' + key)


def main():
    os.makedirs(OUT, exist_ok=True)
    disc()
    ring()
    glow()
    knob()
    stickbase()
    pill()
    arrow(True, 'arrow_up')
    arrow(False, 'arrow_down')
    pause()
    for key, text in LABELS.items():
        label(key, text)
    print('touch art written to', os.path.normpath(OUT))


if __name__ == '__main__':
    main()
