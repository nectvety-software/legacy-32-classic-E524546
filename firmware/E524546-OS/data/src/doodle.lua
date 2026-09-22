-- src/doodle.lua — Net ve tay Ballpoint Pen & Pencil tren vo hoc sinh.
-- Tat ca procedural, chi dung engine.* (line/rect/frame/text), khong can asset.
-- Chu tren man hinh co y khong dau de font mac dinh hien dung.
local E = engine

local D = {}

D.C = {
  paper  = E.color(253, 248, 232),  -- giay vo nga vang
  ruled  = E.color(150, 180, 220),  -- ke ngang xanh mo
  margin = E.color(220, 90, 90),    -- le doc do
  pen    = E.color(20, 60, 165),    -- but bi xanh
  pen2   = E.color(180, 40, 40),    -- but bi do (gach chan, danh dau)
  pencil = E.color(95, 95, 105),    -- but chi xam
  faint  = E.color(170, 165, 150),  -- chi mo (phac thao)
}

-- Duong thang run tay: cat nho + lech sin de tao cam giac ve tay.
function D.wline(x1, y1, x2, y2, c, seed)
  seed = seed or 1
  local dx, dy = x2 - x1, y2 - y1
  local len = math.sqrt(dx * dx + dy * dy)
  if len < 1 then return end
  local n = math.max(2, math.floor(len / 8))
  local nx, ny = -dy / len, dx / len
  local px, py = x1, y1
  for i = 1, n do
    local t = i / n
    local off = math.sin(i * 12.9898 + seed * 78.233) * 1.3
    local qx = x1 + dx * t + nx * off
    local qy = y1 + dy * t + ny * off
    E.line(math.floor(px), math.floor(py), math.floor(qx), math.floor(qy), c)
    px, py = qx, qy
  end
end

-- Hinh chu nhat phec thao: 2 net de + lech 1px.
function D.skrect(x, y, w, h, c, seed)
  seed = seed or 5
  D.wline(x, y, x + w, y, c, seed)
  D.wline(x + w, y, x + w, y + h, c, seed + 1)
  D.wline(x + w, y + h, x, y + h, c, seed + 2)
  D.wline(x, y + h, x, y, c, seed + 3)
  D.wline(x + 1, y + 1, x + w + 1, y + 1, c, seed + 4) -- net de
end

-- Tron phec thao: 2 vong lech nhau.
function D.skcircle(cx, cy, r, c, seed)
  seed = seed or 9
  for pass = 0, 1 do
    local n = 18
    local a0 = seed + pass * 0.35
    local px = cx + (r + pass) * math.cos(a0)
    local py = cy + (r + pass) * math.sin(a0)
    for i = 1, n do
      local a = a0 + i / n * 6.2832
      local rr = r + pass + math.sin(i * 7.3 + seed * 3.1) * 1.1
      local qx = cx + rr * math.cos(a)
      local qy = cy + rr * math.sin(a)
      E.line(math.floor(px), math.floor(py), math.floor(qx), math.floor(qy), c)
      px, py = qx, qy
    end
  end
end

function D.check(x, y, s, c, seed)
  D.wline(x, y + s * 0.5, x + s * 0.4, y + s, c, seed or 2)
  D.wline(x + s * 0.4, y + s, x + s, y, c, (seed or 2) + 1)
end

function D.cross(x, y, s, c, seed)
  D.wline(x, y, x + s, y + s, c, seed or 3)
  D.wline(x + s, y, x, y + s, c, (seed or 3) + 1)
end

function D.star(cx, cy, r, c, seed)
  seed = seed or 7
  local px, py = nil, nil
  for i = 0, 10 do
    local rr = (i % 2 == 0) and r or (r * 0.45)
    local a = -1.5708 + i * 0.6283 + seed * 0.05
    local qx = cx + rr * math.cos(a)
    local qy = cy + rr * math.sin(a)
    if px then
      D.wline(math.floor(px), math.floor(py), math.floor(qx), math.floor(qy), c, seed + i)
    end
    px, py = qx, qy
  end
end

function D.arrow(x1, y1, x2, y2, c, seed)
  seed = seed or 4
  D.wline(x1, y1, x2, y2, c, seed)
  local a = math.atan2(y2 - y1, x2 - x1)
  local h = 7
  D.wline(x2, y2, math.floor(x2 - h * math.cos(a - 0.5)), math.floor(y2 - h * math.sin(a - 0.5)), c, seed + 1)
  D.wline(x2, y2, math.floor(x2 - h * math.cos(a + 0.5)), math.floor(y2 - h * math.sin(a + 0.5)), c, seed + 2)
