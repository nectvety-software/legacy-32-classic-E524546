#include "Whisk3D.h"
#include <math.h>
#include <stdlib.h>

Matrix4::Matrix4() {
    for (int i = 0; i < 16; i++) m[i] = 0;
}

Matrix4 Matrix4::identity() {
    Matrix4 result;
    result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
    return result;
}

Matrix4 Matrix4::translation(float x, float y, float z) {
    Matrix4 result = identity();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

Matrix4 Matrix4::rotationX(float angle) {
    Matrix4 result = identity();
    float c = cosf(angle);
    float s = sinf(angle);
    result.m[5] = c;  result.m[6] = -s;
    result.m[9] = s;  result.m[10] = c;
    return result;
}

Matrix4 Matrix4::rotationY(float angle) {
    Matrix4 result = identity();
    float c = cosf(angle);
    float s = sinf(angle);
    result.m[0] = c;  result.m[8] = s;
    result.m[2] = -s; result.m[10] = c;
    return result;
}

Matrix4 Matrix4::rotationZ(float angle) {
    Matrix4 result = identity();
    float c = cosf(angle);
    float s = sinf(angle);
    result.m[0] = c;  result.m[1] = -s;
    result.m[4] = s;  result.m[5] = c;
    return result;
}

Matrix4 Matrix4::scale(float x, float y, float z) {
    Matrix4 result;
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    result.m[15] = 1.0f;
    return result;
}

Matrix4 Matrix4::perspective(float fov, float aspect, float near, float far) {
    Matrix4 result;
    float tanHalfFov = tanf(fov * 0.5f);
    result.m[0] = 1.0f / (aspect * tanHalfFov);
    result.m[5] = 1.0f / tanHalfFov;
    result.m[10] = -(far + near) / (far - near);
    result.m[11] = -1.0f;
    result.m[14] = -(2.0f * far * near) / (far - near);
    return result;
}

Matrix4 Matrix4::operator*(const Matrix4& other) const {
    Matrix4 result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                result.m[i * 4 + j] += m[i * 4 + k] * other.m[k * 4 + j];
            }
        }
    }
    return result;
}

Vector3 Matrix4::transform(const Vector3& v) const {
    Vector3 result;
    result.x = m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12];
    result.y = m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13];
    result.z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14];
    float w = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15];
    if (fabsf(w) > 0.0001f) {
        result.x /= w; result.y /= w; result.z /= w;
    }
    return result;
}

Object3D::Object3D() : vertices(nullptr), indices(nullptr), vertexCount(0), indexCount(0) {
    position = Vector3(0, 0, 0);
    rotation = Vector3(0, 0, 0);
    scale = Vector3(1, 1, 1);
}

Object3D::~Object3D() {
    if (vertices) free(vertices);
    if (indices) free(indices);
}

void Object3D::updateTransform() {
    transform = Matrix4::translation(position.x, position.y, position.z) *
                Matrix4::rotationY(rotation.y) *
                Matrix4::rotationX(rotation.x) *
                Matrix4::rotationZ(rotation.z) *
                Matrix4::scale(scale.x, scale.y, scale.z);
}

Whisk3D::Whisk3D(int width, int height) : screenW(width), screenH(height) {
    drawPixelCallback = nullptr;
    fillRectCallback = nullptr;
    camPos = Vector3(0, 0, 5);
    camTarget = Vector3(0, 0, 0);
    camUp = Vector3(0, 1, 0);
    fov = 60.0f * (3.14159f / 180.0f);
    nearPlane = 0.1f;
    farPlane = 100.0f;
}

Whisk3D::~Whisk3D() {}

