-- tools/lua_smoke.lua — Smoke test E524546 Doodle OS tren PC (chay bang lupa/LuaJIT).
-- Stub engine.* + di toan bo 15 app nhu PROMPT.md muc 19: dem draw call,
-- khong duoc man hinh den (clear/rect/text/flush > 0), khong duoc loi runtime.
TEST = { clear = 0, rect = 0, text = 0, flush = 0, line = 0 }
FAKEFS = {}
TICK = 0

-- Lua 5.1.5 dich (ESP32) co math.atan2; Lua test tren PC (5.4+) da bo
-- nen shim de smoke test chay duoc ma khong doi code dich.
if not math.atan2 then
  function math.atan2(y, x) return math.atan(y, x) end
end

engine = {}
engine.W, engine.H = 240, 320
engine.version = "smoke-1.0"
engine.has_audio = false
engine.has_files = true
engine.has_images = true
engine.has_touch = false
engine.has_rename = false
engine.has_removable = true
engine.has_log = true
engine.runtime_compatible = true

function engine.color(r, g, b) return (r % 32) * 1024 + (g % 64) * 32 + (b % 32) end
function engine.clear(c) TEST.clear = TEST.clear + 1 end
function engine.rect(x, y, w, h, c) TEST.rect = TEST.rect + 1 end
function engine.frame(x, y, w, h, c) TEST.rect = TEST.rect + 1 end
function engine.line(x1, y1, x2, y2, c) TEST.line = TEST.line + 1 end
function engine.text(x, y, s, c) TEST.text = TEST.text + 1 end
function engine.set_font(n) end
function engine.text_width(s) return #tostring(s) * 6 end
function engine.font_height() return 8 end
function engine.image(x, y, p) return false end
function engine.image_region(p, a, b, c, d, e, f) return false end
function engine.flush() TEST.flush = TEST.flush + 1 end
function engine.tick_ms() return TICK end
function engine.log(s) end
function engine.exit() end
function engine.file_exists(n) return FAKEFS[n] ~= nil end
function engine.file_write(n, d) FAKEFS[n] = d return true end
function engine.file_read(n) return FAKEFS[n] end
function engine.file_delete(n) FAKEFS[n] = nil return true end
function engine.audio_play(p) return false end
function engine.audio_stop() end
function engine.audio_set_volume(v) end
function engine.audio_is_playing() return false end
function engine.capabilities() return 0 end
function engine.device_info()
  return { family = "smoke", width = 240, height = 320, preferred_fps = 30,
           recommended_ram_kb = 8192, capabilities = 0, compatibility = "full" }
end
function engine.runtime_compat() return { compatible = true } end
function engine.sd_ok() return false end
mre = engine

-- require("src.x") -> data/src/x.lua (giong firmware ESP32)
local searchers = package.searchers or package.loaders
table.insert(searchers, 2, function(mod)
  local p = "data/" .. mod:gsub("%.", "/") .. ".lua"
  local f = io.open(p, "r")
  if f then f:close() return assert(loadfile(p)) end
  return nil
end)

dofile("data/conf.lua")
dofile("data/main.lua")

assert(type(engine.load) == "function", "thieu engine.load")
assert(type(engine.update) == "function", "thieu engine.update")
assert(type(engine.draw) == "function", "thieu engine.draw")
assert(type(engine.keypressed) == "function", "thieu engine.keypressed")
assert(type(engine.keyreleased) == "function", "thieu engine.keyreleased")

engine.load()

local function frames(n, dtms)
  for _ = 1, n do
    TICK = TICK + dtms
    engine.update(dtms / 1000)
    engine.draw()
  end
end

local function press(k)
  engine.keypressed(k)
  frames(2, 33)
  engine.keyreleased(k)
  frames(2, 33)
end

-- qua man hinh boot (1600ms)
frames(60, 33)

local sel = 1
local NAPP = 15
for app = 1, NAPP do
  local downs = (app - sel) % NAPP
  for _ = 1, downs do press("down") end
  sel = app
  press("ok") -- mo app (notes->paint tu mo sau SELECT tiep)
  if app == 5 then press("ok") end -- notes: mo trang 1 -> paint
  frames(15, 33)
  -- bam toan bo phim nhu nguoi dung that (mapping Symbian moi);
  -- back/menu de cuoi de khong roi app giua chung.
  for _, k in ipairs({ "up", "down", "left", "right", "ok", "delete", "option", "mode", "1", "5", "0" }) do
    press(k)
  end
  frames(10, 33)
  -- ve menu (app sau nhat can 3 back: vd wifi ctx -> list -> main -> menu)
  press("back"); press("back"); press("back"); press("menu")
  frames(5, 33)
end

-- ve them vai chuc frame o menu
frames(30, 33)

assert(TEST.clear > 0, "khong co clear -> man hinh den")
assert(TEST.text > 0, "khong co text")
assert(TEST.flush > 0, "khong co flush")
assert(TEST.line > 0, "khong co line (mat net doodle)")

print(string.format("SMOKE OK: 15 app + paint | clear=%d rect=%d line=%d text=%d flush=%d",
  TEST.clear, TEST.rect, TEST.line, TEST.text, TEST.flush))
