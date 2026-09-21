-- src/os.lua — Registry 15 app kieu PochitaOS + launcher + cai dat.
-- Thu tu app giong pochita/src/app_registry.h: WiFi..VM.
local E = engine
local D = require("src.doodle")
local C = D.C

local S = {}

S.apps = {
  { id = "wifi",      name = "WiFi" },
  { id = "files",     name = "Files" },
  { id = "bluetooth", name = "Bluetooth" },
  { id = "terminal",  name = "Terminal" },
  { id = "notes",     name = "Notes" },
  { id = "lora",      name = "LoRa" },
  { id = "ir",        name = "IR" },
  { id = "browser",   name = "Browser" },
  { id = "settings",  name = "Settings" },
  { id = "radio",     name = "Radio" },
  { id = "music",     name = "Music" },
  { id = "paint",     name = "Paint" },
  { id = "retro",     name = "Retro" },
  { id = "chat",      name = "Chat" },
  { id = "vm",        name = "VM" },
}

S.sel = 1
S.scroll = 0
S.vis = 4
S.top = 78
S.rowh = 54

S.settings = { fps = 30, tool = 1, bt = false }
S.boot_count = 0

function S.load_all()
  if E.has_files then
    local s = E.file_read("settings.dat")
    if s then
      local f, t, b = s:match("(%d+)%s+(%d+)%s+(%d+)")
      if f then S.settings.fps = tonumber(f) end
      if t then S.settings.tool = tonumber(t) end
      if b then S.settings.bt = (b == "1") end
    end
    local d = E.file_read("dem.dat")
    if d then S.boot_count = tonumber(d) or 0 end
    S.boot_count = S.boot_count + 1
    E.file_write("dem.dat", tostring(S.boot_count))
  end
end

function S.save_settings()
  if E.has_files then
    E.file_write("settings.dat",
      S.settings.fps .. " " .. S.settings.tool .. " " .. (S.settings.bt and "1" or "0"))
  end
end

function S.move(d)
  S.sel = S.sel + d
  if S.sel < 1 then S.sel = #S.apps end
  if S.sel > #S.apps then S.sel = 1 end
  if S.sel - S.scroll > S.vis then S.scroll = S.sel - S.vis end
  if S.sel <= S.scroll then S.scroll = S.sel - 1 end
end

-- Cat chuoi dai thanh nhieu dong (moi dong <= n ky tu), de preview/dialog.
function S.wrap(s, n)
  local out = {}
  s = tostring(s):gsub("\n", " ")
  while #s > n do
    out[#out + 1] = s:sub(1, n)
    s = s:sub(n + 1)
  end
  out[#out + 1] = s
  return out
end

function S.draw_launcher()
  D.paper()
  D.title(40, "Menu chinh")
  E.text(180, 42, S.sel .. "/" .. #S.apps, C.pencil)
  D.star(215, 60, 6, C.pen2, 5)
  for r = 1, S.vis do
    local i = S.scroll + r
    if i > #S.apps then break end
    local y = S.top + (r - 1) * S.rowh
    local a = S.apps[i]
    D.icon(a.id, 40, y)
    E.set_font(12)
    E.text(70, y + 4, a.name, (i == S.sel) and C.pen or C.pencil)
    E.set_font(8)
    if i == S.sel then
      D.arrow(30, y + 24, 62, y + 24, C.pen2, i)
      D.wline(70, y + 26, 70 + E.text_width(a.name) + 8, y + 25, C.pen2, i * 3)
    end
  end
  -- thanh cuon ve tay ben phai
  D.wline(230, 78, 230, 282, C.faint, 91)
  local kh = math.floor(204 * S.vis / #S.apps)
  local ky = 78 + math.floor(204 * S.scroll / #S.apps)
  E.rect(227, ky, 6, kh, C.pencil)
  D.footer("len/xuong chon - START mo")
end

return S
