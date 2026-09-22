-- PoChiTaOS Lua demo: for loops + hardware buttons + graphics
-- Put on SD, open from the File Manager (runs as a graphics app).
-- Buttons: OPTION (pin 18) to quit, B/MENU to exit the OS screen after.

cls()

-- 1) for loop: draw a growing rainbow of bars
for i = 1, 8 do
    fillRect(20, i * 20, i * 24, 10, color(i * 30, i * 10, 255 - i * 20))
    sleep(80)
end

text(20, 190, "Move with d-pad, A=clear", color(255, 255, 0))
text(20, 205, "OPTION exits", color(255, 255, 0))

-- 2) interactive: pixel trail controlled by the real board buttons
x = 120
y = 160
running = true
while running do
    if btn(7) then y = y - 2 end       -- KEY_UP   (pin 7)
    if btn(46) then y = y + 2 end      -- KEY_DOWN (pin 46)
    if btn(45) then x = x - 2 end      -- KEY_LEFT (pin 45)
    if btn(6) then x = x + 2 end       -- KEY_RIGHT(pin 6)
    if btn(8) then                     -- KEY_A    (pin 8)
        cls()
        text(20, 190, "Move with d-pad, A=clear", color(255, 255, 0))
        text(20, 205, "OPTION exits", color(255, 255, 0))
    end
    if btn(18) then running = false end -- KEY_OPTION (pin 18)
    pixel(x, y, color(255, 200, 0))
    sleep(20)
end
