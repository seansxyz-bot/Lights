#include "mainwindow.h"

#include "gui/deltaall.h"
#include "gui/deltagroup.h"
#include "gui/gamedayscreen.h"
#include "gui/home.h"
#include "gui/imgbutton.h"
#include "gui/patterns.h"
#include "gui/settings.h"
#include "gui/themes.h"
#include "tools/bme280.h"
#include "tools/sportsschedules.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

MainWindow::MainWindow() {
  Logger::useStdOutAndFile(LOG_FILE_MSTR, true);
  LOG_INFO() << "Logger initialized";
  LOG_INFO() << "MainWindow ctor begin";

  fullscreen();

  loadSettings();
  buildShell();
  startThreads();
  startConnections();

  add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
             Gdk::TOUCH_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK |
             Gdk::SCROLL_MASK);

  signal_event_after().connect(
      sigc::mem_fun(*this, &MainWindow::onAnyEventAfter));

  showHomePage();
  resetIdleClockTimer();

  LOG_INFO() << "MainWindow ctor complete";
}

void MainWindow::loadSettings() {
  writeToServer = true;
  LOG_INFO() << "Loading settings from " << SETTINGS_PATH;

  m_ledInfo = readLEDInfo(std::string(SETTINGS_PATH));
  m_options = readOptions(std::string(SETTINGS_PATH));
  m_schedule = readSchedule(std::string(SETTINGS_PATH));
  m_themes = readThemeColors(std::string(SETTINGS_PATH));

  LOG_INFO() << "Settings loaded. leds=" << m_ledInfo.size()
             << " schedule_entries=" << m_schedule.size();
  if (m_options.on)
    m_powerThread.setEnabled(m_options, true);
}

void MainWindow::startThreads() {
  LOG_INFO() << "Starting ClockThread";
  ClockThread::instance().start();
  ClockThread::instance().setSchedules(m_schedule);

  LOG_INFO() << "Starting PowerSwitch";
  m_powerThread.start();
  LOG_INFO() << "Starting DoorbellThread";
  m_doorbellThread.start();
  m_mobileLightsPoller =
      std::make_unique<MobileLightsPoller>(m_ledInfo, m_options, m_schedule);
  m_mobileLightsPoller->start();
}

void MainWindow::startConnections() {
  LOG_INFO() << "Connecting MainWindow signals";

  m_newHourConn =
      ClockThread::instance().signal_new_hour().connect([this](int hour) {
        LOG_INFO() << "Top of the hour: " << hour;

        if (hour == 0) {
          refreshNextKrakenGame();
        }

        if (m_schedule.size() > 16 && isGameDay(m_schedule[16].sDate)) {
          LOG_INFO()
              << "Today is game day from schedule entry 16. Showing GameDay.";
          showGameDayPage();
        }
      });
  m_newMinuteConn =
      ClockThread::instance().signal_new_minute().connect([this](int minute) {
        LOG_INFO() << "Top of the minute: " << minute;
        m_powerThread.setEnabled(m_options, gpio.read(PIN_SENSOR, true));
      });
  ClockThread::instance().signal_schedule_started().connect(
      sigc::mem_fun(*this, &MainWindow::onScheduleStarted));

  ClockThread::instance().signal_schedule_ended().connect(
      sigc::mem_fun(*this, &MainWindow::onScheduleEnded));

  m_doorbellConn = m_doorbellThread.signal_doorbell_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onDoorbellChanged));

  m_mobileLightsPoller->signal_options_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobileOptionsChanged));

  m_mobileLightsPoller->signal_leds_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobileLEDsChanged));

  m_mobileLightsPoller->signal_schedules_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onMobileSchedulesChanged));
}

void MainWindow::buildShell() {
  LOG_INFO() << "Building shell";

  add(m_overlay);

  buildOverlay();
  buildStack();
  buildPages();
  connectPageSignals();
}

void MainWindow::buildOverlay() {
  LOG_INFO() << "Building overlay";

  m_overlay.add(m_stack);

  m_overlay.add(m_stack);
  m_overlay.add_overlay(m_toast);

  m_toast.hideMessage();

  auto cancelBtn = Gtk::manage(
      new ImageButton(std::string(ICON_PATH) + "/cancel.png", 72, 0, 0));
  cancelBtn->set_can_focus(false);
  cancelBtn->get_style_context()->add_class("destructive-action");
  cancelBtn->signal_clicked().connect(
      sigc::mem_fun(*this, &MainWindow::cancelRestartCountdown));
}

