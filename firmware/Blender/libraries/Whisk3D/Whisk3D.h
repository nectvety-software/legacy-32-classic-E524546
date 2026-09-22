#ifndef Whisk3D_H
#define Whisk3D_H

#include <Arduino.h>

struct Vector3 {
    float x, y, z;
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    
    Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
    Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
    Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
    Vector3 operator/(float s) const { return Vector3(x / s, y / s, z / s); }
    
    float dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
    
    Vector3 cross(const Vector3& v) const {
        return Vector3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }
    
    float length() const { return sqrtf(x * x + y * y + z * z); }
    
    Vector3 normalize() const {
        float len = length();
        if (len > 0.0001f) {
            return Vector3(x / len, y / len, z / len);
        }
        return Vector3(0, 0, 0);
    }
};

struct Vertex {
    Vector3 position;
    uint16_t color;
};

class Matrix4 {
public:
    float m[16];
    
    Matrix4();
    
    static Matrix4 identity();
    static Matrix4 translation(float x, float y, float z);
    static Matrix4 rotationX(float angle);
    static Matrix4 rotationY(float angle);
    static Matrix4 rotationZ(float angle);
    static Matrix4 scale(float x, float y, float z);
    static Matrix4 perspective(float fov, float aspect, float near, float far);
    
    Matrix4 operator*(const Matrix4& other) const;
    Vector3 transform(const Vector3& v) const;
};

struct Object3D {
    Vertex* vertices;
    uint16_t* indices;
    int vertexCount;
    int indexCount;
    Matrix4 transform;
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    
    Object3D();
    ~Object3D();
    
    void updateTransform();
};

class Whisk3D {
public:
    Whisk3D(int width, int height);
    ~Whisk3D();
    
    void(*drawPixelCallback)(int16_t x, int16_t y, uint16_t color);
    void(*fillRectCallback)(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    void setDisplayCallbacks(
        void(*dp)(int16_t x, int16_t y, uint16_t color),
        void(*fr)(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    );
    
    void setCamera(Vector3 pos, Vector3 target, Vector3 up);
    void setProjection(float fov, float near, float far);
    
    void clear(uint16_t color);
    void drawPoint(Vector3 p, uint16_t color);
    void drawLine(Vector3 p1, Vector3 p2, uint16_t color);
    void drawObject(Object3D& obj);
    void drawCube(float size, uint16_t color);
    void drawPyramid(float size, float height, uint16_t color);
    void drawSphere(float radius, int segments, int rings, uint16_t color);
    void drawGrid(float size, int divisions, uint16_t color);
    
    void render();
    
private:
    int screenW, screenH;
    Vector3 camPos, camTarget, camUp;
    Matrix4 viewMatrix, projMatrix;
    float fov, nearPlane, farPlane;
    
    Vector3 worldToScreen(const Vector3& v);
    void bresenhamLine(int x0, int y0, int x1, int y1, uint16_t color);
    void drawTriangle(Vector3 v0, Vector3 v1, Vector3 v2, uint16_t color);
};

#endif
