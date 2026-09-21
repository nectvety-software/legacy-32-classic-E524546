-- src/player.lua — ví dụ module require() kiểu LuaS30 (resource-based)
local Player = {}
Player.x, Player.y = 112, 150
function Player.move(dx, dy, dt)
  Player.x = Player.x + dx * 90 * (dt or 0.033)
  Player.y = Player.y + dy * 90 * (dt or 0.033)
end
return Player
