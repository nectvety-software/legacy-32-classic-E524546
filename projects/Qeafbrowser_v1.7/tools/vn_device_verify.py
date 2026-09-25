#!/usr/bin/env python3
"""Device verify: WiFi -> BBC RSS regression -> Vietnamese UTF-8 page -> shots."""
import serial, time, sys, os, struct, re

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM3"
BAUD = 115200
OUT = os.path.join(os.path.dirname(__file__), "device_out")
os.makedirs(OUT, exist_ok=True)
W, H = 240, 320
SSID = os.environ.get("QB_SSID", "VNPT-Home")
PASS = os.environ.get("QB_PASS", "abc")
fails, log = [], []

def rgb565_to_bmp(path, data):
    px = []
    for i in range(0, len(data), 2):
        v = (data[i] << 8) | data[i + 1]
        r = ((v >> 11) & 0x1F) << 3
        g = ((v >> 5) & 0x3F) << 2
        b = (v & 0x1F) << 3
        px.append((r, g, b))
    row_pad = (4 - (W * 3) % 4) % 4
    img = bytearray()
    for y in range(H - 1, -1, -1):
        for x in range(W):
            r, g, b = px[y * W + x]
            img += bytes((b, g, r))
        img += b"\x00" * row_pad
    off = 54
    size = off + len(img)
    hdr = b"BM" + struct.pack("<IHHI", size, 0, 0, off)
    dib = struct.pack("<IiiHHIIiiII", 40, W, H, 1, 24, 0, len(img), 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(hdr + dib + img)

class Dev:
    def __init__(self, port, retries=12):
        self.s = None
        last = None
        for _ in range(retries):
            try:
                self.s = serial.Serial(port, BAUD, timeout=0.3)
                break
            except Exception as e:
                last = e
                time.sleep(0.8)
        if self.s is None:
            raise last
        time.sleep(0.8)
        try:
            self.s.reset_input_buffer()
        except Exception:
            pass

    def _open(self):
        for _ in range(8):
            try:
                self.s = serial.Serial(PORT, BAUD, timeout=0.3)
                time.sleep(0.3)
                return True
            except Exception:
                time.sleep(0.5)
        return False

    def send(self, line):
        payload = (line + "\n").encode()
        for attempt in range(3):
            try:
                if self.s is None or not self.s.is_open:
                    if not self._open():
                        raise RuntimeError("port closed")
                self.s.write(payload)
                self.s.flush()
                return
            except Exception as e:
                print(f"  send retry {attempt+1}: {e}")
                try:
                    if self.s:
                        self.s.close()
                except Exception:
                    pass
                self.s = None
                time.sleep(0.5)
        raise RuntimeError(f"send failed: {line!r}")

    def read_until(self, marker, timeout=10.0):
        buf = b""
        t0 = time.time()
        reopens = 0
        while time.time() - t0 < timeout:
            try:
                if self.s is None or not self.s.is_open:
                    reopens += 1
                    if reopens > 4:
                        break
                    if not self._open():
                        time.sleep(0.4)
                        continue
                chunk = self.s.read(4096)
            except Exception:
                reopens += 1
                try:
                    if self.s:
                        self.s.close()
                except Exception:
                    pass
                self.s = None
                time.sleep(0.3)
                continue
            if chunk:
                buf += chunk
                log.append(chunk)
                if marker in buf:
                    return buf
        return buf

    def shot(self, name, timeout=25.0):
        self.send("shot")
        buf = b""
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                if self.s is None or not self.s.is_open:
                    if not self._open():
                        time.sleep(0.3)
                        continue
                chunk = self.s.read(8192)
            except Exception:
                time.sleep(0.2)
                continue
            if chunk:
                buf += chunk
                if b"SHOT2 " in buf and b"\nEND" in buf:
                    break
                if b"[shot]" in buf and b"SHOT2" not in buf:
                    print("  shot fail:", buf[-80:])
                    return False
        i = buf.find(b"SHOT2 ")
        if i < 0:
            print("  no SHOT2", len(buf))
            return False
        j = buf.find(b"\n", i)
        n = int(buf[i + 6 : j])
        raw = buf[j + 1 : j + 1 + n]
        t0 = time.time()
        while len(raw) < n and time.time() - t0 < 20:
            try:
                chunk = self.s.read(n - len(raw))
                if chunk:
                    raw += chunk
            except Exception:
                time.sleep(0.2)
        if len(raw) < n:
            print(f"  incomplete {len(raw)}/{n}")
            return False
        path = os.path.join(OUT, name + ".bmp")
        rgb565_to_bmp(path, raw[:n])
        print("  shot ->", path)
        return True

def check(cond, msg):
    tag = "PASS" if cond else "FAIL"
    print(f"[{tag}] {msg}")
    if not cond:
        fails.append(msg)

def main():
    # free COM3: kill any python holding it is caller's job
    d = Dev(PORT)
    boot = d.read_until(b"setup done", timeout=12)
    if b"setup done" not in boot:
        d.send("status")
        boot += d.read_until(b"[status]", timeout=5)
    print(boot.decode("utf-8", "replace")[-600:])
    check(b"setup done" in boot or b"[status]" in boot, "boot: CLI reachable")
    check(b"CRITICAL" not in boot, "no CRITICAL mem errors")

    # ensure browser mode (launcher may hold screen)
    d.send("key back")
    time.sleep(0.3)

    # WiFi
    d.send("status")
    st = d.read_until(b"[status]", timeout=5).decode("utf-8", "replace")
    print(st.strip())
    if "wifi=up" not in st:
        print(f"== connect WiFi {SSID} ==")
        d.send(f"wifi {SSID} {PASS}")
        # wifi OK prints [cli] wifi OK then go_url home
        buf = d.read_until(b"[cli] wifi ", timeout=15)
        # also wait for either OK or FAIL
        if b"[cli] wifi OK" not in buf:
            buf += d.read_until(b"[cli] wifi FAIL", timeout=12)
        print(buf.decode("utf-8", "replace")[-400:])
        check(b"[cli] wifi OK" in buf, f"wifi connect {SSID}")
        time.sleep(1.0)
        d.send("status")
        st = d.read_until(b"[status]", timeout=5).decode("utf-8", "replace")
        print(st.strip())
        check("wifi=up" in st, "wifi status up")
        # wifi cmd auto-goes home; wait doc
        d.read_until(b"[doc]", timeout=8)

    # exit launcher if needed -> browser
    d.send("go mtt:start")
    time.sleep(0.8)
    d.read_until(b"[doc]", timeout=6)

    # 1) BBC RSS regression
    print("== BBC RSS ==")
    d.send("go https://feeds.bbci.co.uk/news/rss.xml")
    d.read_until(b"[nav]", timeout=6)
    more = d.read_until(b"[doc]", timeout=30)
    d.send("doc")
    doc = d.read_until(b"[doc]", timeout=8)
    dt = doc.decode("utf-8", "replace")
    print(dt[:700])
    check(b"[doc]" in doc, "doc: BBC RSS loaded")
    check(b"WiFi is off" not in doc, "BBC: not WiFi-off page")
    m = re.search(r'title="([^"]*)"', dt)
    if m:
        title = m.group(1)
        print("TITLE=", repr(title))
        check(title.count("BBC News") <= 1, f"BBC title not doubled: {title[:80]}")
        check(len(title) > 5, f"BBC title non-empty: {title[:80]}")
    else:
        fails.append("bbc-no-title")
    m = re.search(r"links=(\d+)", dt)
    if m:
        nl = int(m.group(1))
        print("links=", nl)
        check(nl >= 20, f"BBC links={nl} (>=20)")
    else:
        fails.append("bbc-no-links")
    d.shot("14_rss_bbc")

    # 2) Vietnamese page (UTF-8 diacritics + optional numeric entities)
    print("== Vietnamese page ==")
    d.send("go https://vi.wikipedia.org/wiki/Ti%E1%BA%BFng_Vi%E1%BB%87t")
    d.read_until(b"[nav]", timeout=6)
    more = d.read_until(b"[doc]", timeout=35)
    d.send("doc")
    doc = d.read_until(b"[doc]", timeout=8)
    dt = doc.decode("utf-8", "replace")
    print(dt[:800])
    check(b"[doc]" in doc, "doc: VN wiki loaded")
    check(b"WiFi is off" not in doc, "VN: not WiFi-off page")
    has_high = any(b >= 0xC3 for b in doc)
    print("utf8_high=", has_high)
    check(has_high, "VN: dump has UTF-8 high bytes")
    seqs = [
        b"\xe1\xba\xbf",  # ế
        b"\xe1\xbb\x87",  # ệ
        b"\xc3\xa1",      # á
        b"\xc6\xa1",      # ơ
        b"\xe1\xbb\xad",  # ữ
        b"\xe1\xba\xb3",  # ạ
        b"\xe1\xba\xb9",  # ế-ish
    ]
    hits = sum(1 for s in seqs if s in doc)
    print("vn_seq_hits=", hits)
    check(hits >= 1, f"VN diacritics in doc dump (hits={hits})")
    m = re.search(r'title="([^"]*)"', dt)
    if m:
        t = m.group(1)
        print("VN TITLE=", repr(t))
        check(any(ord(c) > 127 for c in t), f"VN title has diacritics: {t[:80]}")
    d.shot("16_vietnamese_wiki")

    with open(os.path.join(OUT, "vn_device_run.txt"), "wb") as f:
        f.write(b"".join(log))
    print(f"\n== RESULT: {len(fails)} FAIL ==")
    for msg in fails:
        print(" -", msg)
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