void MainWindow::buildStack() {
  LOG_INFO() << "Building stack";

  m_stack.set_transition_type(Gtk::STACK_TRANSITION_TYPE_NONE);
  m_stack.set_transition_duration(250);
  m_stack.set_hexpand(true);
  m_stack.set_vexpand(true);
}

void MainWindow::buildPages() {
  LOG_INFO() << "Building persistent pages";

  m_homePage = Gtk::manage(new Home());
  m_clockPage = Gtk::manage(new ClockScreen());

  m_stack.add(*m_homePage, "home");
  m_stack.add(*m_clockPage, "clock");

  LOG_INFO() << "Persistent pages added: home, clock";
}

void MainWindow::connectPageSignals() {
  LOG_INFO() << "Connecting page signals";

  if (m_homePage) {
    m_homePage->signal_delta_all_requested().connect(
        [this]() { showDeltaAllPage(); });

    m_homePage->signal_delta_group_requested().connect(
        [this]() { showDeltaGroupPage(); });

    m_homePage->signal_themes_requested().connect(
        [this]() { showThemesPage(false); });

    m_homePage->signal_patterns_requested().connect(
        [this]() { showPatternPage(); });

    m_homePage->signal_settings_requested().connect(
        [this]() { showSettingsPage(); });
  } else {
    LOG_ERROR() << "m_homePage is null in connectPageSignals()";
  }

  if (m_clockPage) {
    m_clockPage->signal_dismiss_requested().connect(
        [this]() { dismissClockPage(); });

    m_clockPage->setEnvProvider([this](float &tempF, float &humidity) -> bool {

#ifdef MOCK_HARDWARE
      // Ubuntu / VM mode
      tempF = 71.8f;
      humidity = 43.0f;
      return true;
#else
      // Real Pi hardware
      static auto lastRead =
          std::chrono::steady_clock::now() - std::chrono::minutes(10);
      static float cachedTempF = 72.0f;
      static float cachedHumidity = 40.0f;

      auto now = std::chrono::steady_clock::now();

      if (now - lastRead >= std::chrono::minutes(10)) {
        bme280_env_data env{};

        if (bme280_read_environment(&env)) {
          cachedTempF = (env.temperature_c * 9.0f / 5.0f) + 32.0f;
          cachedHumidity = env.humidity;
          lastRead = now;

          LOG_INFO() << "Env updated: " << cachedTempF << "F, "
                     << cachedHumidity << "%";
        } else {
          LOG_ERROR() << "BME280 read failed";
        }
      }

      tempF = cachedTempF;
      humidity = cachedHumidity;
      return true;
#endif
    });
  } else {
    LOG_ERROR() << "m_clockPage is null in connectPageSignals()";
  }
}

void MainWindow::onMobileOptionsChanged(const Options &options) {
  if (m_options == options)
    return;
  m_options = options;
  std::cout << "Web Options Changed: " << std::endl;
  writeToServer = false;
  writeOptions(SETTINGS_PATH, m_options);
  writeToServer = true;
}

void MainWindow::onMobileLEDsChanged(const std::vector<LEDData> &ledInfo) {
  if (m_ledInfo == ledInfo)
    return;
  std::cout << "Web LEDs Changed: " << m_ledInfo[1].name << " - "
            << ledInfo[1].name << std::endl;
  m_ledInfo = ledInfo;
  writeToServer = false;
  writeLEDInfo(SETTINGS_PATH, m_ledInfo);
  writeToServer = true;
}

void MainWindow::onMobileSchedulesChanged(
    const std::vector<Schedule> &schedule) {
  if (m_schedule == schedule)
    return;
  std::cout << "Web Schedule Changed: " << std::endl;
  m_schedule = schedule;
  writeToServer = false;
  writeSchedule(SETTINGS_PATH, m_schedule);
  writeToServer = true;
}

void MainWindow::showPage(const std::string &pageName) {
  LOG_INFO() << "Switching to page: " << pageName;

  if (pageName != "clock")
    resetIdleClockTimer();

  m_stack.set_visible_child(pageName);
  show_all_children();
}

void MainWindow::showHomePage() {
  LOG_INFO() << "Showing home page";

  m_clockVisible = false;

  resetIdleClockTimer();
  m_stack.set_visible_child(*m_homePage);
  m_stack.show_all_children();
  show_all_children();
  m_stack.queue_draw();
}

