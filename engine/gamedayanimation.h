#pragma once

#include "../models/types.h"

#include <cairomm/context.h>
#include <epoxy/gl.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm.h>

#include <chrono>
#include <random>
#include <string>
#include <vector>

struct GameDayTeamVisual {
  std::string city;
  std::string name;
  std::string logoPath;
  std::string flagPath;
  std::vector<RGB_Color> colors;
};

struct GameDayAnimationData {
  std::string league;
  GameDayTeamVisual left;
  GameDayTeamVisual right;
  std::string gameTimeDisplay;
  std::string seattleTeamCode;
  std::vector<RGB_Color> seattleTeamColors;
};

class GameDayAnimation : public Gtk::GLArea {
public:
  explicit GameDayAnimation(GameDayAnimationData data);
  ~GameDayAnimation() override;

  void start();
  void stop();

protected:
  void on_realize() override;
  void on_unrealize() override;
  bool on_render(const Glib::RefPtr<Gdk::GLContext> &context) override;

  virtual void drawBackground(const Cairo::RefPtr<Cairo::Context> &cr, int w,
                              int h, double t) = 0;

  struct ColorF {
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;
    double a = 1.0;
  };

  static ColorF toColorF(const RGB_Color &c, double alpha = 1.0);

private:
  struct Particle {
    double x = 0;
    double y = 0;
    double vx = 0;
    double vy = 0;
    double age = 0;
    double life = 1;
    double radius = 2;
    double alphaScale = 1;
    ColorF color;
  };

  bool onTick();
  void initGl();
  void shutdownGl();
  void update(double dt);
  void drawFrame();
  void uploadTexture(const std::vector<unsigned char> &rgba);
  void drawTexture();

  void spawnBurst();
  void drawFireworks(const Cairo::RefPtr<Cairo::Context> &cr);
  void drawBackgroundImage(const Cairo::RefPtr<Cairo::Context> &cr, int w,
                           int h, double t);
  void drawLogo(const Cairo::RefPtr<Cairo::Context> &cr,
                const std::string &path, double cx, double cy, double maxW,
                double maxH);
  void drawWavingFlag(const Cairo::RefPtr<Cairo::Context> &cr,
                      const GameDayTeamVisual &team, bool leftSide, double cx,
                      double cy, double maxW, double maxH, double t);
  void drawOutlinedText(const Cairo::RefPtr<Cairo::Context> &cr,
                        const std::string &text, double cx, double cy,
                        double size, const ColorF &fill, double outlineWidth,
                        bool italic = false);
  void drawTeamBlock(const Cairo::RefPtr<Cairo::Context> &cr,
                     const GameDayTeamVisual &team, bool leftSide, double t);

  ColorF teamColor(const GameDayTeamVisual &team, size_t idx,
                   ColorF fallback) const;
  ColorF seattleFireworkColor();

private:
  GameDayAnimationData m_data;
  sigc::connection m_tickConn;
  std::chrono::steady_clock::time_point m_lastTick;
  double m_elapsed = 0.0;
  bool m_glReady = false;
  bool m_initialFireworkSpawned = false;

  GLuint m_program = 0;
  GLuint m_texture = 0;
  GLuint m_vbo = 0;
  GLint m_aPos = -1;
  GLint m_aUv = -1;
  GLint m_uTex = -1;

  std::vector<Particle> m_particles;
  std::mt19937 m_rng;
};
