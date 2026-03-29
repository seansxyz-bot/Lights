#include "gamedayscreen.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <pango/pangocairo.h>

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr int ANIM_INTERVAL_MS = 16;
constexpr int COUNTDOWN_INTERVAL_MS = 1000;
constexpr double TEXT_SWAP_SECONDS = 1.5 / 2.0;
} // namespace

GameDayScreen::GameDayScreen(const TeamInfo &teamInfo, const GameInfo &gameInfo)
    : m_teamInfo(teamInfo), m_gameInfo(gameInfo),
      m_rng(std::random_device{}()) {

  m_gameTime = parse_game_time();

  std::cout << "GameDayScreen: " << m_gameInfo.away << " @ " << m_gameInfo.home
            << std::endl;

  queue_draw();
}

void GameDayScreen::start_animation() {
  if (m_animConn.connected())
    return;

  m_lastTick = std::chrono::steady_clock::now();
  m_animStart = m_lastTick;

  m_animConn = Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &GameDayScreen::on_timeout), ANIM_INTERVAL_MS);
}

void GameDayScreen::stop_animation() {
  if (m_animConn.connected())
    m_animConn.disconnect();
}

bool GameDayScreen::on_timeout() {
  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> dt = now - m_lastTick;
  m_lastTick = now;

  update_particles(dt.count());

  std::uniform_real_distribution<double> burstChance(0.0, 1.0);
  if (burstChance(m_rng) < 0.09)
    spawn_firework();

  queue_draw();
  return true;
}

void GameDayScreen::update_particles(double dt) {
  constexpr double gravity = 32.0;
  constexpr double drag = 0.995;

  for (auto &p : m_particles) {
    p.life -= dt;

    p.x += p.vx * dt;
    p.y += p.vy * dt;

    p.vy += gravity * dt;
    p.vx *= drag;
    p.vy *= drag;
  }

  m_particles.erase(
      std::remove_if(m_particles.begin(), m_particles.end(),
                     [](const Particle &p) { return p.life <= 0.0; }),
      m_particles.end());
}

Gdk::RGBA GameDayScreen::pick_team_color() {
  std::uniform_int_distribution<int> pick(0, 1);
  return pick(m_rng) == 0 ? m_teamInfo.c1 : m_teamInfo.c2;
}

bool GameDayScreen::use_swapped_text_colors() const {
  auto now = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(now - m_animStart).count();
  return (static_cast<int>(elapsed / TEXT_SWAP_SECONDS) % 2) == 1;
}

void GameDayScreen::spawn_firework() {
  auto alloc = get_allocation();
  const double w = alloc.get_width();
  const double h = alloc.get_height();

  if (w <= 0 || h <= 0)
    return;

  std::uniform_int_distribution<int> zonePick(0, 3);
  std::uniform_real_distribution<double> leftX(w * 0.08, w * 0.28);
  std::uniform_real_distribution<double> rightX(w * 0.72, w * 0.92);
  std::uniform_real_distribution<double> topY(h * 0.10, h * 0.30);
  std::uniform_real_distribution<double> bottomY(h * 0.68, h * 0.90);

  double cx = 0.0;
  double cy = 0.0;

  switch (zonePick(m_rng)) {
  case 0:
    cx = leftX(m_rng);
    cy = topY(m_rng);
    break;
  case 1:
    cx = rightX(m_rng);
    cy = topY(m_rng);
    break;
  case 2:
    cx = leftX(m_rng);
    cy = bottomY(m_rng);
    break;
  default:
    cx = rightX(m_rng);
    cy = bottomY(m_rng);
    break;
  }

  std::uniform_int_distribution<int> particleCountDist(24, 44);
  std::uniform_real_distribution<double> speedDist(40.0, 135.0);
  std::uniform_real_distribution<double> sizeDist(1.8, 4.5);
  std::uniform_real_distribution<double> lifeDist(0.75, 1.45);
  std::uniform_real_distribution<double> angleJitter(-0.08, 0.08);

  const int particleCount = particleCountDist(m_rng);

  for (int i = 0; i < particleCount; ++i) {
    double angle = (2.0 * PI * i) / particleCount + angleJitter(m_rng);
    double speed = speedDist(m_rng);

    Particle p;
    p.x = cx;
    p.y = cy;
    p.vx = std::cos(angle) * speed;
    p.vy = std::sin(angle) * speed;
    p.life = lifeDist(m_rng);
    p.maxLife = p.life;
    p.size = sizeDist(m_rng);
    p.color = pick_team_color();

    m_particles.push_back(p);
  }
}

