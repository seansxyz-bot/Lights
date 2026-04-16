#include "clock.h"

#include "../utils/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

ClockScreen::ClockScreen()
    : m_cssDate(Gtk::CssProvider::create()),
      m_cssEnv(Gtk::CssProvider::create()),
      m_cssClock(Gtk::CssProvider::create()) {
  LOG_INFO() << "ClockScreen ctor begin";

  add_events(Gdk::BUTTON_PRESS_MASK | Gdk::TOUCH_MASK);

  // Root container
  add(m_rootFixed);

  // ---------- Label names ----------
  m_lblDate.set_name("clock_date");
  m_lblEnv.set_name("clock_env");
  m_lblClock.set_name("clock_time");

  // ---------- Initial text ----------
  m_lblDate.set_text("01/01/2026");
  m_lblEnv.set_text("72°F / 40%");
  m_lblClock.set_text("11:11:11");

  // ---------- Label alignment ----------
  m_lblDate.set_justify(Gtk::JUSTIFY_CENTER);
  m_lblEnv.set_justify(Gtk::JUSTIFY_CENTER);
  m_lblClock.set_halign(Gtk::ALIGN_CENTER);
  m_lblClock.set_valign(Gtk::ALIGN_START);

  m_lblClock.set_justify(Gtk::JUSTIFY_CENTER);

  m_lblDate.set_halign(Gtk::ALIGN_CENTER);
  m_lblEnv.set_halign(Gtk::ALIGN_CENTER);
  m_lblClock.set_halign(Gtk::ALIGN_CENTER);

  m_lblDate.set_valign(Gtk::ALIGN_END);
  m_lblEnv.set_valign(Gtk::ALIGN_END);

  m_lblDate.set_hexpand(true);
  m_lblEnv.set_hexpand(true);

  m_lblDate.set_single_line_mode(true);
  m_lblEnv.set_single_line_mode(true);
  m_lblClock.set_single_line_mode(true);

  // ---------- Local CSS providers ----------
  m_lblDate.get_style_context()->add_provider(m_cssDate,
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
  m_lblEnv.get_style_context()->add_provider(m_cssEnv,
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);
  m_lblClock.get_style_context()->add_provider(
      m_cssClock, GTK_STYLE_PROVIDER_PRIORITY_USER);

  setLabelCss(m_lblDate, m_cssDate, m_dateFontPx, 400);
  setLabelCss(m_lblEnv, m_cssEnv, m_envFontPx, 400);
  setLabelCss(m_lblClock, m_cssClock, m_clockFontPx, 900);

  // ---------- Grid setup ----------
  m_grid.set_row_spacing(0);
  m_grid.set_column_spacing(0);
  m_grid.set_row_homogeneous(true);
  m_grid.set_column_homogeneous(true);

  m_grid.attach(m_lblDate, 0, 0, 1, 1);
  m_grid.attach(m_lblEnv, 1, 0, 1, 1);

  m_clockCell.put(m_lblClock, 0, 0);
  m_grid.attach(m_clockCell, 0, 1, 2, 1);

  m_rootFixed.put(m_grid, 0, 0);

  // ---------- Default colors ----------
  m_bgColor.set_rgba(0.0, 0.0, 0.0, 1.0);
  m_fgColor.set_rgba(1.0, 1.0, 1.0, 1.0);
  applyColors();

  // ---------- Dispatcher ----------
  m_dispatcher.connect(sigc::mem_fun(*this, &ClockScreen::onClockDispatcher));

  // ---------- Clock thread updates ----------
  m_clockTickConn = ClockThread::instance().signal_tick().connect([this]() {
    {
      std::lock_guard<std::mutex> lock(m_dataMutex);
      m_pendingDate = ClockThread::instance().dateText();
      m_pendingClock = ClockThread::instance().timeText();
    }
    m_dispatcher.emit();
  });

  // ---------- Initial text/layout only ----------
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_pendingDate = ClockThread::instance().dateText();
    m_pendingClock = ClockThread::instance().timeText();
  }

  applyPendingText();
  show_all_children();

  LOG_INFO() << "ClockScreen ctor complete";
}

