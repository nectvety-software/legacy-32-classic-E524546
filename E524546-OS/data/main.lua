-- main.lua — E524546 Doodle OS: menu + 15 app kieu PochitaOS, ve tay vo hoc sinh.
local E = engine
local D = require("src.doodle")
local OS = require("src.os")
local APPS = require("src.apps")
local C = D.C

local screen = "boot" -- boot | menu | <app id> | paint
local t0 = 0
local keys = {}

function E.load()
  E.set_font(8)
  t0 = E.tick_ms()
  OS.load_all()
end

function E.update(dt)
  dt = dt or 0.033
  if screen == "boot" then
    if E.tick_ms() - t0 > 1600 then screen = "menu" end
    return
  end
  APPS.update(screen, dt, keys)
end

local function draw_boot()
  D.paper()
  E.set_font(12)
  E.text(40, 120, "E524546-OS", C.pen)
  E.set_font(8)
  E.text(40, 142, "vo hoc sinh - pochita style", C.pencil)
  D.star(196, 70, 14, C.pen2, 3)
  D.spiral(52, 230, 16, C.pencil, 2)
  D.arrow(60, 250, 180, 250, C.pen, 8)
  local t = math.min(1, (E.tick_ms() - t0) / 1500)
  D.progress(38, 264, 164, 14, t, C.pen, 11)
  D.footer("dang mo vo...")
end

function E.draw()
  if screen == "boot" then draw_boot()
  elseif screen == "menu" then OS.draw_launcher()
  else APPS.draw(screen) end
  E.flush()
end

function E.keypressed(k)
  k = tostring(k):lower()
  keys[k] = true
  if screen == "boot" then return end
  if screen == "menu" then
    if k == "up" then OS.move(-1)
    elseif k == "down" then OS.move(1)
    elseif k == "ok" then
      screen = OS.apps[OS.sel].id
      APPS.enter(screen)
    end
    return
  end
  local next_ = APPS.key(screen, k)
  if next_ then
    screen = next_
    if screen ~= "menu" then APPS.enter(screen) end
  end
end

function E.keyreleased(k)
  keys[tostring(k):lower()] = false
end