void GameDayScreen::draw_logo(const Cairo::RefPtr<Cairo::Context> &cr) {
  if (!m_teamInfo.logo)
    return;

  auto alloc = get_allocation();
  const int w = alloc.get_width();
  const int h = alloc.get_height();

  const int srcW = m_teamInfo.logo->get_width();
  const int srcH = m_teamInfo.logo->get_height();

  if (srcW <= 0 || srcH <= 0 || w <= 0 || h <= 0)
    return;

  // Bigger logo
  const double maxW = w * 0.62;
  const double maxH = h * 0.62;

  const double scaleX = maxW / static_cast<double>(srcW);
  const double scaleY = maxH / static_cast<double>(srcH);
  const double scale = std::min(scaleX, scaleY);

  const int drawW = std::max(1, static_cast<int>(srcW * scale));
  const int drawH = std::max(1, static_cast<int>(srcH * scale));

  auto scaled =
      m_teamInfo.logo->scale_simple(drawW, drawH, Gdk::INTERP_BILINEAR);
  if (!scaled)
    return;

  const double x = (w - drawW) / 2.0;
  const double y = (h - drawH) / 2.0;

  cr->save();
  Gdk::Cairo::set_source_pixbuf(cr, scaled, x, y);

  // Slightly darker / stronger than before
  cr->paint_with_alpha(0.26);

  cr->restore();
}

void GameDayScreen::draw_fireworks(const Cairo::RefPtr<Cairo::Context> &cr) {
  for (const auto &p : m_particles) {
    double alpha = p.life / p.maxLife;
    alpha = std::max(0.0, std::min(1.0, alpha));

    cr->save();
    cr->set_source_rgba(p.color.get_red(), p.color.get_green(),
                        p.color.get_blue(), alpha * 0.55);
    cr->arc(p.x, p.y, p.size, 0.0, 2.0 * PI);
    cr->fill();
    cr->restore();
  }
}

std::chrono::system_clock::time_point
GameDayScreen::make_time_from_mmdd_hhmm(const std::string &scheduledDate,
                                        const std::string &militaryTime) {
  auto now = std::chrono::system_clock::now();
  std::time_t tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm = *std::localtime(&tt);

  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;

  char slash = 0;
  char colon = 0;

  std::istringstream ds(scheduledDate);
  ds >> month >> slash >> day;

  std::istringstream ts(militaryTime);
  ts >> hour >> colon >> minute;

  if (!ds || slash != '/' || !ts || colon != ':')
    return now;

  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = 0;

  std::time_t gameT = std::mktime(&tm);
  return std::chrono::system_clock::from_time_t(gameT);
}

std::chrono::system_clock::time_point GameDayScreen::parse_game_time() const {
  if (!m_gameInfo.scheduledDate.empty() && !m_gameInfo.militaryTime.empty())
    return make_time_from_mmdd_hhmm(m_gameInfo.scheduledDate,
                                    m_gameInfo.militaryTime);

  return std::chrono::system_clock::now();
}

std::string GameDayScreen::countdown_text() const {
  auto now = std::chrono::system_clock::now();

  if (m_gameTime <= now)
    return "00:00";

  auto diffMinutes =
      std::chrono::duration_cast<std::chrono::minutes>(m_gameTime - now)
          .count();

  long long hours = diffMinutes / 60;
  long long minutes = diffMinutes % 60;

  std::ostringstream ss;
  ss << std::setfill('0') << std::setw(2) << hours << ":" << std::setw(2)
     << minutes;
  return ss.str();
}

void GameDayScreen::draw_outlined_layout(
    const Cairo::RefPtr<Cairo::Context> &cr,
    const Glib::RefPtr<Pango::Layout> &layout, double x, double y,
    const Gdk::RGBA &fillColor, const Gdk::RGBA &outlineColor,
    double outlineWidth) {
  cr->save();

  cr->move_to(x + 2.0, y + 2.0);
  pango_cairo_layout_path(cr->cobj(), layout->gobj());
  cr->set_source_rgba(0.0, 0.0, 0.0, 0.18);
  cr->fill();

  cr->begin_new_path();

  cr->move_to(x, y);
  pango_cairo_layout_path(cr->cobj(), layout->gobj());

  cr->set_source_rgba(fillColor.get_red(), fillColor.get_green(),
                      fillColor.get_blue(), fillColor.get_alpha());
  cr->fill_preserve();

  cr->set_line_width(outlineWidth);
  cr->set_line_join(Cairo::LINE_JOIN_ROUND);
  cr->set_line_cap(Cairo::LINE_CAP_ROUND);
  cr->set_source_rgba(outlineColor.get_red(), outlineColor.get_green(),
                      outlineColor.get_blue(), outlineColor.get_alpha());
  cr->stroke();

  cr->restore();
}

