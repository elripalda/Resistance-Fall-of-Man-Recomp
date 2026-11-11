"""Draws the R1 launcher's sidebar picture (original art, no game assets).

    python tools/art/make_r1_art.py [--preview DIR]

writes
    project/launcher_side.png  the launcher sidebar picture (embedded as RCDATA 2 by project/launcher.rc)

A grey English town at dusk: terraced houses with chimney pots, a church tower,
and a dark alien tower on the horizon (x = 307 of 480, the launcher crops around
it) lit by amber lights, with smoke and drifting embers. Everything is drawn with
PIL from shapes and noise (fixed seed, so the output is reproducible).
The launcher's icon (project/launcher.ico) is not touched.
"""
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, '..', '..', 'project'))

# palette (shared with project/launcher.cpp)
SIDEBAR = (17, 18, 20)
ORANGE = (222, 130, 44)       # accent
ORANGE_HOT = (255, 214, 150)


def vgrad(w, h, top, bot, a=255):
    im = Image.new('RGBA', (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line((0, y, w, y), fill=tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3)) + (a,))
    return im


def glow(img, mask, layers):
    for blur, gain, col in layers:
        g = mask.filter(ImageFilter.GaussianBlur(blur)).point(lambda v, k=gain: min(255, int(v * k)))
        lay = Image.new('RGBA', img.size, col + (0,))
        lay.putalpha(g)
        img = Image.alpha_composite(img, lay)
    return img


def tower_mask(w, h, cx, base_y, height, width):
    """A blocky, stepped alien tower with a pointed crown and side fins."""
    m = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(m)
    top = base_y - height
    d.polygon([(cx - width * 0.5, base_y), (cx - width * 0.32, base_y - height * 0.25),
               (cx - width * 0.2, base_y - height * 0.7), (cx - width * 0.08, top + height * 0.08), (cx, top),
               (cx + width * 0.08, top + height * 0.08), (cx + width * 0.2, base_y - height * 0.7),
               (cx + width * 0.32, base_y - height * 0.25), (cx + width * 0.5, base_y)], fill=255)
    for k, s in ((0.3, 0.62), (0.55, 0.42)):            # fins
        y = base_y - height * k
        d.polygon([(cx - width * s, y + height * 0.05), (cx - width * 0.15, y - height * 0.04),
                   (cx + width * 0.15, y - height * 0.04), (cx + width * s, y + height * 0.05)], fill=255)
    return m


