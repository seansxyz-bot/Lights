#include "renderer.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
constexpr float PI = 3.14159265358979323846f;

float deg_to_rad(float degrees) { return degrees * (PI / 180.0f); }

Renderer::Vec3 normalize(const Renderer::Vec3 &v) {
  const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  if (len <= 0.000001f)
    return {0.0f, 0.0f, 0.0f};

  return {v.x / len, v.y / len, v.z / len};
}

Renderer::Vec3 subtract(const Renderer::Vec3 &a, const Renderer::Vec3 &b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Renderer::Vec3 cross(const Renderer::Vec3 &a, const Renderer::Vec3 &b) {
  return {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

float dot(const Renderer::Vec3 &a, const Renderer::Vec3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
} // namespace

Renderer::Renderer() {
  m_view = Mat4::identity();
  m_proj = Mat4::identity();
}

Renderer::~Renderer() { shutdown(); }

Renderer::Mat4 Renderer::Mat4::identity() {
  Mat4 out;
  out.m = {
      1.0f, 0.0f, 0.0f, 0.0f, //
      0.0f, 1.0f, 0.0f, 0.0f, //
      0.0f, 0.0f, 1.0f, 0.0f, //
      0.0f, 0.0f, 0.0f, 1.0f  //
  };
  return out;
}

Renderer::Mat4 Renderer::Mat4::perspective(float fovDegrees, float aspect,
                                           float zNear, float zFar) {
  Mat4 out{};
  const float f = 1.0f / std::tan(deg_to_rad(fovDegrees) * 0.5f);

  out.m = {
      f / aspect,
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      f,
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      (zFar + zNear) / (zNear - zFar),
      -1.0f,
      0.0f,
      0.0f,
      (2.0f * zFar * zNear) / (zNear - zFar),
      0.0f,
  };

  return out;
}

Renderer::Mat4 Renderer::Mat4::translation(float x, float y, float z) {
  Mat4 out = identity();
  out.m[12] = x;
  out.m[13] = y;
  out.m[14] = z;
  return out;
}

Renderer::Mat4 Renderer::Mat4::scale(float x, float y, float z) {
  Mat4 out{};
  out.m = {
      x,    0.0f, 0.0f, 0.0f, 0.0f, y,    0.0f, 0.0f,
      0.0f, 0.0f, z,    0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  return out;
}

Renderer::Mat4 Renderer::Mat4::rotationX(float degrees) {
  Mat4 out = identity();
  const float r = deg_to_rad(degrees);
  const float c = std::cos(r);
  const float s = std::sin(r);

  out.m[5] = c;
  out.m[6] = s;
  out.m[9] = -s;
  out.m[10] = c;

  return out;
}

Renderer::Mat4 Renderer::Mat4::rotationY(float degrees) {
  Mat4 out = identity();
  const float r = deg_to_rad(degrees);
  const float c = std::cos(r);
  const float s = std::sin(r);

  out.m[0] = c;
  out.m[2] = -s;
  out.m[8] = s;
  out.m[10] = c;

  return out;
}

Renderer::Mat4 Renderer::Mat4::rotationZ(float degrees) {
  Mat4 out = identity();
  const float r = deg_to_rad(degrees);
  const float c = std::cos(r);
  const float s = std::sin(r);

  out.m[0] = c;
  out.m[1] = s;
  out.m[4] = -s;
  out.m[5] = c;

  return out;
}

Renderer::Mat4 Renderer::Mat4::lookAt(const Vec3 &eye, const Vec3 &target,
                                      const Vec3 &up) {
  const Vec3 f = normalize(subtract(target, eye));
  const Vec3 s = normalize(cross(f, up));
  const Vec3 u = cross(s, f);

  Mat4 out = identity();

  out.m[0] = s.x;
  out.m[1] = u.x;
  out.m[2] = -f.x;
  out.m[3] = 0.0f;

  out.m[4] = s.y;
  out.m[5] = u.y;
  out.m[6] = -f.y;
  out.m[7] = 0.0f;

  out.m[8] = s.z;
  out.m[9] = u.z;
  out.m[10] = -f.z;
  out.m[11] = 0.0f;

  out.m[12] = -dot(s, eye);
  out.m[13] = -dot(u, eye);
  out.m[14] = dot(f, eye);
  out.m[15] = 1.0f;

  return out;
}

Renderer::Mat4 Renderer::Mat4::operator*(const Mat4 &rhs) const {
  Mat4 out{};

  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      out.m[col * 4 + row] = m[0 * 4 + row] * rhs.m[col * 4 + 0] +
                             m[1 * 4 + row] * rhs.m[col * 4 + 1] +
                             m[2 * 4 + row] * rhs.m[col * 4 + 2] +
                             m[3 * 4 + row] * rhs.m[col * 4 + 3];
    }
  }

  return out;
}

bool Renderer::initialize() {
  if (m_ready)
    return true;

  try {
    if (!build_basic_program())
      return false;

    create_cube_geometry();
    create_plane_geometry();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    m_ready = true;
    std::cout << "Renderer initialized." << std::endl;
    return true;
  } catch (const std::exception &ex) {
    std::cerr << "Renderer initialize failed: " << ex.what() << std::endl;
    shutdown();
    return false;
  }
}

void Renderer::shutdown() {
  destroy_geometry();

  if (m_basicProgram != 0) {
    glDeleteProgram(m_basicProgram);
    m_basicProgram = 0;
  }

  m_uModel = -1;
  m_uView = -1;
  m_uProj = -1;
  m_uColor = -1;
  m_aPosition = -1;

  m_ready = false;
}

void Renderer::begin_frame(int width, int height, float clearR, float clearG,
                           float clearB, float clearA) {
  glViewport(0, 0, width, height);
  glClearColor(clearR, clearG, clearB, clearA);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::set_camera(const Vec3 &eye, const Vec3 &target, const Vec3 &up,
                          float fovDegrees, float aspect, float zNear,
                          float zFar) {
  m_view = Mat4::lookAt(eye, target, up);
  m_proj = Mat4::perspective(fovDegrees, aspect, zNear, zFar);
}

void Renderer::draw_cube(const Mat4 &model, const Color &color) {
  if (!m_ready || m_cubeVbo == 0)
    return;

  use_basic_program(model, color);

  glBindBuffer(GL_ARRAY_BUFFER, m_cubeVbo);
  glEnableVertexAttribArray(static_cast<GLuint>(m_aPosition));
  glVertexAttribPointer(static_cast<GLuint>(m_aPosition), 3, GL_FLOAT, GL_FALSE,
                        sizeof(float) * 3, nullptr);

  glDrawArrays(GL_TRIANGLES, 0, m_cubeVertexCount);

  glDisableVertexAttribArray(static_cast<GLuint>(m_aPosition));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::draw_plane_xy(const Mat4 &model, const Color &color) {
  if (!m_ready || m_planeVbo == 0)
    return;

  use_basic_program(model, color);

  glBindBuffer(GL_ARRAY_BUFFER, m_planeVbo);
  glEnableVertexAttribArray(static_cast<GLuint>(m_aPosition));
  glVertexAttribPointer(static_cast<GLuint>(m_aPosition), 3, GL_FLOAT, GL_FALSE,
                        sizeof(float) * 3, nullptr);

  glDrawArrays(GL_TRIANGLES, 0, m_planeVertexCount);

  glDisableVertexAttribArray(static_cast<GLuint>(m_aPosition));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::draw_plane_xz(const Mat4 &model, const Color &color) {
  const Mat4 rotate = Mat4::rotationX(-90.0f);
  draw_plane_xy(model * rotate, color);
}

bool Renderer::is_ready() const { return m_ready; }

bool Renderer::build_basic_program() {
  const std::string vsSource = R"(
attribute vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main() {
  gl_Position = uProj * uView * uModel * vec4(aPosition, 1.0);
}
)";

  const std::string fsSource = R"(
#ifdef GL_ES
precision mediump float;
#endif

uniform vec4 uColor;

void main() {
  gl_FragColor = uColor;
}
)";

  GLuint vs = compile_shader(GL_VERTEX_SHADER, vsSource);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsSource);

  m_basicProgram = link_program(vs, fs);

  glDeleteShader(vs);
  glDeleteShader(fs);

  m_aPosition = glGetAttribLocation(m_basicProgram, "aPosition");
  m_uModel = glGetUniformLocation(m_basicProgram, "uModel");
  m_uView = glGetUniformLocation(m_basicProgram, "uView");
  m_uProj = glGetUniformLocation(m_basicProgram, "uProj");
  m_uColor = glGetUniformLocation(m_basicProgram, "uColor");

  if (m_aPosition < 0 || m_uModel < 0 || m_uView < 0 || m_uProj < 0 ||
      m_uColor < 0) {
    throw std::runtime_error("Renderer shader locations not found.");
  }

  return true;
}

GLuint Renderer::compile_shader(GLenum type, const std::string &source) {
  GLuint shader = glCreateShader(type);
  if (shader == 0)
    throw std::runtime_error("glCreateShader failed.");

  const char *src = source.c_str();
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint status = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

  if (status != GL_TRUE) {
    GLint logLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);

    std::string log;
    if (logLen > 0) {
      log.resize(static_cast<std::size_t>(logLen));
      glGetShaderInfoLog(shader, logLen, nullptr, &log[0]);
    }

    glDeleteShader(shader);
    throw std::runtime_error("Shader compile failed:\n" + log);
  }

  return shader;
}

GLuint Renderer::link_program(GLuint vs, GLuint fs) {
  GLuint program = glCreateProgram();
  if (program == 0)
    throw std::runtime_error("glCreateProgram failed.");

  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);

  GLint status = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &status);

  if (status != GL_TRUE) {
    GLint logLen = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);

    std::string log;
    if (logLen > 0) {
      log.resize(static_cast<std::size_t>(logLen));
      glGetProgramInfoLog(program, logLen, nullptr, &log[0]);
    }

    glDeleteProgram(program);
    throw std::runtime_error("Program link failed:\n" + log);
  }

  return program;
}

