-- src/apps.lua — 15 app kieu PochitaOS, ve phong cach vo hoc sinh.
-- Quy uoc: A.enter(id) / A.update(id,dt,keys) / A.draw(id) / A.key(id,k)
-- A.key tra ve id man hinh tiep theo ("menu", "paint") hoac nil (o lai).
local E = engine
local D = require("src.doodle")
local OS = require("src.os")
local C = D.C

local A = {}

local function is_back(k) return k == "back" or k == "menu" end

-- ================= PAINT (ve tu do, Notes goi toi) =================
A.paint = { trail = {}, cx = 130, cy = 170, tool = 1, slot = 1, msg = "" }
local PENTOOL = { "but bi", "but chi", "tay" }

local function tool_color(i)
  if i == 1 then return C.pen
  elseif i == 2 then return C.pencil
  else return C.paper end
end

function A.paint_load(slot)
  local t = {}
  if E.has_files then
    local s = E.file_read("trang" .. slot .. ".dat")
    if s then
      for line in s:gmatch("[^\n]+") do
        local x1, y1, x2, y2, c = line:match("(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)")
        if x1 and #t < 240 * 5 then
          t[#t + 1] = tonumber(x1); t[#t + 1] = tonumber(y1)
          t[#t + 1] = tonumber(x2); t[#t + 1] = tonumber(y2)
          t[#t + 1] = tonumber(c)
        end
      end
    end
  end
  return t
end

function A.paint_open(slot)
  local P = A.paint
  P.slot = slot or 1
  P.trail = A.paint_load(P.slot)
  P.cx, P.cy = 130, 170
  P.tool = OS.settings.tool or 1
  P.msg = ""
end

function A.paint_save(slot)
  if not E.has_files then return false end
  local parts = {}
  local t = A.paint.trail
  for i = 1, #t, 5 do
    parts[#parts + 1] = t[i] .. " " .. t[i + 1] .. " " .. t[i + 2] .. " " .. t[i + 3] .. " " .. t[i + 4]
  end
  return E.file_write("trang" .. slot .. ".dat", table.concat(parts, "\n"))
end

function A.paint_update(dt, keys)
  local P = A.paint
  local dx, dy = 0, 0
  if keys.left then dx = dx - 1 end
  if keys.right then dx = dx + 1 end
  if keys.up then dy = dy - 1 end
  if keys.down then dy = dy + 1 end
  if dx ~= 0 or dy ~= 0 then
    local nx = math.max(30, math.min(236, P.cx + dx * 110 * dt))
    local ny = math.max(34, math.min(300, P.cy + dy * 110 * dt))
    local t = P.trail
    t[#t + 1] = math.floor(P.cx); t[#t + 1] = math.floor(P.cy)
    t[#t + 1] = math.floor(nx); t[#t + 1] = math.floor(ny)
    t[#t + 1] = tool_color(P.tool)
    while #t > 240 * 5 do for _ = 1, 5 do table.remove(t, 1) end end
    P.cx, P.cy = nx, ny
  end
end

function A.paint_draw()
  local P = A.paint
  D.paper()
  local t = P.trail
  for i = 1, #t, 5 do E.line(t[i], t[i + 1], t[i + 2], t[i + 3], t[i + 4]) end
  E.line(math.floor(P.cx) - 5, math.floor(P.cy), math.floor(P.cx) + 5, math.floor(P.cy), C.pen2)
  E.line(math.floor(P.cx), math.floor(P.cy) - 5, math.floor(P.cx), math.floor(P.cy) + 5, C.pen2)
  D.skcircle(math.floor(P.cx), math.floor(P.cy), 8, C.pencil, 1)
  E.set_font(12)
  E.text(32, 40, "Paint " .. P.slot, C.pen)
  E.set_font(8)
  E.text(120, 42, PENTOOL[P.tool], C.pen2)
  if P.msg ~= "" then E.text(120, 54, P.msg, C.pen2) end
  D.footer("OPTION but - B lui - START luu - A ve")
end

function A.paint_key(k)
  local P = A.paint
  if k == "back" or k == "menu" then return "menu" end
  P.msg = ""
  if k == "option" then
    P.tool = P.tool + 1
    if P.tool > 3 then P.tool = 1 end
  elseif k == "delete" then
    for _ = 1, 5 do table.remove(P.trail) end
  elseif k == "ok" then
    if A.paint_save(P.slot) then P.msg = "da luu trang " .. P.slot
    else P.msg = "loi luu!" end
  end
  return nil
end

-- ================= WIFI (menu nhu pochita router.h) =================
A.wifi = { st = "main", sel = 1, scroll = 0, nets = {}, saved = {}, prog = 0, ctx = nil, detail = 1 }
local WPOOL = {
  { "Lop Hoc 5A", -52, "WPA2" }, { "Thu Vien", -63, "WPA2" },
  { "Can Tin", -71, "WPA" }, { "Phong May", -58, "WPA2" },
  { "Khach", -80, "Mo" }, { "Nha Ben", -77, "WPA2" },
}
local WMAIN = { "Mang hien co", "Mang da luu", "Trang thai", "Quet mang" }
local WCTX = { "Ket noi", "Luu", "Quen", "Chi tiet", "Huy" }

local function wifi_load_saved()
  A.wifi.saved = {}
  if E.has_files then
    local s = E.file_read("wifi.dat")
    if s then
      for line in s:gmatch("[^\n]+") do
        local ss, sec = line:match("([^|]+)|?(.*)")
        if ss and ss ~= "" then
          A.wifi.saved[#A.wifi.saved + 1] = { ssid = ss, sec = sec or "" }
        end
      end
    end
  end
end

local function wifi_save()
  if not E.has_files then return end
  local p = {}
  for _, n in ipairs(A.wifi.saved) do p[#p + 1] = n.ssid .. "|" .. (n.sec or "") end
  E.file_write("wifi.dat", table.concat(p, "\n"))
end

local function bars_for(rssi)
  if rssi > -60 then return 4
  elseif rssi > -68 then return 3
  elseif rssi > -76 then return 2
  else return 1 end
end

local function wifi_to_main()
  local W = A.wifi
  W.st = "main"; W.sel = 1; W.scroll = 0; W.ctx = nil
end

function A.wifi_enter() wifi_to_main() wifi_load_saved() end

function A.wifi_update(dt)
  local W = A.wifi
  if W.st == "scan" or W.st == "conn" then
    W.prog = W.prog + dt * 0.9
    if W.prog >= 1 then
      W.prog = 0
      if W.st == "scan" then
        W.nets = {}
        for i, p in ipairs(WPOOL) do
          local jit = ((E.tick_ms() + i * 137) % 11) - 5
          W.nets[i] = { ssid = p[1], rssi = p[2] + jit, sec = p[3] }
        end
        W.st = "list"; W.sel = 1; W.scroll = 0
      else
        local n = W.nets[W.sel]
        if n then
          local found = false
          for _, s in ipairs(W.saved) do if s.ssid == n.ssid then found = true end end
          if not found then W.saved[#W.saved + 1] = { ssid = n.ssid, sec = n.sec } end
          wifi_save()
        end
        W.st = "list"
      end
    end
  end
end

local function wifi_draw_rows(list, sel, scroll, fmt)
  for r = 1, 4 do
    local i = scroll + r
    if i > #list then break end
    local y = 78 + (r - 1) * 52
    if i == sel then D.skrect(30, y - 4, 202, 46, C.pen2, 100 + i) end
    fmt(list[i], y, i == sel)
  end
end

function A.wifi_draw()
  local W = A.wifi
  D.paper()
  if W.st == "main" then
    D.title(40, "WiFi")
    for i, m in ipairs(WMAIN) do
      local y = 84 + (i - 1) * 44
      E.set_font(12)
      E.text(44, y, m, (i == W.sel) and C.pen or C.pencil)
      E.set_font(8)
      if i == W.sel then D.arrow(30, y + 20, 58, y + 20, C.pen2, i) end
    end
    E.text(34, 272, "da luu " .. #W.saved .. " mang", C.pencil)
    D.footer("START chon - A ve")
  elseif W.st == "scan" or W.st == "conn" then
    D.title(40, W.st == "scan" and "Dang quet..." or "Dang ket noi...")
    D.progress(40, 140, 160, 16, W.prog, C.pen, 7)
    D.spiral(120, 210, 18, C.faint, 3)
    D.footer("doi chut...")
  elseif W.st == "list" then
    D.title(40, "Mang (" .. #W.nets .. ")")
    wifi_draw_rows(W.nets, W.sel, W.scroll, function(n, y, sel)
      local rssi = n.rssi or -70
      E.text(36, y, n.ssid:sub(1, 16), sel and C.pen or C.pencil)
      E.text(36, y + 14, rssi .. " dBm - " .. (n.sec or ""), C.pencil)
      D.bars(168, y, bars_for(rssi), C.pen, C.faint)
      if n.sec ~= "Mo" then D.lock(206, y, 12, C.pencil, y) end
    end)
    D.footer("START tuy chon - A ve")
  elseif W.st == "detail" then
    local n = W.nets[W.detail] or { ssid = "?", rssi = 0, sec = "?" }
    D.title(40, "Chi tiet")
    local y = 84
    local function row(k, v)
      E.text(34, y, k, C.pencil)
      E.text(120, y, v, C.pen)
      y = y + 20
    end
    row("ten:", n.ssid:sub(1, 14))
    row("tin hieu:", n.rssi .. " dBm")
    row("bao mat:", n.sec)
    row("kenh:", tostring(1 + (W.detail % 11)))
    row("bssid:", string.format("%02X:AB:%02X:11:22:%02X", 10 + W.detail * 7, 30 + W.detail * 3, 90 + W.detail))
    D.footer("A ve")
  elseif W.st == "saved" then
    D.title(40, "Mang da luu")
    if #W.saved == 0 then
      E.text(34, 100, "chua co mang nao.", C.pencil)
      E.text(34, 118, "quet roi Ket noi de luu.", C.pencil)
    else
      wifi_draw_rows(W.saved, W.sel, W.scroll, function(n, y, sel)
        E.text(36, y, n.ssid:sub(1, 16), sel and C.pen or C.pencil)
        E.text(36, y + 14, "da luu - " .. (n.sec or ""), C.pencil)
      end)
    end
    D.footer("START ket noi - OPTION quen - A ve")
  elseif W.st == "status" then
    D.title(40, "Trang thai")
    local y = 84
    local function row(k, v)
      E.text(34, y, k, C.pencil)
      E.text(120, y, v, C.pen)
      y = y + 20
    end
    row("ket noi:", "khong (mo phong)")
    row("che do:", "tram")
    row("dia chi:", "-")
    row("da luu:", #W.saved .. " mang")
    D.footer("A ve")
  end
  if W.ctx then
    local lines = {}
    for i, m in ipairs(WCTX) do
      lines[i] = ((i == W.ctx) and "> " or "  ") .. m
    end
    D.dialog("Tuy chon", lines)
  end
end

local function wifi_move(d, n)
  local W = A.wifi
  W.sel = W.sel + d
  if W.sel < 1 then W.sel = n end
  if W.sel > n then W.sel = 1 end
  if W.sel - W.scroll > 4 then W.scroll = W.sel - 4 end
  if W.sel <= W.scroll then W.scroll = W.sel - 1 end
end

function A.wifi_key(k)
  local W = A.wifi
  if W.ctx then
    if k == "up" then W.ctx = W.ctx - 1 if W.ctx < 1 then W.ctx = 5 end
    elseif k == "down" then W.ctx = W.ctx + 1 if W.ctx > 5 then W.ctx = 1 end
    elseif k == "ok" then
      local c = W.ctx
      W.ctx = nil
      if c == 1 then W.st = "conn"; W.prog = 0
      elseif c == 2 then
        local n = W.nets[W.sel]
        if n then W.saved[#W.saved + 1] = { ssid = n.ssid, sec = n.sec } wifi_save() end
      elseif c == 3 then
        local n = W.nets[W.sel]
        if n then
          for i, s in ipairs(W.saved) do
            if s.ssid == n.ssid then table.remove(W.saved, i) break end
          end
          wifi_save()
        end
      elseif c == 4 then W.detail = W.sel; W.st = "detail" end
    elseif is_back(k) then W.ctx = nil end
    return nil
  end
  if is_back(k) then
    if W.st ~= "main" then wifi_to_main() else return "menu" end
    return nil
  end
  if W.st == "main" then
    if k == "up" then W.sel = W.sel - 1 if W.sel < 1 then W.sel = 4 end
    elseif k == "down" then W.sel = W.sel + 1 if W.sel > 4 then W.sel = 1 end
    elseif k == "ok" then
      if W.sel == 1 then W.st = "scan"; W.prog = 0
      elseif W.sel == 2 then W.st = "saved"; W.sel = 1; W.scroll = 0
      elseif W.sel == 3 then W.st = "status"
      else W.st = "scan"; W.prog = 0 end
    end
  elseif W.st == "list" then
    if k == "up" then wifi_move(-1, #W.nets)
    elseif k == "down" then wifi_move(1, #W.nets)
    elseif k == "ok" then W.ctx = 1 end
  elseif W.st == "saved" then
    if #W.saved > 0 then
      if k == "up" then wifi_move(-1, #W.saved)
      elseif k == "down" then wifi_move(1, #W.saved)
      elseif k == "ok" then W.st = "conn"; W.prog = 0; W.nets = W.saved
      elseif k == "option" then
        table.remove(W.saved, W.sel); wifi_save()
        if W.sel > #W.saved and W.sel > 1 then W.sel = #W.saved end
        W.scroll = 0
      end
    end
  end
  return nil
end

-- ================= FILES =================
A.files = {
  sel = 1, scroll = 0, prev = nil,
  list = { "main.lua", "conf.lua", "settings.dat", "wifi.dat", "dem.dat",
           "trang1.dat", "trang2.dat", "trang3.dat" },
}

function A.files_draw()
  D.paper()
  D.title(40, "Files")
  if A.files.prev then
    for i, s in ipairs(A.files.prev) do
      if i > 11 then break end
      E.text(32, 70 + (i - 1) * 18, s:sub(1, 30), C.pencil)
    end
    D.footer("START/A ve")
    return
  end
  local F = A.files
  for r = 1, 5 do
    local i = F.scroll + r
    if i > #F.list then break end
    local y = 78 + (r - 1) * 42
    local has = E.has_files and E.file_exists(F.list[i])
    D.icon("files", 36, y)
    E.set_font(12)
    E.text(64, y + 2, F.list[i]:sub(1, 12), (i == F.sel) and C.pen or C.pencil)
    E.set_font(8)
    E.text(64, y + 24, has and "co" or "trong", has and C.pen or C.faint)
    if i == F.sel then D.arrow(28, y + 20, 32, y + 20, C.pen2, i) end
  end
  D.footer("START xem - OPTION xoa - A ve")
end

function A.files_key(k)
  local F = A.files
  if F.prev then
    if k == "ok" or is_back(k) then F.prev = nil end
    return nil
  end
  if is_back(k) then return "menu" end
  if k == "up" then F.sel = F.sel - 1 if F.sel < 1 then F.sel = #F.list end
  elseif k == "down" then F.sel = F.sel + 1 if F.sel > #F.list then F.sel = 1 end
  end
  if F.sel - F.scroll > 5 then F.scroll = F.sel - 5 end
  if F.sel <= F.scroll then F.scroll = F.sel - 1 end
  if k == "ok" then
    local s = E.has_files and E.file_read(F.list[F.sel]) or nil
    if s then F.prev = OS.wrap(s:sub(1, 220), 28)
    else F.prev = { "tep trong." } end
  elseif k == "option" then
    if E.has_files and E.file_delete(F.list[F.sel]) then
      F.prev = { "da xoa " .. F.list[F.sel] }
    else F.prev = { "khong xoa duoc." } end
  end
  return nil
end

-- ================= BLUETOOTH =================
A.bt = { sel = 1, msg = nil, devs = { "Tai nghe A", "Loa B", "Ban phim" } }

function A.bt_draw()
  D.paper()
  D.title(40, "Bluetooth")
  E.text(36, 84, "Bluetooth", C.pen)
  D.toggle(170, 82, OS.settings.bt)
  if OS.settings.bt then
    E.text(36, 110, "thiet bi gan day:", C.pencil)
    for i, d in ipairs(A.bt.devs) do
      local y = 130 + (i - 1) * 40
      D.skcircle(44, y + 8, 7, (i == A.bt.sel) and C.pen or C.pencil, i)
      E.text(60, y + 2, d, (i == A.bt.sel) and C.pen or C.pencil)
    end
  else
    E.text(36, 110, "dang tat.", C.pencil)
  end
  D.footer("START bat/tat hoac ket noi - A ve")
  if A.bt.msg then D.dialog("Bluetooth", { A.bt.msg }) end
end

function A.bt_key(k)
  if A.bt.msg then A.bt.msg = nil return nil end
  if is_back(k) then return "menu" end
  if k == "up" then A.bt.sel = A.bt.sel - 1 if A.bt.sel < 1 then A.bt.sel = 3 end
  elseif k == "down" then A.bt.sel = A.bt.sel + 1 if A.bt.sel > 3 then A.bt.sel = 1 end
  elseif k == "ok" then
    if not OS.settings.bt then
      OS.settings.bt = true
      OS.save_settings()
    else
      A.bt.msg = "da ket noi " .. A.bt.devs[A.bt.sel] .. "!"
    end
  elseif k == "option" then
    OS.settings.bt = false
    OS.save_settings()
  end
  return nil
end

-- ================= TERMINAL =================
A.term = { log = { "Terminal san sang.", "go 'help' de xem lenh." }, cmds = { "help", "info", "dem", "xoa" }, ci = 1 }

local function term_print(s)
  local T = A.term
  T.log[#T.log + 1] = s
  while #T.log > 9 do table.remove(T.log, 1) end
end

local function term_run(cmd)
  term_print("> " .. cmd)
  if cmd == "help" then term_print("lenh: help info dem xoa")
  elseif cmd == "info" then
    local d = E.device_info()
    term_print(d.family .. " " .. d.width .. "x" .. d.height)
  elseif cmd == "dem" then term_print("so lan mo: " .. OS.boot_count)
  elseif cmd == "xoa" then A.term.log = {}
  else term_print("lenh la: " .. cmd) end
end

function A.term_draw()
  D.paper()
  D.title(40, "Terminal")
  local T = A.term
  local n = #T.log
  local first = math.max(1, n - 8)
  for i = first, n do
    E.text(32, 74 + (i - first) * 18, T.log[i]:sub(1, 30), C.pencil)
  end
  E.text(32, 250, "> " .. T.cmds[T.ci], C.pen)
  D.wline(32, 266, 32 + E.text_width("> " .. T.cmds[T.ci]) + 6, 265, C.pen2, 3)
  D.footer("len/xuong chon lenh - START chay")
end

function A.term_key(k)
  local T = A.term
  if is_back(k) then return "menu" end
  if k == "up" then T.ci = T.ci - 1 if T.ci < 1 then T.ci = #T.cmds end
  elseif k == "down" then T.ci = T.ci + 1 if T.ci > #T.cmds then T.ci = 1 end
  elseif k == "ok" then term_run(T.cmds[T.ci]) end
  return nil
end

-- ================= NOTES (trang ve luu san) =================
A.notes = { sel = 1, info = {} }

function A.notes_enter()
  for s = 1, 3 do
    local t = A.paint_load(s)
    A.notes.info[s] = #t / 5
  end
end

function A.notes_draw()
  D.paper()
  D.title(40, "Notes")
  for i = 1, 3 do
    local y = 84 + (i - 1) * 48
    D.skrect(38, y, 30, 36, C.pen, 110 + i)
    E.line(43, y + 12, 63, y + 12, C.ruled)
    E.line(43, y + 22, 63, y + 22, C.ruled)
    local n = A.notes.info[i] or 0
    E.set_font(12)
    E.text(78, y + 2, "Trang " .. i, (i == A.notes.sel) and C.pen or C.pencil)
    E.set_font(8)
    E.text(78, y + 24, (n > 0) and ("co " .. n .. " net") or "trong", C.pencil)
    if i == A.notes.sel then D.arrow(28, y + 24, 34, y + 24, C.pen2, i) end
  end
  D.footer("START mo ve - OPTION xoa - A ve")
end

function A.notes_key(k)
  if is_back(k) then return "menu" end
  if k == "up" then A.notes.sel = A.notes.sel - 1 if A.notes.sel < 1 then A.notes.sel = 3 end
  elseif k == "down" then A.notes.sel = A.notes.sel + 1 if A.notes.sel > 3 then A.notes.sel = 1 end
  elseif k == "ok" then
    A.from_notes = true
    A.paint_open(A.notes.sel)
    return "paint"
  elseif k == "option" then
    if E.has_files then E.file_delete("trang" .. A.notes.sel .. ".dat") end
    A.notes_enter()
  end
  return nil
end

-- ================= LORA =================
A.lora = { ch = 1, log = {}, n = 0 }

function A.lora_draw()
  D.paper()
  D.title(40, "LoRa")
  E.text(36, 82, "kenh: " .. A.lora.ch, C.pen)
  E.text(120, 82, "< > doi kenh", C.pencil)
  D.spiral(200, 110, 14, C.faint, 5)
  local y = 120
  for _, s in ipairs(A.lora.log) do
    E.text(36, y, s:sub(1, 28), C.pencil)
    y = y + 18
  end
  D.footer("START gui ban tin - A ve")
end

function A.lora_key(k)
  if is_back(k) then return "menu" end
  if k == "left" then A.lora.ch = A.lora.ch - 1 if A.lora.ch < 1 then A.lora.ch = 8 end
  elseif k == "right" then A.lora.ch = A.lora.ch + 1 if A.lora.ch > 8 then A.lora.ch = 1 end
  elseif k == "ok" then
    A.lora.n = A.lora.n + 1
    A.lora.log[#A.lora.log + 1] = "gui #" .. A.lora.n .. " kenh " .. A.lora.ch .. ": OK"
    while #A.lora.log > 6 do table.remove(A.lora.log, 1) end
  end
  return nil
end

-- ================= IR (hong ngoai) =================
A.ir = { sel = 1, msg = nil, list = { "TV nguon", "TV am +", "TV am -", "Quat", "Dieu hoa" } }

function A.ir_draw()
  D.paper()
  D.title(40, "IR hong ngoai")
  for i, m in ipairs(A.ir.list) do
    local y = 80 + (i - 1) * 38
    E.text(44, y, m, (i == A.ir.sel) and C.pen or C.pencil)
    if i == A.ir.sel then D.arrow(30, y + 6, 40, y + 6, C.pen2, i) end
  end
  D.arc(200, 120, 10, -1.2, 1.2, C.faint)
  D.arc(200, 120, 16, -1.2, 1.2, C.faint)
  D.footer("START phat tin hieu - A ve")
  if A.ir.msg then D.dialog("IR", { A.ir.msg }) end
end

function A.ir_key(k)
  if A.ir.msg then A.ir.msg = nil return nil end
  if is_back(k) then return "menu" end
  if k == "up" then A.ir.sel = A.ir.sel - 1 if A.ir.sel < 1 then A.ir.sel = 5 end
  elseif k == "down" then A.ir.sel = A.ir.sel + 1 if A.ir.sel > 5 then A.ir.sel = 1 end
  elseif k == "ok" then A.ir.msg = "da phat: " .. A.ir.list[A.ir.sel] end
  return nil
end

-- ================= BROWSER =================
A.web = { sel = 1, page = nil,
  marks = {
    { "Lop hoc", { "Chao lop!", "Hom nay hoc ve Lua.", "Bai tap: ve hinh sao." } },
    { "Tin tuc", { "Troi dep.", "Doi bong thang 2-0." } },
    { "Thoi tiet", { "Nang 32 do.", "Chieu co mua rao." } },
  } }

function A.web_draw()
  D.paper()
  D.title(40, "Browser")
  if A.web.page then
    local p = A.web.marks[A.web.page]
    E.text(34, 78, "trang: " .. p[1], C.pen)
    D.wline(34, 96, 206, 95, C.pen2, 2)
    for i, s in ipairs(p[2]) do E.text(34, 104 + (i - 1) * 20, s:sub(1, 28), C.pencil) end
    D.footer("A dong trang")
  else
    for i, m in ipairs(A.web.marks) do
      local y = 82 + (i - 1) * 44
      D.skcircle(44, y + 8, 8, (i == A.web.sel) and C.pen or C.pencil, i)
      E.text(62, y + 2, m[1], (i == A.web.sel) and C.pen or C.pencil)
      if i == A.web.sel then D.arrow(28, y + 10, 38, y + 10, C.pen2, i) end
    end
    D.footer("START mo trang - A ve")
  end
end

function A.web_key(k)
  if A.web.page then
    if is_back(k) then A.web.page = nil end
    return nil
  end
  if is_back(k) then return "menu" end
  if k == "up" then A.web.sel = A.web.sel - 1 if A.web.sel < 1 then A.web.sel = 3 end
  elseif k == "down" then A.web.sel = A.web.sel + 1 if A.web.sel > 3 then A.web.sel = 1 end
  elseif k == "ok" then A.web.page = A.web.sel end
  return nil
end

-- ================= SETTINGS =================
A.settings = { sel = 1, msg = "" }

function A.settings_draw()
  D.paper()
  D.title(40, "Settings")
  local y = 82
  E.text(36, y, "Toc do khung:", C.pen)
  E.text(150, y, OS.settings.fps .. " fps", C.pen2)
  y = y + 44
  E.text(36, y, "But mac dinh:", C.pen)
  E.text(150, y, (OS.settings.tool == 2) and "but chi" or "but bi", C.pen2)
  y = y + 44
  E.text(36, y, "Bluetooth:", C.pen)
  D.toggle(150, y - 2, OS.settings.bt)
  y = y + 44
  E.text(36, y, "Xoa du lieu", (A.settings.sel == 4) and C.pen2 or C.pen)
  if A.settings.sel < 4 then
    D.arrow(28, 82 + (A.settings.sel - 1) * 44 + 6, 34, 82 + (A.settings.sel - 1) * 44 + 6, C.pen2, 9)
  else
    D.arrow(28, y + 6, 34, y + 6, C.pen2, 9)
  end
  if A.settings.msg ~= "" then E.text(36, 272, A.settings.msg, C.pen2) end
  D.footer("</> doi - START xoa - A ve (fps doi conf.lua)")
end

function A.settings_key(k)
  A.settings.msg = ""
  if is_back(k) then return "menu" end
  if k == "up" then A.settings.sel = A.settings.sel - 1 if A.settings.sel < 1 then A.settings.sel = 4 end
  elseif k == "down" then A.settings.sel = A.settings.sel + 1 if A.settings.sel > 4 then A.settings.sel = 1 end
  elseif k == "left" or k == "right" then
    local d = (k == "right") and 1 or -1
    if A.settings.sel == 1 then
      OS.settings.fps = (OS.settings.fps == 30) and 15 or 30
    elseif A.settings.sel == 2 then
      OS.settings.tool = OS.settings.tool + d
      if OS.settings.tool < 1 then OS.settings.tool = 2 end
      if OS.settings.tool > 2 then OS.settings.tool = 1 end
    elseif A.settings.sel == 3 then
      OS.settings.bt = not OS.settings.bt
    end
    OS.save_settings()
  elseif k == "ok" then
    if A.settings.sel == 4 and E.has_files then
      E.file_delete("dem.dat"); E.file_delete("wifi.dat")
      E.file_delete("trang1.dat"); E.file_delete("trang2.dat"); E.file_delete("trang3.dat")
      A.settings.msg = "da xoa du lieu!"
    end
  end
  return nil
end

-- ================= RADIO =================
A.radio = { f = 91.5, play = false }

function A.radio_draw()
  D.paper()
  D.title(40, "Radio")
  local st = "Dai " .. ((A.radio.f < 95) and "A" or ((A.radio.f < 102) and "B" or "C"))
  E.set_font(12)
  E.text(60, 100, string.format("%.1f", A.radio.f), C.pen)
  E.set_font(8)
  E.text(150, 104, "MHz", C.pencil)
  E.text(60, 126, st, C.pencil)
  if A.radio.play then
    local t = E.tick_ms() / 300
    for i = 0, 6 do
      local h = 6 + math.floor(10 * math.abs(math.sin(t + i * 0.8)))
      E.rect(52 + i * 20, 220 - h, 10, h, C.pen)
    end
    E.text(60, 240, "dang phat...", C.pen2)
  else
    E.text(60, 200, "dung. START de mo.", C.pencil)
  end
  D.footer("</> do tan so - START mo/tat - A ve")
end

function A.radio_key(k)
  if is_back(k) then return "menu" end
  if k == "left" then A.radio.f = A.radio.f - 0.5 if A.radio.f < 87.5 then A.radio.f = 108 end
  elseif k == "right" then A.radio.f = A.radio.f + 0.5 if A.radio.f > 108 then A.radio.f = 87.5 end
  elseif k == "ok" then A.radio.play = not A.radio.play end
  return nil
end

-- ================= MUSIC =================
A.music = { sel = 1, play = nil,
  tracks = { "Bai ca lop hoc", "Trong com", "Que huong" } }

function A.music_draw()
  D.paper()
  D.title(40, "Music")
  for i, t in ipairs(A.music.tracks) do
    local y = 84 + (i - 1) * 44
    E.text(44, y, t, (i == A.music.sel) and C.pen or C.pencil)
    if A.music.play == i then
      local w = E.tick_ms() / 400
      E.rect(44, y + 20 + math.floor(4 * math.sin(w)), 4, 4, C.pen2)
      E.rect(58, y + 20 + math.floor(4 * math.sin(w + 1)), 4, 4, C.pen2)
      E.rect(72, y + 20 + math.floor(4 * math.sin(w + 2)), 4, 4, C.pen2)
    end
    if i == A.music.sel then D.arrow(30, y + 8, 40, y + 8, C.pen2, i) end
  end
  E.text(36, 240, "chua co loa - hat chay!", C.pencil)
  D.footer("START phat/dung - A ve")
end

function A.music_key(k)
  if is_back(k) then A.music.play = nil return "menu" end
  if k == "up" then A.music.sel = A.music.sel - 1 if A.music.sel < 1 then A.music.sel = 3 end
  elseif k == "down" then A.music.sel = A.music.sel + 1 if A.music.sel > 3 then A.music.sel = 1 end
  elseif k == "ok" then
    if A.music.play == A.music.sel then A.music.play = nil
    else A.music.play = A.music.sel end
  end
  return nil
end

-- ================= RETRO (ran san moi) =================
A.snake = { body = {}, dir = { 0, -1 }, apple = { 3, 3 }, score = 0, hi = 0, acc = 0, over = false }
local SC = 12
local SX0, SY0, SCOLS, SROWS = 48, 84, 12, 15

function A.snake_enter()
  local S = A.snake
  S.body = { { 6, 8 }, { 6, 9 }, { 6, 10 } }
  S.dir = { 0, -1 }
  S.apple = { 3, 3 }
  S.score = 0; S.acc = 0; S.over = false
  if E.has_files then
    local s = E.file_read("diem.dat")
    if s then S.hi = tonumber(s) or 0 end
  end
end

local function snake_new_apple()
  local S = A.snake
  for _ = 1, 30 do
    local ax = (E.tick_ms() / 97 + S.score * 13 + #S.body * 7) % SCOLS
    local ay = (E.tick_ms() / 131 + S.score * 7) % SROWS
    ax = math.floor(ax); ay = math.floor(ay)
    local bad = false
    for _, c in ipairs(S.body) do if c[1] == ax and c[2] == ay then bad = true break end end
    if not bad then S.apple = { ax, ay } return end
  end
end

function A.snake_update(dt)
  local S = A.snake
  if S.over then return end
  S.acc = S.acc + dt
  if S.acc < 0.16 then return end
  S.acc = 0
  local h = S.body[1]
  local nx, ny = h[1] + S.dir[1], h[2] + S.dir[2]
  if nx < 0 or ny < 0 or nx >= SCOLS or ny >= SROWS then S.over = true
  else
    for _, c in ipairs(S.body) do
      if c[1] == nx and c[2] == ny then S.over = true break end
    end
  end
  if S.over then
    if S.score > S.hi then
      S.hi = S.score
      if E.has_files then E.file_write("diem.dat", tostring(S.hi)) end
    end
    return
  end
  table.insert(S.body, 1, { nx, ny })
  if nx == S.apple[1] and ny == S.apple[2] then
    S.score = S.score + 1
    snake_new_apple()
  else
    table.remove(S.body)
  end
end

function A.snake_draw()
  local S = A.snake
  D.paper()
  E.set_font(12)
  E.text(32, 40, "Ran", C.pen)
  E.set_font(8)
  E.text(120, 42, "diem " .. S.score .. "  cao " .. S.hi, C.pen2)
  D.skrect(SX0 - 3, SY0 - 3, SCOLS * SC + 6, SROWS * SC + 6, C.pencil, 121)
  for _, c in ipairs(S.body) do
    E.rect(SX0 + c[1] * SC + 1, SY0 + c[2] * SC + 1, SC - 2, SC - 2, C.pen)
  end
  local h = S.body[1]
  if h then E.rect(SX0 + h[1] * SC + 1, SY0 + h[2] * SC + 1, SC - 2, SC - 2, C.pen2) end
  D.skcircle(SX0 + S.apple[1] * SC + 6, SY0 + S.apple[2] * SC + 6, 4, C.pen2, 7)
  D.footer("phim dieu huong lai - A ve")
  if S.over then D.dialog("Thua!", { "diem: " .. S.score, "START choi lai" }) end
end

function A.snake_key(k)
  local S = A.snake
  if is_back(k) then return "menu" end
  if S.over then
    if k == "ok" then A.snake_enter() end
    return nil
  end
  local d = S.dir
  if k == "up" and d[2] ~= 1 then S.dir = { 0, -1 }
  elseif k == "down" and d[2] ~= -1 then S.dir = { 0, 1 }
  elseif k == "left" and d[1] ~= 1 then S.dir = { -1, 0 }
  elseif k == "right" and d[1] ~= -1 then S.dir = { 1, 0 } end
  return nil
end

-- ================= CHAT =================
A.chat = { msgs = { "Bot: Chao ban!" }, qs = { "Chao", "Ban ten gi", "May gio", "Hat mot cau", "Tam biet" }, qi = 1 }

local function bot_ans(q)
  local l = q:lower()
  if l:find("chao") then return "Chao! Toi la Pochita giay."
  elseif l:find("ten") then return "Toi ten Pochita, sinh trong vo."
  elseif l:find("gio") then return "Bay gio la tick " .. E.tick_ms() .. "."
  elseif l:find("hat") then return "La la la... hay khong?"
  elseif l:find("biet") then return "Tam biet! Hen gap lai."
  else return "Hmm, ban noi gi co?" end
end

local function chat_push(s)
  local M = A.chat.msgs
  M[#M + 1] = s
  while #M > 8 do table.remove(M, 1) end
end

function A.chat_draw()
  D.paper()
  D.title(40, "Chat")
  local M = A.chat.msgs
  local first = math.max(1, #M - 4)
  for i = first, #M do
    local mine = M[i]:sub(1, 3) == "Toi"
    E.text(32, 74 + (i - first) * 22, M[i]:sub(1, 30), mine and C.pen or C.pencil)
  end
  E.text(32, 250, "? " .. A.chat.qs[A.chat.qi], C.pen2)
  D.wline(32, 266, 200, 265, C.faint, 6)
  D.footer("len/xuong chon cau - START gui")
end

function A.chat_key(k)
  local Ch = A.chat
  if is_back(k) then return "menu" end
  if k == "up" then Ch.qi = Ch.qi - 1 if Ch.qi < 1 then Ch.qi = #Ch.qs end
  elseif k == "down" then Ch.qi = Ch.qi + 1 if Ch.qi > #Ch.qs then Ch.qi = 1 end
  elseif k == "ok" then
    local q = Ch.qs[Ch.qi]
    chat_push("Toi: " .. q)
    chat_push("Bot: " .. bot_ans(q))
  end
  return nil
end

-- ================= VM (thong tin may) =================
function A.vm_draw()
  D.paper()
  D.title(40, "VM")
  local d = E.device_info()
  local y = 80
  local function row(k, v)
    E.text(32, y, k, C.pencil)
    E.text(120, y, v:sub(1, 18), C.pen)
    y = y + 19
  end
  row("dong:", tostring(d.family))
  row("man hinh:", d.width .. "x" .. d.height)
  row("ban:", tostring(E.version):sub(1, 18))
  row("lua ram:", string.format("%.1f KB", collectgarbage("count")))
  row("so lan mo:", tostring(OS.boot_count))
  row("the nho:", (E.sd_ok and E.sd_ok()) and "co" or "khong")
  D.skrect(190, 220, 24, 24, C.faint, 55)
  for i = 0, 2 do
    E.line(190, 226 + i * 6, 196, 226 + i * 6, C.faint)
    E.line(208, 226 + i * 6, 214, 226 + i * 6, C.faint)
  end
  D.footer("A ve menu")
end

function A.vm_key(k)
  if is_back(k) then return "menu" end
  return nil
end

-- ================= dispatch =================
function A.enter(id)
  if id == "wifi" then A.wifi_enter()
  elseif id == "notes" then A.notes_enter()
  elseif id == "paint" then
    -- Notes da goi paint_open(slot) truoc khi chuyen sang; chi mo trang
    -- mac dinh khi vao thang tu menu.
    if A.from_notes then A.from_notes = false
    else A.paint_open(1) end
  elseif id == "retro" then A.snake_enter() end
end

function A.update(id, dt, keys)
  if id == "paint" then A.paint_update(dt, keys)
  elseif id == "wifi" then A.wifi_update(dt)
  elseif id == "retro" then A.snake_update(dt) end
end

function A.draw(id)
  if id == "wifi" then A.wifi_draw()
  elseif id == "files" then A.files_draw()
  elseif id == "bluetooth" then A.bt_draw()
  elseif id == "terminal" then A.term_draw()
  elseif id == "notes" then A.notes_draw()
  elseif id == "lora" then A.lora_draw()
  elseif id == "ir" then A.ir_draw()
  elseif id == "browser" then A.web_draw()
  elseif id == "settings" then A.settings_draw()
  elseif id == "radio" then A.radio_draw()
  elseif id == "music" then A.music_draw()
  elseif id == "paint" then A.paint_draw()
  elseif id == "retro" then A.snake_draw()
  elseif id == "chat" then A.chat_draw()
  elseif id == "vm" then A.vm_draw()
  end
end

function A.key(id, k)
  if id == "wifi" then return A.wifi_key(k)
  elseif id == "files" then return A.files_key(k)
  elseif id == "bluetooth" then return A.bt_key(k)
  elseif id == "terminal" then return A.term_key(k)
  elseif id == "notes" then return A.notes_key(k)
  elseif id == "lora" then return A.lora_key(k)
  elseif id == "ir" then return A.ir_key(k)
  elseif id == "browser" then return A.web_key(k)
  elseif id == "settings" then return A.settings_key(k)
  elseif id == "radio" then return A.radio_key(k)
  elseif id == "music" then return A.music_key(k)
  elseif id == "paint" then return A.paint_key(k)
  elseif id == "retro" then return A.snake_key(k)
  elseif id == "chat" then return A.chat_key(k)
  elseif id == "vm" then return A.vm_key(k) end
  return nil
end

return A