void MainWindow::showSettingsPage() {
  LOG_INFO() << "showSettingsPage requested";

  destroyTemporaryPage("settings");

  m_settingsPage = Gtk::manage(new Settings(ICON_PATH, m_options.sensor,
                                            m_options.on, m_bluetoothState));

  m_settingsPage->signal_auto_sensor_toggled().connect([this](bool enabled) {
    m_options.sensor = enabled;
    writeOptions(std::string(SETTINGS_PATH), m_options);
    if (!enabled)
      m_powerThread.setEnabled(m_options, false);
  });

  m_settingsPage->signal_lights_toggled().connect([this](bool enabled) {
    m_options.on = enabled;
    m_powerThread.setEnabled(m_options, enabled);
  });

  m_settingsPage->signal_bluetooth_toggled().connect(
      [this](bool enabled) { m_bluetoothState = enabled; });

  m_settingsPage->signal_theme_control_requested().connect(
      [this]() { showThemesPage(true); });

  m_settingsPage->signal_restart_requested().connect([this]() {
    if (m_settingsPage)
      m_settingsPage->set_restart_enabled(false);
    startRestartCountdown();
  });

  m_settingsPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_settingsPage, "settings");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_settingsPage);
  m_stack.queue_draw();

  m_settingsPage->signal_edit_theme_requested().connect(
      [this]() { showEditThemesPage(); });
}

void MainWindow::showThemesPage(bool schedulerMode) {
  LOG_INFO() << "showThemesPage requested. schedulerMode=" << schedulerMode;

  destroyTemporaryPage("themes");

  m_themesPage = Gtk::manage(
      new Themes(std::string(ICON_PATH), m_options.theme, schedulerMode));

  if (!schedulerMode) {
    m_themesPage->signal_theme_selected().connect([this](int index) {
      m_options.theme = index;
      writeOptions(std::string(SETTINGS_PATH), m_options);
    });

    m_themesPage->signal_done().connect([this]() { showHomePage(); });
  } else {
    m_themesPage->signal_schedule_requested().connect([this](int themeIndex) {
      LOG_INFO() << "Theme scheduler requested for theme index " << themeIndex;
    });

    m_themesPage->signal_done().connect([this]() { showSettingsPage(); });
  }

  m_stack.add(*m_themesPage, "themes");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_themesPage);
  m_stack.queue_draw();
}

void MainWindow::showPatternPage() {
  LOG_INFO() << "showPatternPage requested";

  destroyTemporaryPage("pattern");

  m_patternPage =
      Gtk::manage(new Patterns(std::string(ICON_PATH), m_options.ptrn));

  m_patternPage->signal_pattern_selected().connect([this](int index) {
    m_options.ptrn = index;
    writeOptions(std::string(SETTINGS_PATH), m_options);
  });

  m_patternPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_patternPage, "pattern");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_patternPage);
  m_stack.queue_draw();
}

void MainWindow::showDeltaAllPage() {
  LOG_INFO() << "showDeltaAllPage requested";

  destroyTemporaryPage("delta_all");

  int r = 0, g = 0, b = 0;
  getAvgColor(-1, r, g, b);

  m_deltaAllPage = Gtk::manage(new DeltaAll(
      std::string(ICON_PATH), r, g, b, COLOR_PICKER_SIZE, COLOR_BAR_SIZE, 96));

  m_deltaAllPage->signal_color_changed().connect([this](int r, int g, int b) {
    for (int i = 0; i < NUM_OF_LEDS; i++) {
      m_ledInfo[i].redVal = r;
      m_ledInfo[i].grnVal = g;
      m_ledInfo[i].bluVal = b;
    }
    writeLEDInfo(SETTINGS_PATH, m_ledInfo);
  });

  m_deltaAllPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_deltaAllPage, "delta_all");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_deltaAllPage);
  m_stack.queue_draw();
}

void MainWindow::showDeltaGroupPage() {
  LOG_INFO() << "showDeltaGroupPage requested";

  destroyTemporaryPage("delta_group");

  std::array<DeltaGroup::GroupColor, 3> colors{};
  for (int group = 0; group < 3; ++group) {
    int r = 0, g = 0, b = 0;
    getAvgColor(group, r, g, b);
    colors[group] = {r, g, b};
  }

  m_deltaGroupPage = Gtk::manage(
      new DeltaGroup(std::string(ICON_PATH), m_groupSelection, colors,
                     COLOR_PICKER_SIZE, COLOR_BAR_SIZE, 96));

  m_deltaGroupPage->signal_group_color_changed().connect(
      [this](int group, int r, int g, int b) {
        m_groupSelection = group;
        for (int i = 0; i < NUM_OF_LEDS; i++) {
          if (m_ledInfo[i].group == group) {
            m_ledInfo[i].redVal = r;
            m_ledInfo[i].grnVal = g;
            m_ledInfo[i].bluVal = b;
          }
        }
        writeLEDInfo(SETTINGS_PATH, m_ledInfo);
      });

  m_deltaGroupPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_deltaGroupPage, "delta_group");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_deltaGroupPage);
  m_stack.queue_draw();
}