end

function D.spiral(cx, cy, rmax, c, seed)
  seed = seed or 6
  local px, py = cx, cy
  local steps = 26
  for i = 1, steps do
    local a = seed + i * 0.55
    local r = rmax * i / steps
    local qx = cx + r * math.cos(a)
    local qy = cy + r * math.sin(a)
    E.line(math.floor(px), math.floor(py), math.floor(qx), math.floor(qy), c)
    px, py = qx, qy
  end
end

-- Nen giay vo: ke ngang + le do + 2 lo duc.
function D.paper()
  local C = D.C
  E.clear(C.paper)
  for y = 30, 318, 20 do
    E.line(0, y, 239, y, C.ruled)
  end
  E.line(24, 0, 24, 319, C.margin)
  E.line(26, 0, 26, 319, C.margin)
  for _, hy in ipairs({ 90, 230 }) do
    E.rect(5, hy - 7, 14, 14, C.paper)
    D.skcircle(12, hy, 7, C.pencil, hy)
  end
end

-- Tieu de kieu tieu de vo: chu but bi + gach chan do.
function D.title(y, s)
  E.set_font(12)
  E.text(32, y, s, D.C.pen)
  local w = E.text_width(s)
  D.wline(32, y + 18, 32 + w + 8, y + 17, D.C.pen2, y)
  E.set_font(8)
end

function D.footer(s)
  E.text(30, 306, s, D.C.pencil)
end

-- Cung tron (song wifi, loa, hinh trang tri).
function D.arc(cx, cy, r, a0, a1, c)
  local n = math.max(3, math.floor(r * (a1 - a0) / 6))
  local px = cx + r * math.cos(a0)
  local py = cy + r * math.sin(a0)
  for i = 1, n do
    local a = a0 + (a1 - a0) * i / n
    local qx = cx + r * math.cos(a)
    local qy = cy + r * math.sin(a)
    E.line(math.floor(px), math.floor(py), math.floor(qx), math.floor(qy), c)
    px, py = qx, qy
  end
end

-- Thanh tien trinh kieu gach tay (frac 0..1).
function D.progress(x, y, w, h, frac, c, seed)
  if frac < 0 then frac = 0 elseif frac > 1 then frac = 1 end
  D.skrect(x, y, w, h, D.C.pencil, seed or 21)
  local fw = math.floor((w - 4) * frac)
  if fw > 0 then E.rect(x + 2, y + 2, fw, h - 4, c) end
end

-- Hop thoai giay note giua man hinh.
function D.dialog(title, lines)
  local w = 196
  local h = 48 + #lines * 16
  local x = math.floor((240 - w) / 2)
  local y = math.floor((320 - h) / 2)
  E.rect(x, y, w, h, D.C.paper)
  D.skrect(x, y, w, h, D.C.pen, 31)
  E.set_font(12)
  E.text(x + 10, y + 8, title, D.C.pen)
  E.set_font(8)
  D.wline(x + 10, y + 28, x + w - 10, y + 27, D.C.pen2, 32)
  for i, s in ipairs(lines) do
    E.text(x + 10, y + 34 + (i - 1) * 16, s, D.C.pencil)
  end
end

-- Cong tac bat/tat.
function D.toggle(x, y, on)
  D.skrect(x, y, 26, 14, D.C.pen, 41)
  if on then E.rect(x + 14, y + 2, 10, 10, D.C.pen)
  else E.rect(x + 2, y + 2, 10, 10, D.C.pencil) end
end

-- O khoa (mang bao mat).
function D.lock(x, y, s, c, seed)
  D.skrect(x, math.floor(y + s * 0.4), s, math.floor(s * 0.6), c, seed or 51)
  D.arc(x + s / 2, y + s * 0.4, s * 0.3, 3.1416, 6.2832, c)
end

-- Cot song wifi 0..4.
function D.bars(x, y, n, c, dim)
  for b = 0, 3 do
    local bh = 3 + b * 3
    if b < n then E.rect(x + b * 6, y + 12 - bh, 4, bh, c)
    else E.rect(x + b * 6, y + 12 - bh, 4, bh, dim) end
  end
end