void GameDayScreen::draw_text(const Cairo::RefPtr<Cairo::Context> &cr) {
  auto alloc = get_allocation();
  const int w = alloc.get_width();
  const int h = alloc.get_height();

  const bool swapped = use_swapped_text_colors();
  const Gdk::RGBA fillColor = swapped ? m_teamInfo.c2 : m_teamInfo.c1;
  const Gdk::RGBA outlineColor = swapped ? m_teamInfo.c1 : m_teamInfo.c2;

  auto layoutTitle = create_pango_layout("GAME DAY");
  auto layoutMatch =
      create_pango_layout(m_gameInfo.away + " @ " + m_gameInfo.home);
  auto layoutTime = create_pango_layout("TIME TILL GAME: " + countdown_text());

  layoutTitle->set_alignment(Pango::ALIGN_CENTER);
  layoutMatch->set_alignment(Pango::ALIGN_CENTER);
  layoutTime->set_alignment(Pango::ALIGN_CENTER);

  // Slightly larger text
  const int titlePx = std::max(48, static_cast<int>(h * 0.17));
  const int matchPx = std::max(32, static_cast<int>(h * 0.105));
  const int timePx = std::max(34, static_cast<int>(h * 0.115));

  Pango::FontDescription titleFont;
  titleFont.set_family("Arial");
  titleFont.set_weight(Pango::WEIGHT_BOLD);
  titleFont.set_absolute_size(titlePx * Pango::SCALE);
  layoutTitle->set_font_description(titleFont);

  Pango::FontDescription matchFont;
  matchFont.set_family("Arial");
  matchFont.set_weight(Pango::WEIGHT_BOLD);
  matchFont.set_absolute_size(matchPx * Pango::SCALE);
  layoutMatch->set_font_description(matchFont);

  Pango::FontDescription timeFont;
  timeFont.set_family("Arial");
  timeFont.set_weight(Pango::WEIGHT_BOLD);
  timeFont.set_absolute_size(timePx * Pango::SCALE);
  layoutTime->set_font_description(timeFont);

  int titleW = 0, titleH = 0;
  int matchW = 0, matchH = 0;
  int timeW = 0, timeH = 0;

  layoutTitle->get_pixel_size(titleW, titleH);
  layoutMatch->get_pixel_size(matchW, matchH);
  layoutTime->get_pixel_size(timeW, timeH);

  const int gap1 = std::max(12, static_cast<int>(h * 0.035));
  const int gap2 = std::max(12, static_cast<int>(h * 0.035));

  const int totalHeight = titleH + gap1 + matchH + gap2 + timeH;
  const int topY = (h - totalHeight) / 2;

  const double titleOutline = std::max(3.5, h * 0.009);
  const double matchOutline = std::max(2.8, h * 0.0068);
  const double timeOutline = std::max(2.8, h * 0.0072);

  draw_outlined_layout(cr, layoutTitle, (w - titleW) / 2.0, topY, fillColor,
                       outlineColor, titleOutline);

  draw_outlined_layout(cr, layoutMatch, (w - matchW) / 2.0,
                       topY + titleH + gap1, fillColor, outlineColor,
                       matchOutline);

  draw_outlined_layout(cr, layoutTime, (w - timeW) / 2.0,
                       topY + titleH + gap1 + matchH + gap2, fillColor,
                       outlineColor, timeOutline);
}

bool GameDayScreen::on_draw(const Cairo::RefPtr<Cairo::Context> &cr) {
  auto alloc = get_allocation();
  const int w = alloc.get_width();
  const int h = alloc.get_height();

  cr->save();
  cr->set_source_rgb(1.0, 1.0, 1.0);
  cr->rectangle(0, 0, w, h);
  cr->fill();
  cr->restore();

  draw_logo(cr);
  draw_fireworks(cr);
  draw_text(cr);

  return true;
}