void MainWindow::showClockPage() {
  if (m_clockVisible) {
    LOG_WARN() << "showClockPage ignored because clock is already visible";
    return;
  }

  LOG_INFO() << "Showing clock page";

  m_clockVisible = true;

  if (m_clockPage)
    m_clockPage->start();
  else
    LOG_ERROR() << "m_clockPage is null in showClockPage()";

  m_stack.set_visible_child(*m_clockPage);
  m_stack.show_all_children();
  m_stack.queue_draw();
}

void MainWindow::dismissClockPage() {
  LOG_INFO() << "Clock page dismissed by user";

  m_clockVisible = false;

  if (m_clockPage)
    m_clockPage->stop();
  else
    LOG_ERROR() << "m_clockPage is null in dismissClockPage()";

  showHomePage();
}

void MainWindow::showGameDayPage() {
  LOG_INFO() << "showGameDayPage requested";

  destroyTemporaryPage("game_day");

  TeamInfo krakenInfo(SETTINGS_PATH, KRAKEN_FILE);
  GameInfo gameInfo = readNextGame(SETTINGS_PATH, KRAKEN_FILE);

  m_gameDayPage = Gtk::manage(new GameDayScreen(krakenInfo, gameInfo));
  m_gameDayPage->start_animation();

  const bool returnToClock = m_clockVisible;

  m_stack.add(*m_gameDayPage, "game_day");
  showPage("game_day");

  Glib::signal_timeout().connect_seconds(
      [this, returnToClock]() -> bool {
        if (m_gameDayPage)
          m_gameDayPage->stop_animation();

        destroyTemporaryPage("game_day");

        if (returnToClock) {
          m_stack.set_visible_child("clock");
          show_all_children();
        } else {
          showHomePage();
        }

        return false;
      },
      30);
}

void MainWindow::destroyTemporaryPage(const std::string &pageName) {
  auto child = m_stack.get_child_by_name(pageName);
  if (!child)
    return;

  LOG_INFO() << "Destroying temporary page: " << pageName;

  m_stack.remove(*child);

  if (pageName == "settings")
    m_settingsPage = nullptr;
  else if (pageName == "delta_all")
    m_deltaAllPage = nullptr;
  else if (pageName == "delta_group")
    m_deltaGroupPage = nullptr;
  else if (pageName == "themes")
    m_themesPage = nullptr;
  else if (pageName == "pattern")
    m_patternPage = nullptr;
  else if (pageName == "game_day")
    m_gameDayPage = nullptr;
  else if (pageName == "edit_themes")
    m_editThemesPage = nullptr;
  else if (pageName == "edit_theme")
    m_editThemePage = nullptr;
}

void MainWindow::destroyAllTemporaryPages() {
  destroyTemporaryPage("settings");
  destroyTemporaryPage("delta_all");
  destroyTemporaryPage("delta_group");
  destroyTemporaryPage("themes");
  destroyTemporaryPage("pattern");
  destroyTemporaryPage("game_day");
  destroyTemporaryPage("edit_themes");
  destroyTemporaryPage("edit_theme");
}

void MainWindow::resetIdleClockTimer() {
  if (m_idleClockConn.connected())
    m_idleClockConn.disconnect();

  if (m_clockVisible)
    return;

  m_idleClockConn = Glib::signal_timeout().connect_seconds(
      sigc::mem_fun(*this, &MainWindow::onIdleClockTimeout), 60);
}

bool MainWindow::onIdleClockTimeout() {
  LOG_INFO() << "Idle clock timeout fired";
  showClockPage();
  return false;
}

void MainWindow::onAnyEventAfter(GdkEvent *event) {
  if (!event)
    return;

  switch (event->type) {
  case GDK_BUTTON_PRESS:
  case GDK_BUTTON_RELEASE:
  case GDK_TOUCH_BEGIN:
  case GDK_TOUCH_UPDATE:
  case GDK_TOUCH_END:
  case GDK_KEY_PRESS:
  case GDK_KEY_RELEASE:
  case GDK_SCROLL:
    resetIdleClockTimer();
    break;

  default:
    break;
  }
}