def draw_sidebar(w=480, h=1000):
    rnd = random.Random(1951)
    img = vgrad(w, h, (26, 24, 26), (84, 62, 46))
    horizon = int(h * 0.70)
    # low, heavy clouds lit amber from below
    clouds = Image.new('L', (w, h), 0)
    cd = ImageDraw.Draw(clouds)
    for _ in range(150):
        cx, cy = rnd.uniform(-60, w + 60), rnd.uniform(0, horizon * 0.85)
        rx, ry = rnd.uniform(40, 160), rnd.uniform(8, 24)
        cd.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=rnd.randrange(20, 70))
    clouds = clouds.filter(ImageFilter.GaussianBlur(14))
    cl = Image.new('RGBA', (w, h), (130, 112, 100, 0))
    cl.putalpha(clouds)
    img = Image.alpha_composite(img, cl)
    # fires on the horizon
    burn = Image.new('L', (w, h), 0)
    ImageDraw.Draw(burn).ellipse((-120, horizon - 90, w + 120, horizon + 70), fill=120)
    img = glow(img, burn, ((60, 1.0, (150, 70, 26)),))
    sx = 307
    tw_h = int(h * 0.44)
    halo = Image.new('L', (w, h), 0)
    ImageDraw.Draw(halo).ellipse((sx - 150, horizon - tw_h - 90, sx + 150, horizon - tw_h + 150), fill=90)
    img = glow(img, halo, ((70, 1.0, (130, 70, 30)),))
    tm = tower_mask(w, h, sx, horizon, tw_h, 120)
    far = Image.new('RGBA', (w, h), (30, 28, 30, 0))
    far.putalpha(tm)
    img = Image.alpha_composite(img, far)
    lights = Image.new('L', (w, h), 0)
    ld = ImageDraw.Draw(lights)
    for k in (0.2, 0.38, 0.55, 0.72, 0.93):
        y = horizon - tw_h * k
        ld.rectangle((sx - 3, y - 2, sx + 3, y + 2), fill=255)
    img = glow(img, lights, ((8, 1.3, ORANGE), (1, 1.0, ORANGE_HOT)))
    # smoke columns
    smoke = Image.new('L', (w, h), 0)
    sd = ImageDraw.Draw(smoke)
    for bx in (70, 190, 420):
        x, y = bx, horizon
        for i in range(40):
            r = 10 + i * 1.6
            sd.ellipse((x - r, y - r * 0.7, x + r, y + r * 0.7), fill=40)
            x += rnd.uniform(-2, 6)
            y -= rnd.uniform(9, 14)
    smoke = smoke.filter(ImageFilter.GaussianBlur(10))
    sl = Image.new('RGBA', (w, h), (44, 40, 40, 0))
    sl.putalpha(smoke)
    img = Image.alpha_composite(img, sl)
    # haze over the horizon
    fm = Image.new('L', (w, h), 0)
    fd = ImageDraw.Draw(fm)
    for y in range(h):
        t = 1 - min(1.0, abs(y - horizon) / (h * 0.12))
        fd.line((0, y, w, y), fill=int(110 * t * t))
    fog = Image.new('RGBA', (w, h), (110, 92, 80, 0))
    fog.putalpha(fm)
    img = Image.alpha_composite(img, fog)
    # the town: terraced rows with chimney pots, a church tower, a gasholder, broken roofs
    town = Image.new('L', (w, h), 0)
    td = ImageDraw.Draw(town)
    td.rectangle((0, horizon + 30, w, h), fill=255)     # the street level
    x = -10
    while x < w + 10:
        kind = rnd.random()
        base = horizon + rnd.randrange(8, 30)
        if kind < 0.6:                                   # a terrace: several houses sharing a roof line
            n = rnd.randrange(2, 5)
            hw = rnd.randrange(18, 26)
            wall = rnd.randrange(24, 36)
            bw = n * hw
            td.rectangle((x, base - wall, x + bw, h), fill=255)
            peak = base - wall - rnd.randrange(10, 18)
            if rnd.random() < 0.3:                       # bombed-out end
                td.polygon([(x, base - wall), (x + bw * 0.3, peak), (x + bw * 0.6, peak + 4),
                            (x + bw * 0.72, base - wall + 10), (x + bw, base - wall)], fill=255)
            else:
                td.polygon([(x - 2, base - wall), (x + 6, peak), (x + bw - 6, peak), (x + bw + 2, base - wall)], fill=255)
            for i in range(n + 1):                       # chimney stacks with pots
                cx = x + i * hw - 3
                td.rectangle((cx, peak - 12, cx + 7, peak + 2), fill=255)
                td.rectangle((cx + 1, peak - 16, cx + 2, peak - 12), fill=255)
                td.rectangle((cx + 4, peak - 16, cx + 5, peak - 12), fill=255)
        elif kind < 0.72:                                # church tower with a short spire
            tw = rnd.randrange(20, 28)
            td.rectangle((x, base - 96, x + tw, h), fill=255)
            for i in range(4):                           # battlements
                td.rectangle((x + i * tw / 4, base - 102, x + i * tw / 4 + tw / 8, base - 96), fill=255)
            td.polygon([(x + 3, base - 96), (x + tw / 2, base - 150), (x + tw - 3, base - 96)], fill=255)
            bw = tw
        elif kind < 0.82:                                # gasholder frame
            bw = rnd.randrange(44, 60)
            td.rectangle((x + 4, base - 50, x + bw - 4, h), fill=255)
            for lx in range(int(x), int(x + bw) + 1, max(1, bw // 4)):
                td.line((lx, base - 74, lx, base - 50), fill=255, width=2)
            td.line((x, base - 74, x + bw, base - 74), fill=255, width=2)
        else:                                            # a mill chimney
            bw = rnd.randrange(30, 44)
            td.rectangle((x, base - 40, x + bw, h), fill=255)
            td.polygon([(x + bw * 0.55, base - 40), (x + bw * 0.62, base - 150), (x + bw * 0.74, base - 150),
                        (x + bw * 0.82, base - 40)], fill=255)
        x += bw + rnd.randrange(-2, 10)
    town_l = Image.new('RGBA', (w, h), (11, 11, 12, 0))
    town_l.putalpha(town)
    img = Image.alpha_composite(img, town_l)
    # a few fires in windows
    win = Image.new('L', (w, h), 0)
    wd = ImageDraw.Draw(win)
    for _ in range(16):
        wx, wy = rnd.randrange(0, w), rnd.randrange(horizon - 30, horizon + 30)
        if town.getpixel((min(w - 1, wx), min(h - 1, wy))):
            wd.rectangle((wx, wy, wx + 3, wy + 4), fill=200)
    img = glow(img, win, ((6, 1.0, ORANGE), (0.5, 1.0, ORANGE_HOT)))
    # drifting embers
    ash = Image.new('L', (w, h), 0)
    ad = ImageDraw.Draw(ash)
    for _ in range(260):
        ax, ay, r = rnd.uniform(0, w), rnd.uniform(0, h * 0.85), rnd.uniform(0.6, 1.8)
        ad.ellipse((ax - r, ay - r, ax + r, ay + r), fill=rnd.randrange(50, 190))
    ash = ash.filter(ImageFilter.GaussianBlur(0.7))
    al = Image.new('RGBA', (w, h), (255, 170, 100, 0))
    al.putalpha(ash)
    img = Image.alpha_composite(img, al)
    # the ground fades back into the sidebar colour at the bottom
    fade = Image.new('L', (w, h), 0)
    fd = ImageDraw.Draw(fade)
    for y in range(h):
        t = max(0.0, (y - h * 0.80) / (h * 0.20))
        fd.line((0, y, w, y), fill=int(255 * min(1.0, t)))
    bot = Image.new('RGBA', (w, h), SIDEBAR + (0,))
    bot.putalpha(fade)
    img = Image.alpha_composite(img, bot)
    return img.convert('RGB')


def main():
    side = draw_sidebar()
    out = os.path.join(PROJECT, 'launcher_side.png')
    side.save(out, optimize=True)
    print('wrote', out)
    if '--preview' in sys.argv:
        d = sys.argv[sys.argv.index('--preview') + 1]
        os.makedirs(d, exist_ok=True)
        side.save(os.path.join(d, 'launcher_side.png'))


if __name__ == '__main__':
    main()
