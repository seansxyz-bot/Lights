#pragma once

#include <epoxy/gl.h>

#include <array>
#include <string>

class Renderer {
public:
  struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
  };

  struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
  };

  struct Mat4 {
    std::array<float, 16> m{};

    static Mat4 identity();
    static Mat4 perspective(float fovDegrees, float aspect, float zNear,
                            float zFar);
    static Mat4 translation(float x, float y, float z);
    static Mat4 scale(float x, float y, float z);
    static Mat4 rotationX(float degrees);
    static Mat4 rotationY(float degrees);
    static Mat4 rotationZ(float degrees);
    static Mat4 lookAt(const Vec3 &eye, const Vec3 &target, const Vec3 &up);

    Mat4 operator*(const Mat4 &rhs) const;
  };

public:
  Renderer();
  ~Renderer();

  bool initialize();
  void shutdown();

  void begin_frame(int width, int height, float clearR, float clearG,
                   float clearB, float clearA);

  void set_camera(const Vec3 &eye, const Vec3 &target, const Vec3 &up,
                  float fovDegrees, float aspect, float zNear, float zFar);

  void draw_cube(const Mat4 &model, const Color &color);
  void draw_plane_xy(const Mat4 &model, const Color &color);
  void draw_plane_xz(const Mat4 &model, const Color &color);

  bool is_ready() const;

private:
  bool build_basic_program();
  GLuint compile_shader(GLenum type, const std::string &source);
  GLuint link_program(GLuint vs, GLuint fs);

  void create_cube_geometry();
  void create_plane_geometry();
  void destroy_geometry();

  void use_basic_program(const Mat4 &model, const Color &color);

private:
  bool m_ready = false;

  GLuint m_basicProgram = 0;

  GLint m_uModel = -1;
  GLint m_uView = -1;
  GLint m_uProj = -1;
  GLint m_uColor = -1;

  GLint m_aPosition = -1;

  GLuint m_cubeVbo = 0;
  GLuint m_planeVbo = 0;

  GLsizei m_cubeVertexCount = 0;
  GLsizei m_planeVertexCount = 0;

  Mat4 m_view;
  Mat4 m_proj;
};
