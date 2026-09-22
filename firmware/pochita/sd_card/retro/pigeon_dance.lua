-- Pigeon Dance Game for PochitaOS
-- Use arrow keys or buttons to dance!

print("PIGEON DANCE!")
print("Press buttons to dance!")

x = 120
y = 100
frame = 0
score = 0
dancing = false
direction = 0

function drawPigeon(cx, cy, f)
    -- Body
    print("   ___   ")
    print("  (o o)  ")
    print("  ( > )  ")
    print("   ---   ")
end

while true do
    print("PIGEON DANCE!")
    print("Score: " .. score)
    print("-----------")
    
    if frame % 4 == 0 then
        print("  >o<  ")
    elseif frame % 4 == 1 then
        print("  <o>  ")
    elseif frame % 4 == 2 then
        print("  >o<  ")
    else
        print("  <o>  ")
    end
    
    print(" /| |\\")
    print("  | |  ")
    print("  _'_  ")
    
    print("-----------")
    print("A: Jump!")
    print("B: Exit")
    
    score = score + 1
    frame = frame + 1
    sleep(500)
    
    if score > 10 then
        print("GREAT!")
    end
    if score > 20 then
        print("AMAZING!")
    end
    if score > 30 then
        print("PERFECT!")
    end
end

print("Game Over!")
print("Final Score: " .. score)
