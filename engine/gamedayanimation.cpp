#include "gamedayanimation.h"

#include "../utils/logger.h"

#include <epoxy/gl.h>
#include <gdkmm/general.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
constexpr int kRenderW = 1024;
constexpr int kRenderH = 600;
constexpr int kFrameMs = 16;
constexpr double kDurationSeconds = 30.0;
constexpr double kFlagSpeedScale = 0.4;

std::string animationRoot() {
#if (UBUNTU == 1)
  return "/home/dev/.lightcontroller/animations";
#else
  return "/home/lights/.local/share/lights/animations";
#endif
}

GLuint compileShader(GLenum type, const char *src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (ok != GL_TRUE) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(std::max(1, len)), '\0');
    glGetShaderInfoLog(shader, len, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("GameDay shader compile failed: " + log);
  }
  return shader;
}

GLuint linkProgram(GLuint vs, GLuint fs) {
  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  GLint ok = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (ok != GL_TRUE) {
    GLint len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(std::max(1, len)), '\0');
    glGetProgramInfoLog(program, len, nullptr, log.data());
    glDeleteProgram(program);
    throw std::runtime_error("GameDay shader link failed: " + log);
  }
  return program;
}

std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return s;
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

double fireworksIntensity(double elapsed) {
  const double progress = clamp01(elapsed / kDurationSeconds);
  return progress < 0.5 ? progress * 2.0 : (1.0 - progress) * 2.0;
}

std::string backgroundPathForLeague(const std::string &league) {
  const std::string root = animationRoot();
  const std::string leagueLower = lower(league);
  if (leagueLower == "mlb")
    return root + "/gd_mlb.png";
  if (leagueLower == "nfl")
    return root + "/gd_nfl.png";
  if (leagueLower == "nhl")
    return root + "/gd_nhl.png";

  std::cerr << "GameDayAnimation unknown league background '" << league
            << "'; falling back to " << root << "/gd_mlb.png" << std::endl;
  return root + "/gd_mlb.png";
}

Glib::RefPtr<Gdk::Pixbuf>
padTransparentRgb(const Glib::RefPtr<Gdk::Pixbuf> &src) {
  if (!src || !src->get_has_alpha())
    return src;

  auto out = src->copy();
  const int w = out->get_width();
  const int h = out->get_height();
  const int stride = out->get_rowstride();
  const int channels = out->get_n_channels();
  if (channels < 4)
    return out;

  const guchar *original = src->get_pixels();
  guchar *pixels = out->get_pixels();
  const int radius = 4;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      guchar *dst = pixels + y * stride + x * channels;
      if (dst[3] != 0)
        continue;

      int bestDist = radius * radius + 1;
      const guchar *best = nullptr;
      for (int dy = -radius; dy <= radius; ++dy) {
        const int yy = y + dy;
        if (yy < 0 || yy >= h)
          continue;
        for (int dx = -radius; dx <= radius; ++dx) {
          const int xx = x + dx;
          if (xx < 0 || xx >= w)
            continue;
          const int dist = dx * dx + dy * dy;
          if (dist == 0 || dist >= bestDist)
            continue;
          const guchar *candidate = original + yy * stride + xx * channels;
          if (candidate[3] == 0)
            continue;
          best = candidate;
          bestDist = dist;
        }
      }

      if (best) {
        dst[0] = best[0];
        dst[1] = best[1];
        dst[2] = best[2];
      }
    }
  }

  return out;
}

Glib::RefPtr<Gdk::Pixbuf> loadPixbuf(const std::string &path) {
  if (path.empty())
    return {};
  static std::map<std::string, Glib::RefPtr<Gdk::Pixbuf>> cache;
  auto cached = cache.find(path);
  if (cached != cache.end())
    return cached->second;
  try {
    auto pix = padTransparentRgb(Gdk::Pixbuf::create_from_file(path));
    cache[path] = pix;
    return pix;
  } catch (const std::exception &e) {
    std::cerr << "GameDayAnimation image load failed " << path << ": "
              << e.what() << std::endl;
  } catch (...) {
    std::cerr << "GameDayAnimation image load failed " << path << std::endl;
  }
  return {};
}
} // namespace

