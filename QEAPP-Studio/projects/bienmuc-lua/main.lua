-- Bien Muc Lite: doodle sea duel for VQEAF Lua Beta.
-- API: engine.clear/rect/text/blit1/heap_used/heap_peak, keys up/down/left/right/start/option.
-- Canvas 240x270. Bounded pools, no file/network/audio.

local W, H = 240, 270
local SEA = 168
local SHIP_X = 56

-- engine.rect/text require integer coordinates (Lua 5.4 / is float).
local function I(v)
  return math.floor(v)
end
local function R(x, y, w, h, c)
  if w < 1 or h < 1 then return end
  engine.rect(I(x), I(y), I(w), I(h), c)
end
local function T(x, y, s, c)
  engine.text(I(x), I(y), s, c)
end

local C = {
  paper = 0xF7F2, line = 0xC6B8, ink = 0x2104, sea = 0x3AD7, sea2 = 0x2A55,
  red = 0xC986, white = 0xFFFF, yellow = 0xFFE0, green = 0x2E66, orange = 0xFC00,
  gray = 0x8C51, dark = 0x10A2
}

local state = 'title' -- title | play | pause | over
local score, wave, wave_t, banner = 0, 1, 0, 0
local hp, ink, cool, inv = 3, 100, 0, 0
local ship_y, aim, scroll = 140, 0, 0
local seed = 20260925
local foes, shots, balls, pops = {}, {}, {}, {}

local function rnd(n)
  seed = (seed * 1103515245 + 12345) % 2147483647
  return (seed % n)
end

local KIND = {
  {w=22,h=10,hp=2,sp=28,sc=100,col=C.dark},   -- boat
  {w=12,h=8,hp=1,sp=48,sc=40,col=C.sea2},     -- fish
  {w=14,h=28,hp=3,sp=18,sc=80,col=C.gray},    -- arm
  {w=18,h=14,hp=2,sp=12,sc=120,col=C.orange}, -- blimp
}

local function reset()
  score, wave, wave_t, banner = 0, 1, 0, 1.2
  hp, ink, cool, inv = 3, 100, 0, 0
  ship_y, aim, scroll = 140, 0, 0
  foes, shots, balls, pops = {}, {}, {}, {}
  state = 'play'
end

local function add_foe(kind)
  if #foes >= 8 then return end
  local k = KIND[kind]
  foes[#foes+1] = {
    kind=kind, x=W+10+rnd(30), y=40+rnd(SEA-50),
    hp=k.hp, t=0, fire=1.2+rnd(20)/10
  }
end