ClockScreen::~ClockScreen() {
  LOG_INFO() << "ClockScreen dtor begin";

  stop();

  if (m_clockTickConn.connected())
    m_clockTickConn.disconnect();

  LOG_INFO() << "ClockScreen dtor complete";
}

void ClockScreen::setEnvProvider(
    const std::function<bool(float &, float &)> &provider) {
  LOG_INFO() << "ClockScreen env provider set";
  m_envProvider = provider;
  refreshEnvFromProvider();
}

void ClockScreen::start() {
  if (m_started) {
    LOG_WARN() << "ClockScreen::start() ignored, already started";
    return;
  }

  LOG_INFO() << "ClockScreen starting";

  m_started = true;
  m_offsetX = 0;
  m_offsetY = 0;
  m_moveDown = true;
  m_moveRight = true;
  m_inverted = false;

  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_pendingDate = ClockThread::instance().dateText();
    m_pendingClock = ClockThread::instance().timeText();
  }

  refreshEnvFromProvider();
  applyColors();
  layoutGrid();
  queue_draw();

  if (!m_shiftVertTimerConn.connected()) {
    m_shiftVertTimerConn = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &ClockScreen::shiftVertical), 2);
  }

  if (!m_shiftHorizTimerConn.connected()) {
    m_shiftHorizTimerConn = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &ClockScreen::shiftHorizontal), 5);
  }

  if (!m_envTimerConn.connected()) {
    m_envTimerConn = Glib::signal_timeout().connect_seconds(
        [this]() -> bool {
          refreshEnvFromProvider();
          return true;
        },
        300);
  }

  m_dispatcher.emit();

  LOG_INFO() << "ClockScreen started";
}

void ClockScreen::stop() {
  if (!m_started) {
    return;
  }

  LOG_INFO() << "ClockScreen stopping";

  if (m_shiftVertTimerConn.connected())
    m_shiftVertTimerConn.disconnect();

  if (m_shiftHorizTimerConn.connected())
    m_shiftHorizTimerConn.disconnect();

  if (m_envTimerConn.connected())
    m_envTimerConn.disconnect();

  if (m_colorTimerConn.connected())
    m_colorTimerConn.disconnect();

  m_started = false;

  LOG_INFO() << "ClockScreen stopped";
}

void ClockScreen::refreshEnvFromProvider() {
  if (!m_envProvider)
    return;

  float temp = 0.0f;
  float hum = 0.0f;

  if (m_envProvider(temp, hum))
    setTempHumidity(temp, hum);
}

void ClockScreen::onClockDispatcher() { applyPendingText(); }

void ClockScreen::applyPendingText() {
  std::string date;
  std::string env;
  std::string clock;

  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    date = m_pendingDate;
    env = m_pendingEnv;
    clock = m_pendingClock;
  }

  if (!date.empty())
    m_lblDate.set_text(date);

  if (!env.empty())
    m_lblEnv.set_text(env);

  if (!clock.empty())
    m_lblClock.set_text(clock);

  layoutGrid();
}

std::string ClockScreen::formatEnv(float temp_f, float humidity) const {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(0) << temp_f << "°F / "
     << std::setprecision(0) << humidity << "%";
  return ss.str();
}

void ClockScreen::setColors(const Gdk::RGBA &bg, const Gdk::RGBA &fg) {
  m_bgColor = bg;
  m_fgColor = fg;
  applyColors();
}

void ClockScreen::applyColors() {
  override_background_color(m_bgColor);
  m_lblDate.override_color(m_fgColor);
  m_lblEnv.override_color(m_fgColor);
  m_lblClock.override_color(m_fgColor);
}

bool ClockScreen::shiftVertical() {
  static constexpr int STEP_PX = 1;
  static constexpr int MIN_Y = -425;
  static constexpr int MAX_Y = 8;

  if (m_moveDown)
    m_offsetY += STEP_PX;
  else
    m_offsetY -= STEP_PX;

  if (m_offsetY >= MAX_Y) {
    m_offsetY = MAX_Y;
    m_moveDown = false;
  } else if (m_offsetY <= MIN_Y) {
    m_offsetY = MIN_Y;
    m_moveDown = true;
  }

  layoutGrid();
  return true;
}