void MainWindow::startRestartCountdown() {
  LOG_WARN() << "Restart countdown started";

  if (m_restartCountdownConn.connected())
    m_restartCountdownConn.disconnect();

  m_restartSecondsLeft = 5;
  updateRestartToastText();

  m_toast.showMessage(
      m_toast.text(),
      {{
          "",
          std::string(ICON_PATH) + "/cancel.png",
          72,
          0,
          0,
          "destructive-action",
          sigc::mem_fun(*this, &MainWindow::cancelRestartCountdown),
      }});

  m_overlay.queue_draw();

  m_restartCountdownConn = Glib::signal_timeout().connect_seconds(
      sigc::mem_fun(*this, &MainWindow::onRestartCountdownTick), 1);
}

void MainWindow::updateRestartToastText() {
  m_toast.setText("Restarting in... " + std::to_string(m_restartSecondsLeft));
}

bool MainWindow::onRestartCountdownTick() {
  --m_restartSecondsLeft;

  LOG_INFO() << "Restart countdown tick: " << m_restartSecondsLeft;

  if (m_restartSecondsLeft <= 0) {
    m_toast.hideMessage();

    if (m_restartCountdownConn.connected())
      m_restartCountdownConn.disconnect();

    doRestart();
    return false;
  }

  updateRestartToastText();
  return true;
}

void MainWindow::cancelRestartCountdown() {
  LOG_WARN() << "Restart countdown canceled";

  if (m_restartCountdownConn.connected())
    m_restartCountdownConn.disconnect();

  m_toast.hideMessage();

  if (m_settingsPage)
    m_settingsPage->set_restart_enabled(true);
}

void MainWindow::doRestart() {
  LOG_ERROR() << "Restarting system now";
  system("reboot");
}

void MainWindow::onScheduleStarted(const Schedule &schedules) {
  // turn on theme pattern combo
  std::cout << "StartTheme: " << schedules.name << std::endl;
}

void MainWindow::onScheduleEnded(const Schedule &schedules) {
  // turn off theme and pattern
}

void MainWindow::refreshNextKrakenGame() {
  if (m_schedule.size() <= 16) {
    LOG_ERROR() << "Schedule does not contain entry 16 for Kraken game";
    return;
  }

  LOG_INFO() << "Refreshing next Kraken game";
  SportsSchedules schedule;
  GameInfo krakenGame = schedule.getNextGame(THE_KRAKEN);
  // GameInfo marinersGame = schedule.getNextGame(THE_MARINERS);

  // more screens for 2+games in a day and combos, double header for the
  // Mariners, hockey and football....

  m_schedule[16].sDate = krakenGame.scheduledDate;
  m_schedule[16].eDate = krakenGame.scheduledDate;
  m_schedule[16].sTime = krakenGame.militaryTime;
  m_schedule[16].eTime = addHours(krakenGame.militaryTime, 3);

  writeNextGame(SETTINGS_PATH, KRAKEN_FILE, krakenGame);
  writeSchedule(SETTINGS_PATH, m_schedule);

  LOG_INFO() << "Kraken game updated. date=" << krakenGame.scheduledDate
             << " time=" << krakenGame.militaryTime;

  if (isGameDay(krakenGame.scheduledDate)) {
    LOG_INFO() << "Kraken game is today. Scheduling event.";
    ClockThread::instance().setSchedules(m_schedule);
  }
}

bool MainWindow::isGameDay(const std::string &date) {
  std::time_t t = std::time(nullptr);
  std::tm *now = std::localtime(&t);

  int month = now->tm_mon + 1;
  int day = now->tm_mday;

  char today[6];
  std::snprintf(today, sizeof(today), "%02d/%02d", month, day);

  const bool match = (date == today);

  LOG_INFO() << "isGameDay(" << date << ") -> " << (match ? "true" : "false");

  return match;
}

std::string MainWindow::addHours(const std::string &time24, int hours) {
  std::tm tm{};
  std::istringstream ss(time24);
  ss >> std::get_time(&tm, "%H:%M");

  std::time_t t = std::mktime(&tm);
  t += hours * 3600;

  std::tm *result = std::localtime(&t);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%H:%M", result);

  return buffer;
}

