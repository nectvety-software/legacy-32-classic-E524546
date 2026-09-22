
/*
  3D Engine for Blender OBJ Viewer
  Optimized for ESP32-S3 with PSRAM
*/

#ifndef ENGINE_3D_H
#define ENGINE_3D_H

#include <Arduino.h>
#include <math.h>

struct Vec3 {
  float x, y, z;
  Vec3(float x=0, float y=0, float z=0) : x(x), y(y), z(z) {}

  Vec3 operator+(const Vec3& v) const { return Vec3(x+v.x, y+v.y, z+v.z); }
  Vec3 operator-(const Vec3& v) const { return Vec3(x-v.x, y-v.y, z-v.z); }
  Vec3 operator*(float s) const { return Vec3(x*s, y*s, z*s); }

  float dot(const Vec3& v) const { return x*v.x + y*v.y + z*v.z; }
  Vec3 cross(const Vec3& v) const {
    return Vec3(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x);
  }

  float length() const { return sqrt(x*x + y*y + z*z); }
  Vec3 normalize() const {
    float len = length();
    if (len > 0) return Vec3(x/len, y/len, z/len);
    return Vec3(0, 0, 0);
  }
};

struct Mat4 {
  float m[4][4];

  Mat4(bool identity = true) {
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        m[i][j] = (identity && i == j) ? 1.0f : 0.0f;
  }

  static Mat4 rotationX(float angle) {
    Mat4 r(false);
    float c = cos(angle), s = sin(angle);
    r.m[0][0] = 1; r.m[1][1] = c; r.m[1][2] = -s;
    r.m[2][1] = s; r.m[2][2] = c; r.m[3][3] = 1;
    return r;
  }

  static Mat4 rotationY(float angle) {
    Mat4 r(false);
    float c = cos(angle), s = sin(angle);
    r.m[0][0] = c; r.m[0][2] = s;
    r.m[1][1] = 1;
    r.m[2][0] = -s; r.m[2][2] = c;
    r.m[3][3] = 1;
    return r;
  }

  static Mat4 rotationZ(float angle) {
    Mat4 r(false);
    float c = cos(angle), s = sin(angle);
    r.m[0][0] = c; r.m[0][1] = -s;
    r.m[1][0] = s; r.m[1][1] = c;
    r.m[2][2] = 1; r.m[3][3] = 1;
    return r;
  }

  static Mat4 translation(float x, float y, float z) {
    Mat4 r(true);
    r.m[3][0] = x; r.m[3][1] = y; r.m[3][2] = z;
    return r;
  }

  static Mat4 scale(float s) {
    Mat4 r(false);
    r.m[0][0] = r.m[1][1] = r.m[2][2] = s;
    r.m[3][3] = 1;
    return r;
  }

  Vec3 transform(const Vec3& v) const {
    float x = v.x*m[0][0] + v.y*m[1][0] + v.z*m[2][0] + m[3][0];
    float y = v.x*m[0][1] + v.y*m[1][1] + v.z*m[2][1] + m[3][1];
    float z = v.x*m[0][2] + v.y*m[1][2] + v.z*m[2][2] + m[3][2];
    float w = v.x*m[0][3] + v.y*m[1][3] + v.z*m[2][3] + m[3][3];
    if (w != 0) { x /= w; y /= w; z /= w; }
    return Vec3(x, y, z);
  }

  Mat4 operator*(const Mat4& o) const {
    Mat4 r(false);
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        for (int k = 0; k < 4; k++)
          r.m[i][j] += m[i][k] * o.m[k][j];
    return r;
  }
};

struct Camera {
  Vec3 position;
  Vec3 target;
  Vec3 up;
  float fov;
  float nearPlane;
  float farPlane;

  Camera() : position(0, 0, 5), target(0, 0, 0), up(0, 1, 0),
             fov(45), nearPlane(0.1f), farPlane(100.0f) {}

  Mat4 getViewMatrix() {
    Vec3 z = (position - target).normalize();
    Vec3 x = up.cross(z).normalize();
    Vec3 y = z.cross(x);

    Mat4 view(false);
    view.m[0][0] = x.x; view.m[0][1] = y.x; view.m[0][2] = z.x;
    view.m[1][0] = x.y; view.m[1][1] = y.y; view.m[1][2] = z.y;
    view.m[2][0] = x.z; view.m[2][1] = y.z; view.m[2][2] = z.z;
    view.m[3][0] = -x.dot(position);
    view.m[3][1] = -y.dot(position);
    view.m[3][2] = -z.dot(position);
    view.m[3][3] = 1;
    return view;
  }

  Mat4 getProjectionMatrix(float aspect) {
    float f = 1.0f / tan(fov * PI / 360.0f);
    Mat4 proj(false);
    proj.m[0][0] = f / aspect;
    proj.m[1][1] = f;
    proj.m[2][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
    proj.m[2][3] = -1;
    proj.m[3][2] = (2 * farPlane * nearPlane) / (nearPlane - farPlane);
    return proj;
  }

  void orbit(float dx, float dy) {
    Vec3 dir = position - target;
    float len = dir.length();
    float theta = atan2(dir.z, dir.x) + dx;
    float phi = acos(dir.y / len) + dy;
    phi = constrain(phi, 0.1f, PI - 0.1f);

    position.x = target.x + len * sin(phi) * cos(theta);
    position.y = target.y + len * cos(phi);
    position.z = target.z + len * sin(phi) * sin(theta);
  }

  void zoom(float delta) {
    Vec3 dir = position - target;
    float len = dir.length();
    len = constrain(len + delta, nearPlane, farPlane);
    dir = dir.normalize() * len;
    position = target + dir;
  }
};

struct Mesh {
  Vec3* vertices;
  int* indices;
  int vertexCount;
  int indexCount;

