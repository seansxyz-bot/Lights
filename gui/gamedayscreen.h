#pragma once

#include "../tools/settingsrw.h"
#include <chrono>
#include <gtkmm.h>
#include <random>
#include <string>
#include <vector>

class GameDayScreen : public Gtk::DrawingArea {
public:
  GameDayScreen(const TeamRecord &teamInfo, const GameInfo &gameInfo);
  virtual ~GameDayScreen() = default;

  void start_animation();
  void stop_animation();

protected:
  bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;

private:
  struct Particle {
    double x = 0.0;
    double y = 0.0;
    double vx = 0.0;
    double vy = 0.0;
    double life = 0.0;
    double maxLife = 1.0;
    double size = 2.0;
    Gdk::RGBA color;
  };

private:
  bool on_timeout();

  void update_particles(double dt);
  void spawn_firework();
  void draw_logo(const Cairo::RefPtr<Cairo::Context> &cr);
  void draw_fireworks(const Cairo::RefPtr<Cairo::Context> &cr);
  void draw_text(const Cairo::RefPtr<Cairo::Context> &cr);

  void draw_outlined_layout(const Cairo::RefPtr<Cairo::Context> &cr,
                            const Glib::RefPtr<Pango::Layout> &layout, double x,
                            double y, const Gdk::RGBA &fillColor,
                            const Gdk::RGBA &outlineColor, double outlineWidth);

  std::string countdown_text() const;
  Gdk::RGBA pick_team_color();
  bool use_swapped_text_colors() const;

  std::chrono::system_clock::time_point parse_game_time() const;
  static std::chrono::system_clock::time_point
  make_time_from_mmdd_hhmm(const std::string &scheduledDate,
                           const std::string &militaryTime);

private:
  TeamRecord m_teamInfo;
  GameInfo m_gameInfo;
  std::chrono::system_clock::time_point m_gameTime;

  std::vector<Particle> m_particles;

  sigc::connection m_animConn;
  std::chrono::steady_clock::time_point m_lastTick;
  std::chrono::steady_clock::time_point m_animStart;

  std::mt19937 m_rng;
};
