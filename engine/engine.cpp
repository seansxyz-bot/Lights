#include "engine.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
constexpr int FRAME_MS = 16;
}

Engine::Engine(const TeamRecord &teamRecord, const TeamStats &teamStats,
               const GameInfo &gameInfo)
    : m_teamRecord(teamRecord), m_teamStats(teamStats), m_gameInfo(gameInfo) {
  set_hexpand(true);
  set_vexpand(true);

  set_has_depth_buffer(true);
  set_has_stencil_buffer(false);
  set_auto_render(false);

  // ES-style path.
  set_use_es(true);

  m_animationType = m_teamRecord.nextGameParser;
  m_animationPath = m_teamRecord.nextGameUrlTemplate;
  loadAnimationFile(m_animationPath);

  reset_timing();
}

Engine::~Engine() { stop(); }

void Engine::start() {
  if (m_tickConn.connected())
    return;

  reset_timing();

  m_tickConn = Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &Engine::on_timeout), FRAME_MS);
}

void Engine::stop() {
  if (m_tickConn.connected())
    m_tickConn.disconnect();
}

void Engine::setTeamRecord(const TeamRecord &teamRecord) {
  m_teamRecord = teamRecord;
  queue_render();
}

void Engine::setTeamStats(const TeamStats &teamStats) {
  m_teamStats = teamStats;
  queue_render();
}

void Engine::setGameInfo(const GameInfo &gameInfo) {
  m_gameInfo = gameInfo;
  queue_render();
}

void Engine::reset_timing() {
  m_lastTick = std::chrono::steady_clock::now();
  m_startTime = m_lastTick;
  m_elapsedSeconds = 0.0;
}

void Engine::on_realize() {
  Gtk::GLArea::on_realize();

  try {
    make_current();

    // if (get_error())
    //     throw std::runtime_error("Gtk::GLArea context error.");

    init_gl();
    m_glReady = true;
  } catch (const std::exception &ex) {
    std::cerr << "Engine on_realize failed: " << ex.what() << std::endl;
    m_glReady = false;
  }
}

void Engine::on_unrealize() {
  make_current();

  if (m_glReady)
    shutdown_gl();

  m_glReady = false;

  Gtk::GLArea::on_unrealize();
}

bool Engine::on_timeout() {
  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> dt = now - m_lastTick;
  m_lastTick = now;

  update(dt.count());
  queue_render();
  return true;
}

void Engine::update(double dt) {
  m_elapsedSeconds += dt;

  m_cameraY = 1.2f + std::sin(m_elapsedSeconds * 2.4) * 0.20f;
}

bool Engine::on_render(const Glib::RefPtr<Gdk::GLContext> & /*context*/) {
  if (!m_glReady)
    return true;

  auto alloc = get_allocation();
  render_frame(alloc.get_width(), alloc.get_height());
  return true;
}

void Engine::init_gl() {
  if (!m_renderer.initialize())
    throw std::runtime_error("Renderer initialization failed.");

  std::cout << "Engine GL initialized." << std::endl;
}

void Engine::shutdown_gl() {
  m_renderer.shutdown();
  std::cout << "Engine GL shutdown." << std::endl;
}

void Engine::render_frame(int width, int height) {
  const float pulse =
      0.55f + 0.45f * std::sin(static_cast<float>(m_elapsedSeconds) * 7.0f);

  const float clearR = 0.04f + m_secondaryColor.r * 0.18f * pulse;
  const float clearG = 0.04f + m_secondaryColor.g * 0.18f * pulse;
  const float clearB = 0.06f + m_secondaryColor.b * 0.18f * pulse;

  m_renderer.begin_frame(width, height, clearR, clearG, clearB, 1.0f);

  const float aspect =
      (height > 0) ? static_cast<float>(width) / static_cast<float>(height)
                   : 1.0f;

  Renderer::Vec3 eye{m_cameraX, m_cameraY, m_cameraZ};
  Renderer::Vec3 target{0.0f, 0.0f, 0.0f};
  Renderer::Vec3 up{0.0f, 1.0f, 0.0f};

  m_renderer.set_camera(eye, target, up, 60.0f, aspect, 0.1f, 100.0f);

  const float spin = static_cast<float>(m_elapsedSeconds * 95.0);
  const float scale = 1.45f + pulse * 0.65f;

  Renderer::Mat4 cubeModel = Renderer::Mat4::translation(0.0f, 0.0f, 0.0f) *
                             Renderer::Mat4::rotationY(spin) *
                             Renderer::Mat4::rotationX(spin * 0.6f) *
                             Renderer::Mat4::scale(scale, scale, scale);

  Renderer::Color cubeColor{
      std::min(1.0f, m_primaryColor.r * (0.55f + pulse)),
      std::min(1.0f, m_primaryColor.g * (0.55f + pulse)),
      std::min(1.0f, m_primaryColor.b * (0.55f + pulse)),
      1.0f,
  };

  m_renderer.draw_cube(cubeModel, cubeColor);

  Renderer::Mat4 floorModel = Renderer::Mat4::translation(0.0f, -2.2f, 0.0f) *
                              Renderer::Mat4::scale(10.0f, 10.0f, 1.0f);

  Renderer::Color floorColor{
      m_secondaryColor.r,
      m_secondaryColor.g,
      m_secondaryColor.b,
      1.0f,
  };

  m_renderer.draw_plane_xz(floorModel, floorColor);
}

void Engine::loadAnimationFile(const std::string &path) {
  if (path.empty())
    return;

  std::ifstream in(path);
  if (!in.is_open())
    return;

  std::string line;
  while (std::getline(in, line)) {
    const auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;

    const std::string key = line.substr(0, eq);
    std::string value = line.substr(eq + 1);

    if (key != "color" && key != "primary")
      continue;

    if (!value.empty() && value[0] == '#')
      value.erase(0, 1);

    if (value.size() != 6)
      continue;

    try {
      const int r = std::stoi(value.substr(0, 2), nullptr, 16);
      const int g = std::stoi(value.substr(2, 2), nullptr, 16);
      const int b = std::stoi(value.substr(4, 2), nullptr, 16);
      m_primaryColor = {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
      m_secondaryColor = {m_primaryColor.r * 0.45f, m_primaryColor.g * 0.45f,
                          m_primaryColor.b * 0.45f, 1.0f};
    } catch (...) {
    }
  }
}