GameDayAnimation::GameDayAnimation(GameDayAnimationData data)
    : m_data(std::move(data)), m_rng(std::random_device{}()) {
  set_hexpand(true);
  set_vexpand(true);
  set_has_depth_buffer(false);
  set_has_stencil_buffer(false);
  set_auto_render(false);
  set_use_es(true);
}

GameDayAnimation::~GameDayAnimation() { stop(); }

void GameDayAnimation::start() {
  if (m_tickConn.connected())
    return;
  m_elapsed = 0.0;
  m_initialFireworkSpawned = false;
  m_particles.clear();
  m_lastTick = std::chrono::steady_clock::now();
  m_tickConn = Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &GameDayAnimation::onTick), kFrameMs);
}

void GameDayAnimation::stop() {
  if (m_tickConn.connected())
    m_tickConn.disconnect();
}

void GameDayAnimation::on_realize() {
  Gtk::GLArea::on_realize();
  make_current();
  try {
    initGl();
    m_glReady = true;
  } catch (const std::exception &e) {
    std::cerr << "GameDayAnimation GL init failed: " << e.what() << std::endl;
    m_glReady = false;
  }
}

void GameDayAnimation::on_unrealize() {
  make_current();
  shutdownGl();
  m_glReady = false;
  Gtk::GLArea::on_unrealize();
}

bool GameDayAnimation::on_render(const Glib::RefPtr<Gdk::GLContext> &) {
  if (!m_glReady)
    return true;
  drawFrame();
  return true;
}

bool GameDayAnimation::onTick() {
  const auto now = std::chrono::steady_clock::now();
  const std::chrono::duration<double> dt = now - m_lastTick;
  m_lastTick = now;
  update(dt.count());
  queue_render();
  return m_elapsed < kDurationSeconds;
}

void GameDayAnimation::initGl() {
  const char *vs = R"(
attribute vec2 aPos;
attribute vec2 aUv;
varying vec2 vUv;
void main() {
  vUv = aUv;
  gl_Position = vec4(aPos, 0.0, 1.0);
}
)";
  const char *fs = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec2 vUv;