local function pop(x,y,t,col)
  if #pops >= 6 then return end
  pops[#pops+1] = {x=x,y=y,t=t,life=0.8,col=col or C.red}
end

local function fire()
  if cool > 0 or ink < 8 then return end
  ink = ink - 8
  cool = 0.22
  shots[#shots+1] = {x=SHIP_X+18, y=ship_y+aim, vx=170, vy=aim*1.6, life=1.2}
end

local function scribble()
  if ink < 70 then return end
  ink = ink - 70
  for i=#foes,1,-1 do
    local f = foes[i]
    score = score + KIND[f.kind].sc
    pop(f.x, f.y, 'X', C.red)
    table.remove(foes, i)
  end
  banner = 0.6
end

local function hit_ship(dmg)
  if inv > 0 then return end
  hp = hp - dmg
  inv = 1.0
  pop(SHIP_X, ship_y, '!', C.red)
  if hp <= 0 then state = 'over' end
end

function on_key(key, down)
  if not down then return end
  if state == 'title' then
    if key == 'start' or key == 'option' then reset() end
    return
  end
  if state == 'over' then
    if key == 'start' or key == 'option' then reset() end
    return
  end
  if key == 'option' then
    state = (state == 'pause') and 'play' or 'pause'
    return
  end
  if state ~= 'play' then return end
  if key == 'up' then ship_y = math.max(28, ship_y - 8); aim = math.max(-8, aim-2)
  elseif key == 'down' then ship_y = math.min(SEA-16, ship_y + 8); aim = math.min(8, aim+2)
  elseif key == 'left' then aim = math.max(-10, aim-3)
  elseif key == 'right' then aim = math.min(10, aim+3)
  elseif key == 'start' then fire()
  end
end

function on_update(dt)
  if state ~= 'play' then return end
  scroll = (scroll + 40*dt) % 40
  if cool > 0 then cool = cool - dt end
  if inv > 0 then inv = inv - dt end
  if banner > 0 then banner = banner - dt end
  ink = math.min(100, ink + 12*dt)
  wave_t = wave_t + dt
  if wave_t > 8 then
    wave_t = 0
    wave = wave + 1
    banner = 1.0
  end
  -- spawn
  if rnd(100) < 3 + wave then
    local r = rnd(100)
    local kind = 1
    if wave <= 1 then kind = (r < 70) and 1 or 2
    elseif wave <= 3 then kind = (r < 40) and 1 or ((r < 75) and 2 or 3)
    else kind = (r < 30) and 1 or ((r < 55) and 2 or ((r < 85) and 3 or 4))
    end
    add_foe(kind)
  end
  -- shots
  for i=#shots,1,-1 do
    local s = shots[i]
    s.x = s.x + s.vx*dt
    s.y = s.y + s.vy*dt
    s.life = s.life - dt
    if s.life <= 0 or s.x > W+8 or s.y < 8 or s.y > SEA then
      table.remove(shots, i)
    end
  end
  -- foes
  for i=#foes,1,-1 do
    local f = foes[i]
    local k = KIND[f.kind]
    f.t = f.t + dt
    f.x = f.x - k.sp*dt
    if f.kind == 2 then f.y = f.y + math.sin(f.t*3)*30*dt end
    if f.kind == 4 then f.y = f.y + (ship_y-f.y)*0.4*dt end
    f.fire = f.fire - dt
    if f.fire <= 0 and f.x < W-10 and f.x > 40 then
      f.fire = 1.6 + rnd(20)/10
      if #balls < 10 then
        local dx, dy = SHIP_X-f.x, ship_y-f.y
        local dist = math.max(1, math.sqrt(dx*dx+dy*dy))
        balls[#balls+1] = {x=f.x, y=f.y, vx=dx/dist*70, vy=dy/dist*70, life=3}
      end
    end
    if f.x < SHIP_X-10 then
      hit_ship(1)
      table.remove(foes, i)
    else
      -- collision with shots
      for j=#shots,1,-1 do
        local s = shots[j]
        if math.abs(s.x-f.x) < k.w/2+3 and math.abs(s.y-f.y) < k.h/2+3 then
          table.remove(shots, j)
          f.hp = f.hp - 1
          if f.hp <= 0 then
            score = score + k.sc
            pop(f.x, f.y, 'X', C.yellow)
            table.remove(foes, i)
          end
          break
        end
      end
    end
  end
  -- balls
  for i=#balls,1,-1 do
    local b = balls[i]
    b.x = b.x + b.vx*dt
    b.y = b.y + b.vy*dt
    b.life = b.life - dt
    if b.life <= 0 or b.x < 8 or b.x > W or b.y < 8 or b.y > SEA then
      table.remove(balls, i)
    elseif math.abs(b.x-SHIP_X) < 10 and math.abs(b.y-ship_y) < 10 then
      table.remove(balls, i)
      hit_ship(1)
    end
  end
  for i=#pops,1,-1 do
    pops[i].life = pops[i].life - dt
    pops[i].y = pops[i].y - 20*dt
    if pops[i].life <= 0 then table.remove(pops, i) end
  end
end

local function paper_bg()
  engine.clear(C.paper)
  -- horizontal notebook lines
  for y=8,SEA-8,14 do
    R(0, y, W, 1, C.line)
  end
  -- red margin
  R(28, 0, 1, SEA, C.red)
  -- sea band
  R(0, SEA, W, H-SEA, C.sea)
  R(0, SEA, W, 2, C.ink)
  for x=-scroll,W,20 do
    R(x, SEA+8, 12, 2, C.sea2)
    R(x+6, SEA+18, 10, 2, C.sea2)
    R(x+2, SEA+28, 14, 2, C.sea2)
  end
end

local function draw_ship()
  if inv > 0 and (math.floor(inv*10) % 2 == 0) then return end
  -- hull
  R(SHIP_X-12, ship_y, 26, 8, C.ink)
  R(SHIP_X-8, ship_y-6, 14, 6, C.ink)
  R(SHIP_X-2, ship_y-14, 2, 8, C.dark)
  R(SHIP_X-2, ship_y-14, 10, 2, C.red)
  -- gun
  local gx, gy = SHIP_X+12, ship_y+2+aim*0.3
  R(gx, gy-1, 8, 2, C.dark)
end

function on_draw()
  paper_bg()
  if state == 'title' then
    R(30, 60, 180, 100, C.white)
    R(30, 60, 180, 2, C.ink)
    R(30, 158, 180, 2, C.ink)
    R(30, 60, 2, 100, C.ink)
    R(208, 60, 2, 100, C.ink)
    T(48, 78, 'BIEN MUC LITE', C.ink)
    T(48, 100, 'START = SAIL', C.red)
    T(48, 118, 'OPTION = PAUSE', C.dark)
    T(48, 136, 'START = FIRE', C.dark)
    draw_ship()
    return
  end

  -- HUD
  R(0, 0, W, 22, C.white)
  R(0, 21, W, 1, C.ink)
  T(6, 4, 'SCORE', C.dark)
  T(52, 4, string.format('%05d', score), C.ink)
  T(110, 4, 'W'..wave, C.dark)
  -- hp
  for i=1,3 do
    local col = (i<=hp) and C.red or C.gray
    R(140+(i-1)*12, 4, 8, 8, col)
  end
  -- ink bar
  R(186, 4, 48, 8, C.gray)
  R(186, 4, I(48*ink/100), 8, C.sea)
  R(186, 4, 48, 1, C.ink)
  R(186, 11, 48, 1, C.ink)

  -- foes
  for i=1,#foes do
    local f = foes[i]
    local k = KIND[f.kind]
    local w,h = k.w, k.h
    local x,y = f.x, f.y
    if f.kind == 1 then
      R(x-w/2, y, w, 4, k.col)
      R(x-w/2+4, y-4, w-8, 4, k.col)
      R(x-1, y-8, 2, 4, C.ink)
    elseif f.kind == 2 then
      R(x-w/2, y-h/2, w, h, k.col)
      R(x-w/2+2, y-h/2+2, w-4, h-4, C.sea)
      R(x-w/2, y, w, 2, C.ink)
    elseif f.kind == 3 then
      R(x-w/2, y-h/2, w, h, k.col)
      R(x-w/2+3, y-h/2+3, w-6, h-6, C.dark)
    else
      R(x-w/2, y-4, w, 8, k.col)
      R(x-4, y-8, 8, 4, C.ink)
    end
  end

  -- shots
  for i=1,#shots do
    local s = shots[i]
    R(s.x, s.y-1, 6, 2, C.ink)
    R(s.x-3, s.y-1, 3, 2, C.orange)
  end
  -- balls
  for i=1,#balls do
    local b = balls[i]
    R(b.x-2, b.y-2, 4, 4, C.red)
    R(b.x-1, b.y-1, 2, 2, C.yellow)
  end

  draw_ship()

  -- pops
  for i=1,#pops do
    local p = pops[i]
    T(p.x-4, p.y-6, tostring(p.t), p.col)
  end

  if banner > 0 then
    R(60, 100, 120, 24, C.white)
    R(60, 100, 120, 2, C.ink)
    R(60, 122, 120, 2, C.ink)
    if state == 'play' then
      T(72, 108, 'WAVE '..wave, C.red)
    end
  end

  if state == 'pause' then
    R(50, 110, 140, 40, C.white)
    R(50, 110, 140, 2, C.ink)
    R(50, 148, 140, 2, C.ink)
    T(70, 124, 'PAUSE', C.ink)
    T(62, 138, 'OPTION = RESUME', C.dark)
  elseif state == 'over' then
    R(40, 100, 160, 56, C.white)
    R(40, 100, 160, 2, C.ink)
    R(40, 154, 160, 2, C.ink)
    T(58, 112, 'CLASS OVER', C.red)
    T(58, 128, 'SCORE '..score, C.ink)
    T(58, 142, 'START = AGAIN', C.dark)
  end

  T(6, H-12, 'START FIRE  UP/DN  OPTION', C.dark)
end