-- Icon ve tay 22x24 cho 15 app kieu PochitaOS.
function D.icon(name, x, y)
  local c = D.C.pen
  if name == "wifi" then
    E.rect(x + 9, y + 18, 4, 4, c)
    D.arc(x + 11, y + 20, 7, -2.4, -0.7, c)
    D.arc(x + 11, y + 20, 12, -2.4, -0.7, c)
  elseif name == "files" then
    E.rect(x + 1, y + 2, 8, 4, c)
    D.skrect(x + 1, y + 5, 20, 15, c, 61)
  elseif name == "bluetooth" then
    D.wline(x + 11, y + 1, x + 11, y + 23, c, 62)
    D.wline(x + 11, y + 1, x + 18, y + 7, c, 63)
    D.wline(x + 18, y + 7, x + 11, y + 13, c, 64)
    D.wline(x + 11, y + 13, x + 18, y + 19, c, 65)
    D.wline(x + 18, y + 19, x + 11, y + 23, c, 66)
  elseif name == "terminal" then
    D.skrect(x + 1, y + 4, 20, 16, c, 67)
    E.text(x + 4, y + 8, ">_", c)
  elseif name == "notes" then
    D.skrect(x + 1, y + 1, 20, 22, c, 68)
    E.line(x + 5, y + 9, x + 17, y + 9, D.C.ruled)
    E.line(x + 5, y + 15, x + 17, y + 15, D.C.ruled)
  elseif name == "lora" then
    D.wline(x + 4, y + 23, x + 11, y + 8, c, 69)
    D.wline(x + 18, y + 23, x + 11, y + 8, c, 70)
    D.arc(x + 11, y + 8, 6, -2.6, -0.5, c)
    D.arc(x + 11, y + 8, 10, -2.6, -0.5, c)
  elseif name == "ir" then
    D.skrect(x + 1, y + 7, 10, 13, c, 71)
    E.rect(x + 4, y + 10, 4, 4, c)
    D.arc(x + 11, y + 13, 5, -1.2, 1.2, c)
    D.arc(x + 11, y + 13, 9, -1.2, 1.2, c)
  elseif name == "browser" then
    D.skcircle(x + 11, y + 12, 9, c, 72)
    E.line(x + 2, y + 12, x + 20, y + 12, c)
    E.line(x + 11, y + 3, x + 11, y + 21, c)
  elseif name == "settings" then
    D.skcircle(x + 11, y + 12, 6, c, 73)
    for i = 0, 7 do
      local a = i * 0.7854
      E.line(x + 11 + math.floor(6 * math.cos(a)), y + 12 + math.floor(6 * math.sin(a)),
             x + 11 + math.floor(9 * math.cos(a)), y + 12 + math.floor(9 * math.sin(a)), c)
    end
  elseif name == "radio" then
    D.skrect(x + 1, y + 10, 20, 12, c, 74)
    D.wline(x + 18, y + 10, x + 22, y + 2, c, 75)
    E.rect(x + 4, y + 14, 3, 3, c)
    E.rect(x + 9, y + 14, 3, 3, c)
  elseif name == "music" then
    E.line(x + 7, y + 4, x + 7, y + 18, c)
    E.line(x + 16, y + 6, x + 16, y + 20, c)
    E.line(x + 7, y + 4, x + 16, y + 6, c)
    E.rect(x + 3, y + 16, 5, 4, c)
    E.rect(x + 12, y + 18, 5, 4, c)
  elseif name == "paint" then
    D.wline(x + 4, y + 20, x + 16, y + 6, c, 76)
    D.wline(x + 16, y + 6, x + 19, y + 3, c, 77)
    E.rect(x + 2, y + 19, 5, 4, c)
  elseif name == "retro" then
    D.skrect(x + 1, y + 7, 20, 12, c, 78)
    E.line(x + 5, y + 12, x + 9, y + 12, c)
    E.line(x + 7, y + 10, x + 7, y + 14, c)
    E.rect(x + 14, y + 10, 3, 3, c)
    E.rect(x + 17, y + 13, 2, 2, c)
  elseif name == "chat" then
    D.skrect(x + 1, y + 3, 20, 13, c, 79)
    D.wline(x + 6, y + 16, x + 4, y + 21, c, 80)
    E.rect(x + 5, y + 8, 2, 2, c)
    E.rect(x + 10, y + 8, 2, 2, c)
    E.rect(x + 15, y + 8, 2, 2, c)
  elseif name == "vm" then
    D.skrect(x + 5, y + 6, 12, 12, c, 81)
    for i = 0, 2 do
      E.line(x + 1, y + 8 + i * 4, x + 5, y + 8 + i * 4, c)
      E.line(x + 17, y + 8 + i * 4, x + 21, y + 8 + i * 4, c)
    end
  end
end

return D