void Whisk3D::setDisplayCallbacks(
    void(*dp)(int16_t x, int16_t y, uint16_t color),
    void(*fr)(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
) {
    drawPixelCallback = dp;
    fillRectCallback = fr;
}

void Whisk3D::setCamera(Vector3 pos, Vector3 target, Vector3 up) {
    camPos = pos;
    camTarget = target;
    camUp = up;
    
    Vector3 z = (pos - target).normalize();
    Vector3 x = up.cross(z).normalize();
    Vector3 y = z.cross(x);
    
    viewMatrix = Matrix4::identity();
    viewMatrix.m[0] = x.x; viewMatrix.m[4] = x.y; viewMatrix.m[8] = x.z;
    viewMatrix.m[1] = y.x; viewMatrix.m[5] = y.y; viewMatrix.m[9] = y.z;
    viewMatrix.m[2] = z.x; viewMatrix.m[6] = z.y; viewMatrix.m[10] = z.z;
    
    viewMatrix.m[12] = -x.dot(pos);
    viewMatrix.m[13] = -y.dot(pos);
    viewMatrix.m[14] = -z.dot(pos);
}

void Whisk3D::setProjection(float fovDeg, float near, float far) {
    fov = fovDeg * (3.14159f / 180.0f);
    nearPlane = near;
    farPlane = far;
    float aspect = (float)screenW / (float)screenH;
    projMatrix = Matrix4::perspective(fov, aspect, nearPlane, farPlane);
}

void Whisk3D::clear(uint16_t color) {
    if (fillRectCallback) {
        fillRectCallback(0, 0, screenW, screenH, color);
    }
}

Vector3 Whisk3D::worldToScreen(const Vector3& v) {
    Matrix4 MVP = projMatrix * viewMatrix;
    Vector3 clip = MVP.transform(v);
    
    if (clip.z < nearPlane || clip.z > farPlane) {
        return Vector3(-1000, -1000, -1000);
    }
    
    float x = (clip.x + 1.0f) * 0.5f * screenW;
    float y = (1.0f - clip.y) * 0.5f * screenH;
    
    return Vector3(x, y, clip.z);
}

void Whisk3D::bresenhamLine(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        if (x0 >= 0 && x0 < screenW && y0 >= 0 && y0 < screenH) {
            if (drawPixelCallback) drawPixelCallback(x0, y0, color);
        }
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void Whisk3D::drawPoint(Vector3 p, uint16_t color) {
    Vector3 screen = worldToScreen(p);
    if (screen.x >= 0 && screen.x < screenW && screen.y >= 0 && screen.y < screenH) {
        if (drawPixelCallback) drawPixelCallback((int16_t)screen.x, (int16_t)screen.y, color);
    }
}

void Whisk3D::drawLine(Vector3 p1, Vector3 p2, uint16_t color) {
    Vector3 s1 = worldToScreen(p1);
    Vector3 s2 = worldToScreen(p2);
    if (s1.z > nearPlane && s1.z < farPlane && s2.z > nearPlane && s2.z < farPlane) {
        bresenhamLine((int)s1.x, (int)s1.y, (int)s2.x, (int)s2.y, color);
    }
}

void Whisk3D::drawObject(Object3D& obj) {
    obj.updateTransform();
    Matrix4 MVP = projMatrix * viewMatrix * obj.transform;
    
    Vector3 transformed[8];
    for (int i = 0; i < obj.vertexCount; i++) {
        transformed[i] = MVP.transform(obj.vertices[i].position);
    }
    
    for (int i = 0; i < obj.indexCount; i += 3) {
        int i0 = obj.indices[i];
        int i1 = obj.indices[i + 1];
        int i2 = obj.indices[i + 2];
        
        Vector3 s0 = worldToScreen(transformed[i0]);
        Vector3 s1 = worldToScreen(transformed[i1]);
        Vector3 s2 = worldToScreen(transformed[i2]);
        
        if (s0.z > nearPlane && s0.z < farPlane &&
            s1.z > nearPlane && s1.z < farPlane &&
            s2.z > nearPlane && s2.z < farPlane) {
            uint16_t col = obj.vertices[i0].color;
            bresenhamLine((int)s0.x, (int)s0.y, (int)s1.x, (int)s1.y, col);
            bresenhamLine((int)s1.x, (int)s1.y, (int)s2.x, (int)s2.y, col);
            bresenhamLine((int)s2.x, (int)s2.y, (int)s0.x, (int)s0.y, col);
        }
    }
}

void Whisk3D::drawCube(float size, uint16_t color) {
    float h = size / 2.0f;
    Vertex verts[8] = {
        {Vector3(-h, -h, -h), color}, {Vector3(h, -h, -h), color},
        {Vector3(h, h, -h), color}, {Vector3(-h, h, -h), color},
        {Vector3(-h, -h, h), color}, {Vector3(h, -h, h), color},
        {Vector3(h, h, h), color}, {Vector3(-h, h, h), color}
    };
    
    uint16_t inds[] = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        2, 6, 7, 2, 7, 3,
        0, 3, 7, 0, 7, 4,
        1, 5, 6, 1, 6, 2
    };
    
    Object3D cube;
    cube.vertices = verts;
    cube.vertexCount = 8;
    cube.indices = inds;
    cube.indexCount = 36;
    drawObject(cube);
}

void Whisk3D::drawPyramid(float size, float height, uint16_t color) {
    float h = size / 2.0f;
    Vertex verts[5] = {
        {Vector3(0, height, 0), color},
        {Vector3(-h, 0, -h), color}, {Vector3(h, 0, -h), color},
        {Vector3(h, 0, h), color}, {Vector3(-h, 0, h), color}
    };
    
    uint16_t inds[] = {
        0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
        1, 4, 3, 1, 3, 2
    };
    
    Object3D pyr;
    pyr.vertices = verts;
    pyr.vertexCount = 5;
    pyr.indices = inds;
    pyr.indexCount = 18;
    drawObject(pyr);
}

void Whisk3D::drawSphere(float radius, int segments, int rings, uint16_t color) {
    Object3D sphere;
    sphere.vertexCount = (segments + 1) * (rings + 1);
    sphere.vertices = (Vertex*)malloc(sizeof(Vertex) * sphere.vertexCount);
    
    int idx = 0;
    for (int ring = 0; ring <= rings; ring++) {
        float phi = 3.14159f * ring / rings;
        for (int seg = 0; seg <= segments; seg++) {
            float theta = 2.0f * 3.14159f * seg / segments;
            float x = radius * sinf(phi) * cosf(theta);
            float y = radius * cosf(phi);
            float z = radius * sinf(phi) * sinf(theta);
            sphere.vertices[idx++].position = Vector3(x, y, z);
        }
    }
    
    sphere.indexCount = segments * rings * 6;
    sphere.indices = (uint16_t*)malloc(sizeof(uint16_t) * sphere.indexCount);
    idx = 0;
    for (int ring = 0; ring < rings; ring++) {
        for (int seg = 0; seg < segments; seg++) {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;
            sphere.indices[idx++] = current;
            sphere.indices[idx++] = next;
            sphere.indices[idx++] = current + 1;
            sphere.indices[idx++] = current + 1;
            sphere.indices[idx++] = next;
            sphere.indices[idx++] = next + 1;
        }
    }
    
    drawObject(sphere);
    free(sphere.vertices);
    free(sphere.indices);
}

void Whisk3D::drawGrid(float size, int divisions, uint16_t color) {
    float step = size / divisions;
    float half = size / 2.0f;
    
    for (int i = 0; i <= divisions; i++) {
        float pos = -half + i * step;
        drawLine(Vector3(pos, 0, -half), Vector3(pos, 0, half), color);
        drawLine(Vector3(-half, 0, pos), Vector3(half, 0, pos), color);
    }
}

void Whisk3D::render() {
    setProjection(fov * 180.0f / 3.14159f, nearPlane, farPlane);
}
