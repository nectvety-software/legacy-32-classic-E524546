#!/usr/bin/env python3
"""Automated device test: launcher + browser over serial CLI. Captures screenshots."""
import serial, time, sys, os, struct

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM3"
BAUD = 115200
OUT = os.path.join(os.path.dirname(__file__), "device_out")
os.makedirs(OUT, exist_ok=True)

W, H = 240, 320
fails = []
log = []

def rgb565_to_bmp(path, data):
    # data: RGB565 W*H*2 stored big-endian on this sprite dump -> 24-bit BMP bottom-up
    px = []
    for i in range(0, len(data), 2):
        v = (data[i] << 8) | data[i + 1]
        r = ((v >> 11) & 0x1F) << 3
        g = ((v >> 5) & 0x3F) << 2
        b = (v & 0x1F) << 3
        px.append((r, g, b))
    row_pad = (4 - (W * 3) % 4) % 4
    img = bytearray()
    for y in range(H-1, -1, -1):
        for x in range(W):
            r, g, b = px[y*W + x]
            img += bytes((b, g, r))
        img += b"\x00" * row_pad
    off = 54
    size = off + len(img)
    hdr = b"BM" + struct.pack("<IHHI", size, 0, 0, off)
    dib = struct.pack("<IiiHHIIiiII", 40, W, H, 1, 24, 0, len(img), 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(hdr + dib + img)

class Dev:
    def __init__(self, port, retries=10):
        self.s = None
        last = None
        for i in range(retries):
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
        # Do NOT toggle DTR/RTS here — CH340 often glitches mid-read after reset.
        # Boot log is captured as-is; setup usually already finished after flash.

    def _open(self):
        last = None
        for _ in range(8):
            try:
                self.s = serial.Serial(PORT, BAUD, timeout=0.3)
                time.sleep(0.3)
                return True
            except Exception as e:
                last = e
                time.sleep(0.5)
        print(f"  reopen fail: {last}")
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

    def read_until(self, marker, timeout=8.0):
        buf = b""
        t0 = time.time()
        reopens = 0
        while time.time() - t0 < timeout:
            try:
                if self.s is None or not self.s.is_open:
                    if reopens >= 3:
                        break
                    reopens += 1
                    if not self._open():
                        time.sleep(0.5)
                        continue
                chunk = self.s.read(4096)
            except Exception as e:
                if reopens >= 3:
                    print(f"  read_until give up: {e}")
                    break
                reopens += 1
                try:
                    if self.s:
                        self.s.close()
                except Exception:
                    pass
                self.s = None
                time.sleep(0.4)
                continue
            if chunk:
                buf += chunk
                log.append(chunk)
                if marker in buf:
                    return buf
        return buf

    def shot(self, name, timeout=20.0):
        try:
            self.send("shot")
        except Exception as e:
            print(f"  shot send fail: {e}")
            return False
        buf = b""
        t0 = time.time()
        reopens = 0
        while time.time() - t0 < timeout:
            try:
                if self.s is None or not self.s.is_open:
                    if reopens >= 3:
                        break
                    reopens += 1
                    if not self._open():
                        time.sleep(0.5)
                        continue
                chunk = self.s.read(4096)
            except Exception as e:
                if reopens >= 3:
                    print(f"  shot read give up: {e}")
                    break
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
                if b"SHOT2 " in buf and b"\nEND" in buf:
                    break
                if b"[shot]" in buf:
                    print(f"  shot fail: {buf[-80:]}")
                    return False
        i = buf.find(b"SHOT2 ")
        if i < 0:
            print(f"  shot: no SHOT2 header in {len(buf)} bytes")
            return False
        j = buf.find(b"\n", i)
        n = int(buf[i+6:j])
        raw = buf[j+1:j+1+n]
        if len(raw) < n:
            t0 = time.time()
            while len(raw) < n and time.time()-t0 < 15:
                try:
                    if self.s is None or not self.s.is_open:
                        if not self._open():
                            time.sleep(0.3)
                            continue
                    chunk = self.s.read(n - len(raw))
                except Exception:
                    time.sleep(0.2)
                    continue
                if chunk:
                    raw += chunk
        path = os.path.join(OUT, name + ".bmp")
        if len(raw) >= n:
            rgb565_to_bmp(path, raw[:n])
            print(f"  shot -> {path} ({n} px bytes)")
            return True
        print(f"  shot incomplete: {len(raw)}/{n}")
        return False

    def key(self, k, wait=0.25):
        self.send(f"key {k}")
        time.sleep(wait)

def check(cond, msg):
    tag = "PASS" if cond else "FAIL"
    print(f"[{tag}] {msg}")
    if not cond:
        fails.append(msg)

def main():
    print(f"== open {PORT} ==")
    # Wipe old shots so we only analyze fresh captures
    for fn in os.listdir(OUT):
        if fn.endswith(('.bmp', '.png')):
            try:
                os.remove(os.path.join(OUT, fn))
            except OSError:
                pass
    d = Dev(PORT)
    # Hot-open: device may already be past boot banner. Probe with status first.
    boot = d.read_until(b"CLI:", timeout=3.0)
    if b"CLI:" not in boot and b"setup done" not in boot and b"[boot]" not in boot:
        d.send("status")
        probe = d.read_until(b"[status]", timeout=5.0)
        boot += probe
        if b"[status]" not in probe:
            d.send("")
            boot += d.read_until(b"CLI:", timeout=8.0)
    text = boot.decode("utf-8", "replace")
    print(text[-2000:])
    # Soft checks: hot-open without boot banner is OK if CLI/status works
    cli_ok = (b"CLI:" in boot or b"setup done" in boot or b"[boot]" in boot
              or b"[status]" in boot or b"[nav]" in boot or b"[doc]" in boot)
    check(cli_ok, "boot: CLI reachable")
    if b"[gfx] frame buffer" in boot:
        check(b"frame buffer" in boot and b"OK" in boot, "frame buffer OK")
    if b"[lc] theme" in boot or b"theme vqeaf" in boot:
        check(True, "boot: theme loaded")
    check(b"CRITICAL" not in boot, "no CRITICAL mem errors")
    # screenshot launcher
    print("== launcher screenshots ==")
    d.shot("00_boot")
    # navigate tabs
    d.key("right"); time.sleep(0.2); d.shot("01_tab_right")
    d.key("right"); time.sleep(0.2); d.shot("02_tab_right2")
    d.key("right"); time.sleep(0.2); d.shot("03_settings")
    # cycle theme: up to Giao dien row then OK? depends layout — down/up list
    for i in range(6):
        d.key("down", 0.12)
    d.shot("04_settings_scrolled")
    d.key("ok", 0.4)
    d.shot("05_after_ok")
    # back to apps tab, open browser
    d.key("left", 0.2); d.key("left", 0.2); d.key("left", 0.2)
    d.shot("06_apps")
    # first item is Trình duyệt -> OK opens browser
    d.key("ok", 0.5)
    d.shot("07_browser_open")
    d.send("status")
    st = d.read_until(b"[status]", timeout=5)
    print(st[-400:].decode("utf-8", "replace"))
    # load a page
    print("== browser load ==")
    d.send("go mtt:start")
    time.sleep(1.5)
    d.shot("08_mtt_start")
    d.send("doc")
    doc = d.read_until(b"[doc]", timeout=5)
    dtext = doc.decode("utf-8", "replace")
    print(dtext[:600])
    check(b"[doc]" in doc, "doc: parsed start page")
    # try real https — wait for [nav] (always printed) then optional [http]/[doc]
    d.send("go https://qeafivels.com/")
    got = d.read_until(b"[nav]", timeout=8.0)
    # After nav, page load ends with [doc] (success) or show_msg path (no [http])
    more = d.read_until(b"[doc]", timeout=20.0)
    got += more
    gtxt = got.decode("utf-8", "replace")
    print(gtxt[-800:])
    time.sleep(1.0)
    d.shot("09_qeafivels")
    d.send("doc")
    doc2 = d.read_until(b"[doc]", timeout=6)
    print(doc2.decode("utf-8", "replace")[:600])
    check(b"[doc]" in doc2, "doc: page after https attempt")
    # If WiFi-off error page: go home before scroll/menu tests so keys are meaningful
    if b"WiFi is off" in doc2 or b"Connection Timeout" in doc2:
        print("  (wifi off / error page -> back to mtt:start for scroll+menu)")
        d.send("go mtt:start")
        d.read_until(b"[nav]", timeout=5)
        time.sleep(0.5)
    # scroll
    for _ in range(5):
        d.key("down", 0.15)
    d.shot("10_scrolled")
    # menu open
    d.key("option", 0.4)
    d.shot("11_menu")
    d.key("back", 0.3)
    # back to launcher?
    d.send("status")
    d.read_until(b"[status]", timeout=4)

    with open(os.path.join(OUT, "serial_log.txt"), "wb") as f:
        f.write(b"".join(log))
    print(f"\n== RESULT: {len(fails)} FAIL ==")
    for m in fails:
        print(" -", m)
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
