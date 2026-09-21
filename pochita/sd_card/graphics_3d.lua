-- PoChiTaOS Lua 3D graphics demo
-- Uses built-in functions: cls(), color(r,g,b), line(x1,y1,x2,y2,c), sin(), cos(), sleep(ms)

function project(x,y,z)
    s = 140 / (z + 220)
    px = x * s + 120
    py = y * s + 160
end

function rotateY(x,z,a)
    rx = x * cos(a) - z * sin(a)
    rz = x * sin(a) + z * cos(a)
end

function rotateX(y,z,a)
    ry = y * cos(a) - z * sin(a)
    rz = y * sin(a) + z * cos(a)
end

function drawEdge(x1,y1,z1,x2,y2,z2,col)
    rotateY(x1, z1, angle)
    xA = rx
    zA = rz
    rotateX(y1, zA, angle * 0.6)
    yA = ry
    zA = rz
    project(xA, yA, zA)
    sx1 = px
    sy1 = py

    rotateY(x2, z2, angle)
    xB = rx
    zB = rz
    rotateX(y2, zB, angle * 0.6)
    yB = ry
    zB = rz
    project(xB, yB, zB)
    line(sx1, sy1, px, py, col)
end

angle = 0
while angle < 6.28 do
    cls()
    c1 = color(255, 120, 60)
    c2 = color(100, 200, 255)
    c3 = color(255, 255, 100)

    drawEdge(-30, -30, -30, 30, -30, -30, c1)
    drawEdge(30, -30, -30, 30, 30, -30, c1)
    drawEdge(30, 30, -30, -30, 30, -30, c1)
    drawEdge(-30, 30, -30, -30, -30, -30, c1)

    drawEdge(-30, -30, 30, 30, -30, 30, c2)
    drawEdge(30, -30, 30, 30, 30, 30, c2)
    drawEdge(30, 30, 30, -30, 30, 30, c2)
    drawEdge(-30, 30, 30, -30, -30, 30, c2)

    drawEdge(-30, -30, -30, -30, -30, 30, c3)
    drawEdge(30, -30, -30, 30, -30, 30, c3)
    drawEdge(30, 30, -30, 30, 30, 30, c3)
    drawEdge(-30, 30, -30, -30, 30, 30, c3)

    sleep(80)
    angle = angle + 0.26
end

print("3D demo finished")