bool ClockScreen::shiftHorizontal() {
  static constexpr int STEP_PX = 1;
  static constexpr int MIN_X = -36;
  static constexpr int MAX_X = 36;

  if (m_moveRight)
    m_offsetX += STEP_PX;
  else
    m_offsetX -= STEP_PX;

  if (m_offsetX >= MAX_X) {
    m_offsetX = MAX_X;
    m_moveRight = false;
  } else if (m_offsetX <= MIN_X) {
    m_offsetX = MIN_X;
    m_moveRight = true;
  }

  layoutGrid();
  return true;
}

void ClockScreen::setLabelCss(Gtk::Label &label,
                              const Glib::RefPtr<Gtk::CssProvider> &provider,
                              int px, int weight) {
  std::ostringstream css;
  css << "#" << label.get_name() << " {" << "font-family: Arial;"
      << "font-size: " << px << "px;" << "font-weight: " << weight << ";"
      << "padding: 0px;" << "margin: 0px;" << "}";

  try {
    provider->load_from_data(css.str());
  } catch (const Glib::Error &ex) {
    LOG_ERROR() << "CSS load error for " << label.get_name() << ": "
                << ex.what();
    LOG_ERROR() << css.str();
  }
}

void ClockScreen::layoutGrid() {
  const auto alloc = get_allocation();
  const int w = alloc.get_width();
  const int h = alloc.get_height();

  if (w <= 0 || h <= 0)
    return;

  const int gridW = w;
  const int gridH = h;

  m_grid.set_size_request(gridW, gridH);

  const int x = ((w - gridW) / 2) + m_offsetX;
  const int y = ((h - gridH) / 2) + m_offsetY;

  m_rootFixed.move(m_grid, x, y);

  const int cellW = gridW;
  const int cellH = gridH / 2;

  m_clockCell.set_size_request(cellW, cellH);

  int cMinW = 0, cNatW = 0, cMinH = 0, cNatH = 0;
  m_lblClock.get_preferred_width(cMinW, cNatW);
  m_lblClock.get_preferred_height(cMinH, cNatH);

  const int clockX = (cellW - cNatW) / 2;
  const int clockY = m_clockNudgeY;

  m_clockCell.move(m_lblClock, clockX, clockY);
}

void ClockScreen::on_size_allocate(Gtk::Allocation &allocation) {
  Gtk::EventBox::on_size_allocate(allocation);
  layoutGrid();
}

bool ClockScreen::getWidgetTopBottom(const Gtk::Widget &widget, int &top,
                                     int &bottom) const {
  int x = 0, y = 0;
  if (!const_cast<Gtk::Widget &>(widget).translate_coordinates(
          const_cast<ClockScreen &>(*this), 0, 0, x, y))
    return false;

  const auto a = widget.get_allocation();
  top = y;
  bottom = y + a.get_height();
  return true;
}

bool ClockScreen::getWidgetLeftRight(const Gtk::Widget &widget, int &left,
                                     int &right) const {
  int x = 0, y = 0;
  if (!const_cast<Gtk::Widget &>(widget).translate_coordinates(
          const_cast<ClockScreen &>(*this), 0, 0, x, y))
    return false;

  const auto a = widget.get_allocation();
  left = x;
  right = x + a.get_width();
  return true;
}

bool ClockScreen::on_button_press_event(GdkEventButton *event) {
  (void)event;
  LOG_INFO() << "ClockScreen pressed, dismiss requested";
  m_signalDismissRequested.emit();
  return true;
}

sigc::signal<void> &ClockScreen::signal_dismiss_requested() {
  return m_signalDismissRequested;
}

void ClockScreen::setTempHumidity(float temp_f, float humidity) {
  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_pendingEnv = formatEnv(temp_f, humidity);
  }
  m_dispatcher.emit();
}