uniform sampler2D uTex;
void main() {
  gl_FragColor = texture2D(uTex, vUv);
}
)";
  GLuint vert = compileShader(GL_VERTEX_SHADER, vs);
  GLuint frag = compileShader(GL_FRAGMENT_SHADER, fs);
  m_program = linkProgram(vert, frag);
  glDeleteShader(vert);
  glDeleteShader(frag);

  m_aPos = glGetAttribLocation(m_program, "aPos");
  m_aUv = glGetAttribLocation(m_program, "aUv");
  m_uTex = glGetUniformLocation(m_program, "uTex");

  const GLfloat verts[] = {
      -1, -1, 0, 1, 1, -1, 1, 1, -1, 1, 0, 0,
      -1, 1,  0, 0, 1, -1, 1, 1, 1,  1, 1, 0,
  };
  glGenBuffers(1, &m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenTextures(1, &m_texture);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void GameDayAnimation::shutdownGl() {
  if (m_texture) glDeleteTextures(1, &m_texture);
  if (m_vbo) glDeleteBuffers(1, &m_vbo);
  if (m_program) glDeleteProgram(m_program);
  m_texture = 0;
  m_vbo = 0;
  m_program = 0;
}

void GameDayAnimation::update(double dt) {
  m_elapsed += dt;
  const double intensity = fireworksIntensity(m_elapsed);
  const double spawnChance = dt * (0.35 + 3.1 * intensity);
  std::uniform_real_distribution<double> chance(0.0, 1.0);
  if (!m_initialFireworkSpawned && m_elapsed <= 1.0) {
    spawnBurst();
    m_initialFireworkSpawned = true;
  } else if (m_elapsed < kDurationSeconds && intensity > 0.02 &&
      chance(m_rng) < spawnChance)
    spawnBurst();

  for (auto &p : m_particles) {
    p.age += dt;
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    const double drag = std::exp(-1.55 * dt);
    p.vx *= drag;
    p.vy *= drag;
    p.vy += 32.0 * dt;
  }
  m_particles.erase(
      std::remove_if(m_particles.begin(), m_particles.end(),
                     [](const Particle &p) { return p.age >= p.life; }),
      m_particles.end());
}

void GameDayAnimation::spawnBurst() {
  const double intensity = std::max(0.12, fireworksIntensity(m_elapsed));
  std::uniform_real_distribution<double> xdist(120.0, 904.0);
  std::uniform_real_distribution<double> ydist(70.0, 245.0);
  std::uniform_real_distribution<double> speed(28.0 + 38.0 * intensity,
                                               82.0 + 118.0 * intensity);
  std::uniform_real_distribution<double> life(0.7, 1.0 + 1.15 * intensity);
  std::uniform_real_distribution<double> radius(0.8 + 1.2 * intensity,
                                                2.2 + 5.3 * intensity);
  std::uniform_real_distribution<double> alpha(0.36 + 0.22 * intensity,
                                               0.58 + 0.38 * intensity);
  std::uniform_real_distribution<double> angle(0.0, 6.283185307);
  std::uniform_int_distribution<int> count(
      static_cast<int>(10 + 16 * intensity),
      static_cast<int>(18 + 54 * intensity));

  const double cx = xdist(m_rng);
  const double cy = ydist(m_rng);
  const int n = count(m_rng);
  for (int i = 0; i < n; ++i) {
    const double a = angle(m_rng);
    const double s = speed(m_rng);
    Particle p;
    p.x = cx;
    p.y = cy;
    p.vx = std::cos(a) * s;
    p.vy = std::sin(a) * s;
    p.life = life(m_rng);
    p.radius = radius(m_rng);
    p.alphaScale = alpha(m_rng);
    p.color = seattleFireworkColor();
    m_particles.push_back(p);
  }
}

void GameDayAnimation::drawFrame() {
  auto surface =
      Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, kRenderW, kRenderH);
  auto cr = Cairo::Context::create(surface);

  drawBackgroundImage(cr, kRenderW, kRenderH, m_elapsed);
  drawFireworks(cr);

  drawLogo(cr, m_data.left.logoPath, 136, 300, 156, 156);
  drawLogo(cr, m_data.right.logoPath, 889, 300, 156, 156);
  drawWavingFlag(cr, m_data.left, true, 333, 300, 256, 171, m_elapsed);
  drawWavingFlag(cr, m_data.right, false, 692, 300, 256, 171, m_elapsed + 0.42);

  drawOutlinedText(cr, "VS", 512, 300, 57, {0.88, 0.9, 0.92, 1}, 7);
  drawOutlinedText(cr, m_data.gameTimeDisplay.empty() ? "@ TBD"
                                                     : m_data.gameTimeDisplay,
                   512, 383, 38, {0.92, 0.94, 0.96, 1}, 6);

  surface->flush();
  const int stride = surface->get_stride();
  const unsigned char *src = surface->get_data();
  std::vector<unsigned char> rgba(static_cast<size_t>(kRenderW) * kRenderH * 4);
  for (int y = 0; y < kRenderH; ++y) {
    const unsigned char *row = src + y * stride;
    for (int x = 0; x < kRenderW; ++x) {
      const unsigned char b = row[x * 4 + 0];
      const unsigned char g = row[x * 4 + 1];
      const unsigned char r = row[x * 4 + 2];
      const unsigned char a = row[x * 4 + 3];
      const size_t o = (static_cast<size_t>(y) * kRenderW + x) * 4;
      rgba[o + 0] = r;
      rgba[o + 1] = g;
      rgba[o + 2] = b;
      rgba[o + 3] = a;
    }
  }

  uploadTexture(rgba);
  drawTexture();
}

void GameDayAnimation::uploadTexture(const std::vector<unsigned char> &rgba) {
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kRenderW, kRenderH, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, rgba.data());
  glBindTexture(GL_TEXTURE_2D, 0);
}

