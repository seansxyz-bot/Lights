#include "clock.h"

#include "../utils/logger.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
constexpr int CLOCK_MOVE_PADDING_LEFT = 10;
constexpr int CLOCK_MOVE_PADDING_RIGHT = 10;
constexpr int CLOCK_MOVE_PADDING_TOP = 10;
constexpr int CLOCK_MOVE_PADDING_BOTTOM = 10;
constexpr int MIN_REAL_ALLOCATION = 100;
constexpr int CLOCK_POSITION_REFRESH_MS = 1000;

} // namespace

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
  m_grid.set_row_homogeneous(false);
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

  updateHourlyVerticalPosition();

  if (!m_shiftVertTimerConn.connected()) {
    m_shiftVertTimerConn = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &ClockScreen::updateHourlyVerticalPosition),
        CLOCK_POSITION_REFRESH_MS);
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
bool ClockScreen::updateHourlyVerticalPosition() {
  if (!m_started)
    return false;

  int minOffsetX = 0;
  int maxOffsetX = 0;
  int minOffsetY = 0;
  int maxOffsetY = 0;

  if (!getMovementOffsetBounds(minOffsetX, maxOffsetX, minOffsetY, maxOffsetY))
    return true;

  m_offsetX = 0;

  if (minOffsetY >= maxOffsetY) {
    m_offsetY = 0;
    layoutGrid();
    return true;
  }

  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);

  std::tm localTm{};
#if defined(_WIN32)
  localtime_s(&localTm, &t);
#else
  localtime_r(&t, &localTm);
#endif

  const double seconds =
      static_cast<double>(localTm.tm_min * 60 + localTm.tm_sec);
  const double phase = seconds / 3600.0;

  const int centerY = 0;
  const int bottomY = maxOffsetY;
  const int topY = minOffsetY;

  auto lerp = [](double a, double b, double p) { return a + ((b - a) * p); };

  double y = 0.0;

  if (phase < 0.25) {
    y = lerp(centerY, bottomY, phase / 0.25);
  } else if (phase < 0.50) {
    y = lerp(bottomY, topY, (phase - 0.25) / 0.25);
  } else if (phase < 0.75) {
    y = lerp(topY, bottomY, (phase - 0.50) / 0.25);
  } else {
    y = lerp(bottomY, centerY, (phase - 0.75) / 0.25);
  }

  m_offsetY =
      std::clamp(static_cast<int>(std::lround(y)), minOffsetY, maxOffsetY);

  layoutGrid();
  return true;
}
void ClockScreen::stop() {
  const bool wasStarted = m_started;
  if (wasStarted)
    LOG_INFO() << "ClockScreen stopping";

  if (m_shiftVertTimerConn.connected())
    m_shiftVertTimerConn.disconnect();

  if (m_envTimerConn.connected())
    m_envTimerConn.disconnect();

  if (m_colorTimerConn.connected())
    m_colorTimerConn.disconnect();

  m_started = false;

  if (wasStarted)
    LOG_INFO() << "ClockScreen stopped";
}

void ClockScreen::resetPosition() {
  m_offsetX = 0;
  m_offsetY = 0;
  m_moveDown = true;
  m_moveRight = true;
  layoutGrid();
  queue_draw();
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

  if (w < MIN_REAL_ALLOCATION || h < MIN_REAL_ALLOCATION)
    return;

  int requestedGridW = 0;
  int requestedGridH = 0;
  measureClockGroup(requestedGridW, requestedGridH);

  if (requestedGridW <= 1 || requestedGridH <= 1)
    return;

  requestedGridW = std::min(requestedGridW, w);
  requestedGridH = std::min(requestedGridH, h);
  m_grid.set_size_request(requestedGridW, requestedGridH);

  int gridW = requestedGridW;
  int gridH = requestedGridH;

  if (gridW <= 1 || gridH <= 1)
    return;

  const int centeredX = (w - gridW) / 2;
  const int centeredY = (h - gridH) / 2;
  int gridLeft = centeredX + m_offsetX;
  int gridTop = centeredY + m_offsetY;

  const bool fitsWithHorizontalPadding =
      gridW <= (w - CLOCK_MOVE_PADDING_LEFT - CLOCK_MOVE_PADDING_RIGHT);
  const bool fitsWithVerticalPadding =
      gridH <= (h - CLOCK_MOVE_PADDING_TOP - CLOCK_MOVE_PADDING_BOTTOM);

  const int minLeft = fitsWithHorizontalPadding ? CLOCK_MOVE_PADDING_LEFT : 0;
  const int minTop = fitsWithVerticalPadding ? CLOCK_MOVE_PADDING_TOP : 0;
  const int maxRight =
      fitsWithHorizontalPadding ? (w - CLOCK_MOVE_PADDING_RIGHT) : w;
  const int maxBottom =
      fitsWithVerticalPadding ? (h - CLOCK_MOVE_PADDING_BOTTOM) : h;

  if (gridLeft < minLeft)
    gridLeft = minLeft;

  if (gridTop < minTop)
    gridTop = minTop;

  if (gridLeft + gridW > maxRight)
    gridLeft = maxRight - gridW;

  if (gridTop + gridH > maxBottom)
    gridTop = maxBottom - gridH;

  gridLeft = std::max(0, gridLeft);
  gridTop = std::max(0, gridTop);

  m_rootFixed.move(m_grid, gridLeft, gridTop);

  int cMinW = 0, cNatW = 0, cMinH = 0, cNatH = 0;
  m_lblClock.get_preferred_width(cMinW, cNatW);
  m_lblClock.get_preferred_height(cMinH, cNatH);

  const int cellW = gridW;
  const int cellH = cNatH;
  m_clockCell.set_size_request(cellW, cellH);

  m_lblClock.get_preferred_width(cMinW, cNatW);
  m_lblClock.get_preferred_height(cMinH, cNatH);

  const int clockX = (cellW - cNatW) / 2;
  const int clockY = m_clockNudgeY;

  m_clockCell.move(m_lblClock, clockX, clockY);
}