void MainWindow::onDoorbellChanged(bool pressed) {
  LOG_INFO() << "Doorbell changed: " << (pressed ? "PRESSED" : "RELEASED");

  if (pressed) {
    // trigger animation, sound, page, whatever
  }
}

void MainWindow::updateOptions(const Options &options) {
  m_options = options;
  writeOptions(std::string(SETTINGS_PATH), m_options);
  LOG_INFO() << "Options updated";
}

void MainWindow::updateScheduleEntry(int index, const Schedule &entry) {
  if (index < 0 || static_cast<size_t>(index) >= m_schedule.size()) {
    LOG_ERROR() << "updateScheduleEntry index out of range: " << index;
    return;
  }

  m_schedule[index] = entry;
  writeSchedule(std::string(SETTINGS_PATH), m_schedule);

  LOG_INFO() << "Schedule entry updated at index " << index;
}

void MainWindow::getAvgColor(int allOrGroup, int &r, int &g, int &b) {
  int x = 0;
  int y = 0;
  int z = 0;

  if (allOrGroup < 0) {
    for (int i = 0; i < NUM_OF_LEDS; i++) {
      x += m_ledInfo[i].redVal;
      y += m_ledInfo[i].grnVal;
      z += m_ledInfo[i].bluVal;
    }
    r = x / NUM_OF_LEDS;
    g = y / NUM_OF_LEDS;
    b = z / NUM_OF_LEDS;
  } else {
    int count = 0;
    for (int i = 0; i < NUM_OF_LEDS; i++) {
      if (m_ledInfo[i].group == allOrGroup) {
        x += m_ledInfo[i].redVal;
        y += m_ledInfo[i].grnVal;
        z += m_ledInfo[i].bluVal;
        ++count;
      }
    }

    if (count <= 0) {
      r = 0;
      g = 0;
      b = 0;
      return;
    }

    r = x / count;
    g = y / count;
    b = z / count;
  }
}
void MainWindow::showEditThemesPage() {
  LOG_INFO() << "showEditThemesPage requested";

  destroyTemporaryPage("edit_themes");

  m_editThemesPage =
      Gtk::manage(new EditThemes(std::string(SETTINGS_PATH), m_themes));

  m_editThemesPage->signal_theme_edit_requested().connect(
      [this](int themeId) { showEditThemePage(themeId); });

  m_editThemesPage->signal_done().connect([this]() { showSettingsPage(); });

  m_stack.add(*m_editThemesPage, "edit_themes");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_editThemesPage);
  m_stack.queue_draw();
}

void MainWindow::showEditThemePage(int themeId) {
  LOG_INFO() << "showEditThemePage requested themeId=" << themeId;

  destroyTemporaryPage("edit_theme");

  auto it = std::find_if(m_themes.begin(), m_themes.end(),
                         [themeId](const Theme &t) { return t.id == themeId; });

  if (it == m_themes.end()) {
    LOG_ERROR() << "Theme id not found: " << themeId;
    return;
  }

  m_editThemePage = Gtk::manage(new EditThemePage(
      std::string(ICON_PATH), *it, COLOR_PICKER_SIZE, COLOR_BAR_SIZE, 96));

  m_editThemePage->signal_save_requested().connect([this](Theme updatedTheme) {
    auto it2 = std::find_if(
        m_themes.begin(), m_themes.end(),
        [&updatedTheme](const Theme &t) { return t.id == updatedTheme.id; });

    if (it2 != m_themes.end()) {
      *it2 = updatedTheme;
      writeThemeColors(std::string(SETTINGS_PATH), m_themes);
      LOG_INFO() << "Theme saved: " << updatedTheme.name;
    }

    showEditThemesPage();
  });

  m_editThemePage->signal_cancel_requested().connect(
      [this]() { showEditThemesPage(); });

  m_stack.add(*m_editThemePage, "edit_theme");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_editThemePage);
  m_stack.queue_draw();
}

MainWindow::~MainWindow() {
  LOG_INFO() << "MainWindow dtor begin";

  m_powerThread.stop(); // 👈 ADD THIS

  if (m_idleClockConn.connected())
    m_idleClockConn.disconnect();

  if (m_restartCountdownConn.connected())
    m_restartCountdownConn.disconnect();

  if (m_themeConn.connected())
    m_themeConn.disconnect();

  if (m_newHourConn.connected())
    m_newHourConn.disconnect();

  if (m_scheduledEventConn.connected())
    m_scheduledEventConn.disconnect();

  destroyAllTemporaryPages();

  LOG_INFO() << "MainWindow dtor complete";
}