void GameDayAnimation::drawTexture() {
  auto alloc = get_allocation();
  glDisable(GL_DEPTH_TEST);
  glClearColor(0, 0, 0, 1);
  glViewport(0, 0, alloc.get_width(), alloc.get_height());
  glClear(GL_COLOR_BUFFER_BIT);

  const double sx = alloc.get_width() / static_cast<double>(kRenderW);
  const double sy = alloc.get_height() / static_cast<double>(kRenderH);
  const double scale = std::min(sx, sy);
  const int vpW = std::max(1, static_cast<int>(kRenderW * scale));
  const int vpH = std::max(1, static_cast<int>(kRenderH * scale));
  const int vpX = (alloc.get_width() - vpW) / 2;
  const int vpY = (alloc.get_height() - vpH) / 2;
  glViewport(vpX, vpY, vpW, vpH);

  glUseProgram(m_program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glUniform1i(m_uTex, 0);

  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glEnableVertexAttribArray(static_cast<GLuint>(m_aPos));
  glEnableVertexAttribArray(static_cast<GLuint>(m_aUv));
  glVertexAttribPointer(static_cast<GLuint>(m_aPos), 2, GL_FLOAT, GL_FALSE,
                        sizeof(GLfloat) * 4, nullptr);
  glVertexAttribPointer(static_cast<GLuint>(m_aUv), 2, GL_FLOAT, GL_FALSE,
                        sizeof(GLfloat) * 4,
                        reinterpret_cast<void *>(sizeof(GLfloat) * 2));
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glDisableVertexAttribArray(static_cast<GLuint>(m_aPos));
  glDisableVertexAttribArray(static_cast<GLuint>(m_aUv));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0);
}

void GameDayAnimation::drawFireworks(const Cairo::RefPtr<Cairo::Context> &cr) {
  cr->save();
  cr->set_operator(Cairo::OPERATOR_OVER);
  for (const auto &p : m_particles) {
    const double fade = clamp01(1.0 - p.age / p.life) * p.alphaScale;
    if (fade <= 0.01)
      continue;

    const double shrink = 0.35 + 0.65 * clamp01(1.0 - p.age / p.life);
    const double coreRadius = p.radius * shrink;
    const double glowRadius = coreRadius * 2.15;
    const double trailX = p.x - p.vx * 0.045;
    const double trailY = p.y - p.vy * 0.045;

    cr->set_line_width(std::max(1.0, coreRadius * 0.55));
    cr->set_line_cap(Cairo::LINE_CAP_ROUND);
    cr->set_source_rgba(p.color.r, p.color.g, p.color.b, fade * 0.24);
    cr->move_to(trailX, trailY);
    cr->line_to(p.x, p.y);
    cr->stroke();

    cr->set_source_rgba(p.color.r, p.color.g, p.color.b, fade * 0.12);
    cr->arc(p.x, p.y, glowRadius, 0, 6.283185307);
    cr->fill();
    cr->set_source_rgba(1.0, 1.0, 1.0, fade * 0.34);
    cr->arc(p.x, p.y, coreRadius * 0.72, 0, 6.283185307);
    cr->fill();
    cr->set_source_rgba(p.color.r, p.color.g, p.color.b, fade * 0.92);
    cr->arc(p.x, p.y, coreRadius, 0, 6.283185307);
    cr->fill();
  }
  cr->restore();
}

void GameDayAnimation::drawBackgroundImage(const Cairo::RefPtr<Cairo::Context> &cr,
                                           int w, int h, double t) {
  const std::string backgroundPath = backgroundPathForLeague(m_data.league);
  auto pix = loadPixbuf(backgroundPath);
  if (!pix) {
    std::cerr << "GameDayAnimation background missing " << backgroundPath
              << "; using league fallback" << std::endl;
    drawBackground(cr, w, h, t);
    return;
  }

  const double scale =
      std::max(w / static_cast<double>(pix->get_width()),
               h / static_cast<double>(pix->get_height()));
  const int sw = std::max(1, static_cast<int>(pix->get_width() * scale));
  const int sh = std::max(1, static_cast<int>(pix->get_height() * scale));
  auto scaled = pix->scale_simple(sw, sh, Gdk::INTERP_BILINEAR);
  Gdk::Cairo::set_source_pixbuf(cr, scaled, (w - sw) / 2.0, (h - sh) / 2.0);
  cr->paint();
}

