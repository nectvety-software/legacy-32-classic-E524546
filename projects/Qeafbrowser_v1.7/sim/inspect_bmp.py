# inspect.py — doc BMP 240x320 cua harness S60 va in ra terminal de kiem tra
# layout + mau + do dam cua font khi khong mo duoc cua so anh.
#   python inspect.py <file.bmp> [ascii|zoom x y w h|colors]
import sys
from PIL import Image

def load(p):
    im = Image.open(p).convert("RGB")
    return im

def cls(r, g, b):
    # lop mau tho de nhin layout
    if r > 230 and g > 230 and b > 230: return '.'   # trang
    if r < 40 and g < 40 and b < 40:    return '#'   # den
    if b > r + 30 and b > g:            return 'B'   # xanh duong
    if r > b + 40 and r > g + 40:       return 'R'   # do
    if g > r + 20 and g > b + 20:       return 'G'
    l = (r + g + b) // 3
    if l > 180: return '-'
    if l > 110: return '='
    if l > 60:  return '+'
    return '#'

def ascii_map(im, cols=60, rows=40):
    w, h = im.size
    px = im.load()
    for j in range(rows):
        line = []
        for i in range(cols):
            x = int(i * w / cols); y = int(j * h / rows)
            line.append(cls(*px[min(x, w - 1), min(y, h - 1)]))
        print("%3d %s" % (int(j * h / rows), "".join(line)))
    print("     " + "".join(str(int(i * w / cols) // 100 % 10) for i in range(cols)))
    print("     " + "".join(str(int(i * w / cols) // 10 % 10) for i in range(cols)))

def bands(im):
    """Gom cac dong lien tiep co cung 'dau vet mau' thanh 1 band de doc layout."""
    px = im.load()
    w, h = im.size
    def hx(c): return "#%02X%02X%02X" % c
    prev = None
    start = 0
    out = []
    for y in range(h):
        samples = [hx(px[x, y]) for x in (2, 12, 60, 120, 180, 228, 237)]
        if samples != prev:
            if prev is not None:
                out.append((start, y - 1, prev))
            prev = samples
            start = y
    out.append((start, h - 1, prev))
    merged = []
    for a, b, s in out:
        if merged and merged[-1][2] == s:
            merged[-1] = (merged[-1][0], b, s)
        else:
            merged.append((a, b, s))
    for a, b, s in merged:
        if b - a < 1:
            continue
        print("y %3d-%3d h=%2d  p2=%s p12=%s p60=%s p120=%s p180=%s p228=%s p237=%s"
              % (a, b, b - a + 1, *s))


def zoom(im, x, y, w, h, thresh=140):
    px = im.load()
    print("zoom %dx%d at (%d,%d)" % (w, h, x, y))
    for j in range(h):
        line = []
        for i in range(w):
            r, g, b = px[min(x + i, im.size[0] - 1), min(y + j, im.size[1] - 1)]
            l = (r + g + b) // 3
            if l < 60: c = '#'
            elif l < thresh: c = '+'
            elif l < 210: c = '.'
            else: c = ' '
            line.append(c)
        print("%4d %s" % (y + j, "".join(line)))

def colors(im):
    px = im.load()
    def hx(c): return "#%02X%02X%02X" % c
    pts = [("pane top-left", 2, 2), ("pane top-right", 237, 2),
           ("pane bottom", 120, 20), ("softkey bar", 120, 310),
           ("content", 120, 200), ("left pad", 2, 150), ("right pad", 237, 150)]
    for name, x, y in pts:
        print("  %-16s (%3d,%3d) %s" % (name, x, y, hx(px[x, y])))
    # dem so pixel theo lop mau tren toan khung
    from collections import Counter
    cnt = Counter(hx(px[x, y]) for y in range(0, im.size[1], 2) for x in range(0, im.size[0], 2))
    print("  top colors:", cnt.most_common(8))

if __name__ == "__main__":
    p = sys.argv[1]
    im = load(p)
    mode = sys.argv[2] if len(sys.argv) > 2 else "ascii"
    if mode == "ascii": ascii_map(im)
    elif mode == "bands": bands(im)
    elif mode == "colors": colors(im)
    elif mode == "zoom":
        x, y, w, h = (int(v) for v in sys.argv[3:7])
        zoom(im, x, y, w, h)
