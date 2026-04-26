#pragma once

#include "../storage/read.h"
#include "../storage/write.h"
#include "renderer.h"

#include <chrono>
#include <gtkmm.h>
#include <string>

class Engine : public Gtk::GLArea {
public:
  Engine(const TeamRecord &teamRecord, const TeamStats &teamStats,
         const GameInfo &gameInfo);
  virtual ~Engine();

  void start();
  void stop();

  void setTeamRecord(const TeamRecord &teamRecord);
  void setTeamStats(const TeamStats &teamStats);
  void setGameInfo(const GameInfo &gameInfo);

protected:
  void on_realize() override;
  void on_unrealize() override;
  bool on_render(const Glib::RefPtr<Gdk::GLContext> &context) override;

private:
  bool on_timeout();

  void init_gl();
  void shutdown_gl();

  void reset_timing();
  void update(double dt);
  void render_frame(int width, int height);
  void loadAnimationFile(const std::string &path);

private:
  TeamRecord m_teamRecord;
  TeamStats m_teamStats;
  GameInfo m_gameInfo;

  Renderer m_renderer;

  sigc::connection m_tickConn;

  std::chrono::steady_clock::time_point m_lastTick;
  std::chrono::steady_clock::time_point m_startTime;

  double m_elapsedSeconds = 0.0;

  float m_cameraX = 0.0f;
  float m_cameraY = 1.2f;
  float m_cameraZ = 6.0f;

  std::string m_animationType;
  std::string m_animationPath;
  Renderer::Color m_primaryColor{0.2f, 0.7f, 1.0f, 1.0f};
  Renderer::Color m_secondaryColor{0.2f, 0.2f, 0.25f, 1.0f};

  bool m_glReady = false;
};