void GameDayAnimation::drawLogo(const Cairo::RefPtr<Cairo::Context> &cr,
                                const std::string &path, double cx, double cy,
                                double maxW, double maxH) {
  if (path.empty())
    return;
  auto pix = loadPixbuf(path);
  if (!pix)
    return;
  const double scale =
      std::min(maxW / pix->get_width(), maxH / pix->get_height());
  const int w = std::max(1, static_cast<int>(pix->get_width() * scale));
  const int h = std::max(1, static_cast<int>(pix->get_height() * scale));
  auto scaled = pix->scale_simple(w, h, Gdk::INTERP_BILINEAR);
  cr->save();
  Gdk::Cairo::set_source_pixbuf(cr, scaled, cx - w / 2.0, cy - h / 2.0);
  cr->paint();
  cr->restore();
}

void GameDayAnimation::drawWavingFlag(const Cairo::RefPtr<Cairo::Context> &cr,
                                      const GameDayTeamVisual &team,
                                      bool leftSide, double cx, double cy,
                                      double maxW, double maxH, double t) {
  auto pix = loadPixbuf(team.flagPath);
  if (!pix) {
    const ColorF primary = teamColor(team, 0, {0.9, 0.9, 0.9, 1});
    const ColorF secondary = teamColor(team, 1, {0.1, 0.1, 0.1, 1});
    cr->save();
    cr->translate(cx - maxW / 2.0, cy - maxH / 2.0);
    cr->set_source_rgba(primary.r, primary.g, primary.b, 0.92);
    cr->rectangle(0, 0, maxW, maxH);
    cr->fill();
    cr->set_source_rgba(secondary.r, secondary.g, secondary.b, 0.92);
    cr->rectangle(0, maxH * 0.58, maxW, maxH * 0.22);
    cr->fill();
    cr->restore();
    return;
  }

  const double scale =
      std::min(maxW / pix->get_width(), maxH / pix->get_height());
  const int w = std::max(1, static_cast<int>(pix->get_width() * scale));
  const int h = std::max(1, static_cast<int>(pix->get_height() * scale));
  auto scaled = pix->scale_simple(w, h, Gdk::INTERP_BILINEAR);
  const double x0 = cx - w / 2.0;
  const double y0 = cy - h / 2.0;
  const int slices = 48;
  const double sliceW = w / static_cast<double>(slices);

  cr->save();
  for (int i = 0; i < slices; ++i) {
    const double xNorm = i / static_cast<double>(slices - 1);
    const double edgeFalloff = 0.55 + 0.45 * (xNorm * xNorm * (3.0 - 2.0 * xNorm));
    const double wave1 = std::sin(xNorm * 8.0 - t * 2.6 * kFlagSpeedScale);
    const double wave2 =
        std::sin(xNorm * 15.0 - t * 3.8 * kFlagSpeedScale + 1.2) * 0.35;
    const double wave3 =
        std::sin(xNorm * 4.0 - t * 1.4 * kFlagSpeedScale + 2.0) * 0.20;
    const double z = std::max(-0.14, std::min(0.14, (wave1 + wave2 + wave3) *
                                                   0.10 * edgeFalloff));
    const double yWave = z * 58.0;
    const double xTension =
        std::sin(xNorm * 6.0 - t * 2.0 * kFlagSpeedScale) * 5.0 * edgeFalloff;
    const double sx = i * sliceW;
    const double dx = x0 + sx + xTension;
    const double shade =
        0.86 + 0.14 * std::cos(xNorm * 8.0 - t * 2.6 * kFlagSpeedScale);

    cr->save();
    cr->rectangle(dx, y0 + yWave, sliceW + 2.0, h);
    cr->clip();
    Gdk::Cairo::set_source_pixbuf(cr, scaled, x0 + xTension, y0 + yWave);
    cr->paint();
    cr->set_source_rgba(shade < 1.0 ? 0.0 : 1.0, shade < 1.0 ? 0.0 : 1.0,
                        shade < 1.0 ? 0.0 : 1.0, std::abs(1.0 - shade) * 0.55);
    cr->rectangle(dx, y0 + yWave, sliceW + 2.0, h);
    cr->fill();
    cr->restore();
  }

  (void)leftSide;
  cr->restore();
}