  Mesh() : vertices(nullptr), indices(nullptr), vertexCount(0), indexCount(0) {}

  ~Mesh() {
    if (vertices) free(vertices);
    if (indices) free(indices);
  }

  bool allocate(int vCount, int iCount) {
    vertexCount = vCount;
    indexCount = iCount;
    vertices = (Vec3*)ps_malloc(vCount * sizeof(Vec3));
    indices = (int*)ps_malloc(iCount * sizeof(int));
    return vertices && indices;
  }

  void calculateNormals(Vec3* normals) {
    // Initialize normals
    for (int i = 0; i < vertexCount; i++) {
      normals[i] = Vec3(0, 0, 0);
    }

    // Calculate face normals and accumulate
    for (int i = 0; i < indexCount; i += 3) {
      Vec3 v0 = vertices[indices[i]];
      Vec3 v1 = vertices[indices[i+1]];
      Vec3 v2 = vertices[indices[i+2]];

      Vec3 normal = (v1 - v0).cross(v2 - v0).normalize();

      normals[indices[i]] = normals[indices[i]] + normal;
      normals[indices[i+1]] = normals[indices[i+1]] + normal;
      normals[indices[i+2]] = normals[indices[i+2]] + normal;
    }

    // Normalize
    for (int i = 0; i < vertexCount; i++) {
      normals[i] = normals[i].normalize();
    }
  }
};

class Renderer3D {
private:
  int screenW, screenH;
  float* zBuffer;

public:
  Renderer3D(int w, int h) : screenW(w), screenH(h) {
    zBuffer = (float*)ps_malloc(w * h * sizeof(float));
  }

  ~Renderer3D() {
    if (zBuffer) free(zBuffer);
  }

  void clearZBuffer() {
    for (int i = 0; i < screenW * screenH; i++) {
      zBuffer[i] = 999999.0f;
    }
  }

  void renderWireframe(TFT_eSPI& tft, Mesh& mesh, Mat4& mvp, uint16_t color) {
    for (int i = 0; i < mesh.indexCount; i += 3) {
      Vec3 v0 = mvp.transform(mesh.vertices[mesh.indices[i]]);
      Vec3 v1 = mvp.transform(mesh.vertices[mesh.indices[i+1]]);
      Vec3 v2 = mvp.transform(mesh.vertices[mesh.indices[i+2]]);

      // Clip and project
      if (v0.z > -1 && v0.z < 1 && v1.z > -1 && v1.z < 1) {
        int x0 = (v0.x + 1) * 0.5f * screenW;
        int y0 = (1 - (v0.y + 1) * 0.5f) * screenH;
        int x1 = (v1.x + 1) * 0.5f * screenW;
        int y1 = (1 - (v1.y + 1) * 0.5f) * screenH;

        if (x0 >= 0 && x0 < screenW && y0 >= 0 && y0 < screenH &&
            x1 >= 0 && x1 < screenW && y1 >= 0 && y1 < screenH) {
          tft.drawLine(x0, y0, x1, y1, color);
        }
      }

      if (v1.z > -1 && v1.z < 1 && v2.z > -1 && v2.z < 1) {
        int x1 = (v1.x + 1) * 0.5f * screenW;
        int y1 = (1 - (v1.y + 1) * 0.5f) * screenH;
        int x2 = (v2.x + 1) * 0.5f * screenW;
        int y2 = (1 - (v2.y + 1) * 0.5f) * screenH;

        if (x1 >= 0 && x1 < screenW && y1 >= 0 && y1 < screenH &&
            x2 >= 0 && x2 < screenW && y2 >= 0 && y2 < screenH) {
          tft.drawLine(x1, y1, x2, y2, color);
        }
      }

      if (v2.z > -1 && v2.z < 1 && v0.z > -1 && v0.z < 1) {
        int x2 = (v2.x + 1) * 0.5f * screenW;
        int y2 = (1 - (v2.y + 1) * 0.5f) * screenH;
        int x0 = (v0.x + 1) * 0.5f * screenW;
        int y0 = (1 - (v0.y + 1) * 0.5f) * screenH;

        if (x2 >= 0 && x2 < screenW && y2 >= 0 && y2 < screenH &&
            x0 >= 0 && x0 < screenW && y0 >= 0 && y0 < screenH) {
          tft.drawLine(x2, y2, x0, y0, color);
        }
      }
    }
  }

  void renderPoints(TFT_eSPI& tft, Mesh& mesh, Mat4& mvp, uint16_t color, int size = 2) {
    for (int i = 0; i < mesh.vertexCount; i++) {
      Vec3 v = mvp.transform(mesh.vertices[i]);

      if (v.z > -1 && v.z < 1) {
        int x = (v.x + 1) * 0.5f * screenW;
        int y = (1 - (v.y + 1) * 0.5f) * screenH;

        if (x >= size && x < screenW - size && y >= size && y < screenH - size) {
          tft.fillRect(x - size/2, y - size/2, size, size, color);
        }
      }
    }
  }
};

#endif
