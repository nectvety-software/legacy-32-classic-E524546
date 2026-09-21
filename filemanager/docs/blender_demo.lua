-- Blender 3D Demo
-- Rotating cube with 3D projection

-- 3D vertices of a cube centered at origin
verts = {
  {-30,-30,-30}, {30,-30,-30}, {30,30,-30}, {-30,30,-30},
  {-30,-30,30}, {30,-30,30}, {30,30,30}, {-30,30,30}
}

-- Edges connecting vertices
edges = {
  {1,2},{2,3},{3,4},{4,1},
  {5,6},{6,7},{7,8},{8,5},
  {1,5},{2,6},{3,7},{4,8}
}

screen.clear()

-- Draw text
screen.color(0, 0, 180)
screen.text(10, 5, "Blender 3D Cube")

angle = 0

-- Simple 3D to 2D projection
function project(vx, vy, vz)
  fov = 100
  scale = 2
  z = vz + fov
  sx = 112 + (vx * scale / z)
  sy = 130 + (vy * scale / z)
  return sx, sy
end

-- Rotate around Y axis
function rotY(vx, vy, vz, a)
  ca = math.cos(a)
  sa = math.sin(a)
  nx = vx * ca + vz * sa
  nz = -vx * sa + vz * ca
  return nx, vy, nz
end

-- Rotate around X axis
function rotX(vx, vy, vz, a)
  ca = math.cos(a)
  sa = math.sin(a)
  ny = vy * ca - vz * sa
  nz = vy * sa + vz * ca
  return vx, ny, nz
end

-- Draw a line in 3D
function drawLine3d(i1, i2, a)
  v1 = verts[i1]
  v2 = verts[i2]
  
  x1, y1, z1 = rotY(v1[1], v1[2], v1[3], a)
  x1, y1, z1 = rotX(x1, y1, z1, a * 0.7)
  
  x2, y2, z2 = rotY(v2[1], v2[2], v2[3], a)
  x2, y2, z2 = rotX(x2, y2, z2, a * 0.7)
  
  sx1, sy1 = project(x1, y1, z1)
  sx2, sy2 = project(x2, y2, z2)
  
  screen.color(0, 0, 200)
  screen.line(sx1, sy1, sx2, sy2)
end

-- Draw filled triangle
function drawTri(v1, v2, v3, col)
  screen.color(col[1], col[2], col[3])
  screen.fill(v1[1], v1[2], v3[1], v3[2])
end

screen.color(0, 0, 0)
screen.text(5, 220, "Press SELECT to exit")

-- Animate
for f = 1, 30 do
  angle = angle + 0.2
  
  -- Draw cube edges
  for i = 1, 12 do
    e = edges[i]
    drawLine3d(e[1], e[2], angle)
  end
  
  screen.color(255, 255, 255)
end

screen.color(0, 150, 0)
screen.text(80, 240, "Done!")