void GameDayAnimation::drawOutlinedText(const Cairo::RefPtr<Cairo::Context> &cr,
                                        const std::string &text, double cx,
                                        double cy, double size,
                                        const ColorF &fill,
                                        double outlineWidth, bool italic) {
  if (text.empty())
    return;
  cr->save();
  cr->select_font_face("DejaVu Sans", italic ? Cairo::FONT_SLANT_ITALIC
                                             : Cairo::FONT_SLANT_NORMAL,
                       Cairo::FONT_WEIGHT_BOLD);
  cr->set_font_size(size);
  Cairo::TextExtents ext;
  cr->get_text_extents(text, ext);
  const double x = cx - (ext.width / 2.0 + ext.x_bearing);
  const double y = cy - (ext.height / 2.0 + ext.y_bearing);

  cr->move_to(x + 5, y + 7);
  cr->text_path(text);
  cr->set_source_rgba(0, 0, 0, 0.50);
  cr->fill();

  cr->move_to(x, y);
  cr->text_path(text);
  cr->set_line_join(Cairo::LINE_JOIN_ROUND);
  cr->set_line_width(outlineWidth);
  cr->set_source_rgba(0, 0, 0, 0.96);
  cr->stroke_preserve();
  cr->set_source_rgba(fill.r, fill.g, fill.b, fill.a);
  cr->fill();

  cr->move_to(x, y - size * 0.07);
  cr->text_path(text);
  cr->set_source_rgba(1, 1, 1, 0.18);
  cr->fill();
  cr->restore();
}

void GameDayAnimation::drawTeamBlock(const Cairo::RefPtr<Cairo::Context> &cr,
                                     const GameDayTeamVisual &team,
                                     bool leftSide, double t) {
  const double cx = leftSide ? 250 : 774;
  const double wave = std::sin(t * 2.1 + (leftSide ? 0.0 : 0.8)) * 5.0;
  const ColorF primary = teamColor(team, 0, {0.86, 0.88, 0.9, 1});
  const ColorF secondary = teamColor(team, 1, {0.55, 0.58, 0.62, 1});
  const ColorF fill{std::min(1.0, primary.r * 0.78 + secondary.r * 0.28),
                    std::min(1.0, primary.g * 0.78 + secondary.g * 0.28),
                    std::min(1.0, primary.b * 0.78 + secondary.b * 0.28), 1};

  drawOutlinedText(cr, upper(team.city), cx, 108 + wave, 34,
                   {0.96, 0.97, 0.98, 1}, 5);
  const std::string name = upper(team.name);
  const double size = name.size() > 11 ? 56 : 68;
  drawOutlinedText(cr, name, cx, 462 + wave, size, fill, 9, true);
}

GameDayAnimation::ColorF GameDayAnimation::teamColor(const GameDayTeamVisual &team,
                                                     size_t idx,
                                                     ColorF fallback) const {
  if (idx >= team.colors.size())
    return fallback;
  return toColorF(team.colors[idx]);
}

GameDayAnimation::ColorF GameDayAnimation::seattleFireworkColor() {
  if (m_data.seattleTeamColors.empty())
    return {0.85, 0.9, 0.95, 1};
  std::uniform_int_distribution<size_t> dist(0, m_data.seattleTeamColors.size() - 1);
  return toColorF(m_data.seattleTeamColors[dist(m_rng)], 1.0);
}

GameDayAnimation::ColorF GameDayAnimation::toColorF(const RGB_Color &c,
                                                    double alpha) {
  return {c.r / 255.0, c.g / 255.0, c.b / 255.0, alpha};
}