void ClockScreen::measureClockGroup(int &width, int &height) {
  int dateMinW = 0, dateNatW = 0, dateMinH = 0, dateNatH = 0;
  int envMinW = 0, envNatW = 0, envMinH = 0, envNatH = 0;
  int clockMinW = 0, clockNatW = 0, clockMinH = 0, clockNatH = 0;

  m_lblDate.get_preferred_width(dateMinW, dateNatW);
  m_lblDate.get_preferred_height(dateMinH, dateNatH);
  m_lblEnv.get_preferred_width(envMinW, envNatW);
  m_lblEnv.get_preferred_height(envMinH, envNatH);
  m_lblClock.get_preferred_width(clockMinW, clockNatW);
  m_lblClock.get_preferred_height(clockMinH, clockNatH);

  width = std::max(clockNatW, (std::max(dateNatW, envNatW) * 2));
  height = std::max(dateNatH, envNatH) + clockNatH;
}

bool ClockScreen::getMovementOffsetBounds(int &minOffsetX, int &maxOffsetX,
                                          int &minOffsetY, int &maxOffsetY,
                                          int *screenWOut, int *screenHOut,
                                          int *gridWOut, int *gridHOut) {
  const auto alloc = get_allocation();
  const int w = alloc.get_width();
  const int h = alloc.get_height();

  if (w < MIN_REAL_ALLOCATION || h < MIN_REAL_ALLOCATION)
    return false;

  int requestedGridW = 0;
  int requestedGridH = 0;
  measureClockGroup(requestedGridW, requestedGridH);

  if (requestedGridW <= 1 || requestedGridH <= 1)
    return false;

  requestedGridW = std::min(requestedGridW, w);
  requestedGridH = std::min(requestedGridH, h);
  m_grid.set_size_request(requestedGridW, requestedGridH);

  int gridW = requestedGridW;
  int gridH = requestedGridH;

  if (gridW <= 1 || gridH <= 1)
    return false;

  const int centeredX = (w - gridW) / 2;
  const int centeredY = (h - gridH) / 2;

  const bool fitsWithHorizontalPadding =
      gridW <= (w - CLOCK_MOVE_PADDING_LEFT - CLOCK_MOVE_PADDING_RIGHT);
  const bool fitsWithVerticalPadding =
      gridH <= (h - CLOCK_MOVE_PADDING_TOP - CLOCK_MOVE_PADDING_BOTTOM);

  const int minLeft = fitsWithHorizontalPadding ? CLOCK_MOVE_PADDING_LEFT : 0;
  const int minTop = fitsWithVerticalPadding ? CLOCK_MOVE_PADDING_TOP : 0;
  const int maxRight =
      fitsWithHorizontalPadding ? (w - CLOCK_MOVE_PADDING_RIGHT) : w;
  const int maxBottom =
      fitsWithVerticalPadding ? (h - CLOCK_MOVE_PADDING_BOTTOM) : h;

  minOffsetX = minLeft - centeredX;
  maxOffsetX = (maxRight - gridW) - centeredX;
  minOffsetY = minTop - centeredY;
  maxOffsetY = (maxBottom - gridH) - centeredY;

  if (screenWOut)
    *screenWOut = w;
  if (screenHOut)
    *screenHOut = h;
  if (gridWOut)
    *gridWOut = gridW;
  if (gridHOut)
    *gridHOut = gridH;

  return true;
}

void ClockScreen::on_size_allocate(Gtk::Allocation &allocation) {
  Gtk::EventBox::on_size_allocate(allocation);
  layoutGrid();
}

void ClockScreen::on_hide() {
  stop();
  resetPosition();
  Gtk::EventBox::on_hide();
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
  stop();
  resetPosition();
  m_signalDismissRequested.emit();
  return true;
}

bool ClockScreen::on_event(GdkEvent *event) {
  if (event && event->type == GDK_TOUCH_BEGIN) {
    LOG_INFO() << "ClockScreen touched, dismiss requested";
    stop();
    resetPosition();
    m_signalDismissRequested.emit();
    return true;
  }

  return Gtk::EventBox::on_event(event);
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
