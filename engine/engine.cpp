#include "engine.h"

#include <cmath>
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

  m_cameraY = 1.2f + std::sin(m_elapsedSeconds * 0.8) * 0.15f;
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
  const float clearR = 0.08f;
  const float clearG = 0.08f;
  const float clearB = 0.10f;

  m_renderer.begin_frame(width, height, clearR, clearG, clearB, 1.0f);

  const float aspect =
      (height > 0) ? static_cast<float>(width) / static_cast<float>(height)
                   : 1.0f;

  Renderer::Vec3 eye{m_cameraX, m_cameraY, m_cameraZ};
  Renderer::Vec3 target{0.0f, 0.0f, 0.0f};
  Renderer::Vec3 up{0.0f, 1.0f, 0.0f};

  m_renderer.set_camera(eye, target, up, 60.0f, aspect, 0.1f, 100.0f);

  const float spin = static_cast<float>(m_elapsedSeconds * 45.0);

  Renderer::Mat4 cubeModel = Renderer::Mat4::translation(0.0f, 0.0f, 0.0f) *
                             Renderer::Mat4::rotationY(spin) *
                             Renderer::Mat4::rotationX(spin * 0.6f) *
                             Renderer::Mat4::scale(1.8f, 1.8f, 1.8f);

  Renderer::Color cubeColor{
      0.2f,
      0.7f,
      1.0f,
      1.0f,
  };

  m_renderer.draw_cube(cubeModel, cubeColor);

  Renderer::Mat4 floorModel = Renderer::Mat4::translation(0.0f, -2.2f, 0.0f) *
                              Renderer::Mat4::scale(10.0f, 10.0f, 1.0f);

  Renderer::Color floorColor{
      0.2f,
      0.2f,
      0.25f,
      1.0f,
  };

  m_renderer.draw_plane_xz(floorModel, floorColor);
}