void Renderer::create_cube_geometry() {
  const float cubeVerts[] = {
      -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,
      -0.5f, -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,

      -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f,
      -0.5f, -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f,

      -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,
      -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f,

      0.5f,  -0.5f, -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,
      0.5f,  -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,

      -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
      -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  -0.5f,

      -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,
      -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,
  };

  glGenBuffers(1, &m_cubeVbo);
  glBindBuffer(GL_ARRAY_BUFFER, m_cubeVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  m_cubeVertexCount = 36;
}

void Renderer::create_plane_geometry() {
  const float planeVerts[] = {
      -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.5f,  0.5f, 0.0f,

      -0.5f, -0.5f, 0.0f, 0.5f, 0.5f,  0.0f, -0.5f, 0.5f, 0.0f,
  };

  glGenBuffers(1, &m_planeVbo);
  glBindBuffer(GL_ARRAY_BUFFER, m_planeVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(planeVerts), planeVerts, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  m_planeVertexCount = 6;
}

void Renderer::destroy_geometry() {
  if (m_cubeVbo != 0) {
    glDeleteBuffers(1, &m_cubeVbo);
    m_cubeVbo = 0;
  }

  if (m_planeVbo != 0) {
    glDeleteBuffers(1, &m_planeVbo);
    m_planeVbo = 0;
  }

  m_cubeVertexCount = 0;
  m_planeVertexCount = 0;
}

void Renderer::use_basic_program(const Mat4 &model, const Color &color) {
  glUseProgram(m_basicProgram);

  glUniformMatrix4fv(m_uModel, 1, GL_FALSE, model.m.data());
  glUniformMatrix4fv(m_uView, 1, GL_FALSE, m_view.m.data());
  glUniformMatrix4fv(m_uProj, 1, GL_FALSE, m_proj.m.data());

  glUniform4f(m_uColor, color.r, color.g, color.b, color.a);
}
