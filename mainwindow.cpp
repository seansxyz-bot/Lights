#include "mainwindow.h"

#include "drivers/i2c/bme280.h"
#include "drivers/network/httphelper.h"
#include "engine/engine.h"
#include "gui/deltaall.h"
#include "gui/deltagroup.h"
#include "gui/home.h"
#include "gui/imgbutton.h"
#include "gui/patterns.h"
#include "gui/settings.h"
#include "gui/themes.h"
#include "utils/ui_metrics.h"
#include <chrono>
#include <ctime>
#include <gtkmm.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <array>
#include <iomanip>
#include <sstream>
#include <string>
// git test
//  showGameDayPage();  // use later for animation

static bool setBluetoothRfkillBlocked(bool blocked) {
  const int rc =
      std::system(blocked ? "rfkill block bluetooth >/dev/null 2>&1"
                          : "rfkill unblock bluetooth >/dev/null 2>&1");
  return rc == 0;
}

MainWindow::MainWindow() : m_btControl(std::string(SETTINGS_PATH)) {
  Logger::useStdOutAndFile(LOG_FILE_MSTR, true);
  LOG_INFO() << "Logger initialized";
  LOG_INFO() << "MainWindow ctor begin";

  fullscreen();

  loadSettings();
  initializeStartupState();
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

void MainWindow::initializeStartupState() {

  m_bluetoothState = 0;

  LOG_INFO() << "BT powerOn: rfkill unblock bluetooth";
  setBluetoothRfkillBlocked(false);

  Glib::signal_idle().connect_once([this]() {
    if (!m_shuttingDown) {
      startBluetoothTransition(false);
    }
  });

  LOG_INFO() << "Applying startup power state";
  bool desiredPowerOn = m_options.on != 0;

  if (m_options.sensor) {
    desiredPowerOn = m_lightSensorThread.readOnce();

    m_options.on = desiredPowerOn ? 1 : 0;
    writeOptions(SETTINGS_PATH, m_options);
  }

  if (!m_powerSwitch.setEnabled(desiredPowerOn)) {
    LOG_WARN() << "Failed to apply startup power state: "
               << m_powerSwitch.lastError();
  }

  if (!m_ampSwitch.setEnabled(false)) {
    LOG_WARN() << "Failed to turn amp off at startup: "
               << m_ampSwitch.lastError();
  }
}

void MainWindow::loadSettings() {
  writeToServer = true;
  LOG_INFO() << "Loading settings from " << SETTINGS_PATH;

  m_ledInfo = readLEDInfo(std::string(SETTINGS_PATH));
  m_options = readOptions(std::string(SETTINGS_PATH));
  std::cout << "ONNNN - " << m_options.on << std::endl;
  m_schedule = readSchedule(std::string(SETTINGS_PATH));
  m_themes = readThemeColors(std::string(SETTINGS_PATH));
  m_pattern = readPatternSpeeds(std::string(SETTINGS_PATH));

  std::time_t now = std::time(nullptr);
  std::tm local_tm{};
  localtime_r(&now, &local_tm);
  const int year = local_tm.tm_year + 1900;

  updateMoveableHolidayDates(year);
  LOG_INFO() << "Settings loaded. leds=" << m_ledInfo.size()
             << " schedule_entries=" << m_schedule.size();

  if (!m_btControl.init()) {
    LOG_ERROR() << "BTControl init failed";
  } else {
    LOG_INFO() << "BTControl initialized";
  }
}

void MainWindow::startThreads() {
  LOG_INFO() << "Starting ClockThread";
  ClockThread::instance().start();
  ClockThread::instance().setSchedules(m_schedule);

  LOG_INFO() << "Starting DoorbellThread";
  m_doorbellThread.start();
  m_mobileLightsPoller =
      std::make_unique<MobileLightsPoller>(m_ledInfo, m_options, m_schedule);
  m_mobileLightsPoller->start();
  if (m_options.sensor) {
    m_lightSensorThread.start();
  }
  m_environmentThread.start();
}

void MainWindow::startConnections() {
  LOG_INFO() << "Connecting MainWindow signals";

  m_newHourConn =
      ClockThread::instance().signal_new_hour().connect([this](int hour) {
        LOG_INFO() << "Top of the hour: " << hour;

        if (hour == 0) {
        }
      });
  m_newMinuteConn =
      ClockThread::instance().signal_new_minute().connect([this](int minute) {
        LOG_INFO() << "Top of the minute: " << minute;
        if (m_options.sensor) {
          m_options.on = gpio.read(PIN_SENSOR, true);
          m_powerSwitch.setEnabled(m_options.on);
        }
      });
  m_newYearConn =
      ClockThread::instance().signal_new_year().connect([this](int year) {
        LOG_INFO() << "New year detected: " << year;
        onNewYear(year);
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

  m_btUiDispatcher.connect(
      sigc::mem_fun(*this, &MainWindow::onBluetoothWorkerFinished));

  m_powerChangedConn = m_powerSwitch.signal_power_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onPowerSwitchChanged));

  m_btPowerChangedConn = m_btControl.signal_power_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onBluetoothPowerChanged));

  m_lightSensorConn = m_lightSensorThread.signal_sensor_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onLightSensorChanged));

  m_environmentConn = m_environmentThread.signal_environment_changed().connect(
      sigc::mem_fun(*this, &MainWindow::onEnvironmentChanged));
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
        [this]() { showThemesPage(); });

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
      tempF = 71.8f;
      humidity = 43.0f;
      return true;
#else
      tempF = m_lastEnvironmentReading.temperatureF;
      humidity = m_lastEnvironmentReading.humidity;
      return true;
#endif
    });
  } else {
    LOG_ERROR() << "m_clockPage is null in connectPageSignals()";
  }
}

void MainWindow::onLightSensorChanged(bool sensorWantsLightsOn) {
  LOG_INFO() << "Light sensor changed -> "
             << (sensorWantsLightsOn ? "ON" : "OFF");

  m_options.on = sensorWantsLightsOn ? 1 : 0;
  writeOptions(SETTINGS_PATH, m_options);

  if (!m_powerSwitch.setEnabled(sensorWantsLightsOn)) {
    LOG_WARN() << "Failed to apply light sensor power state: "
               << m_powerSwitch.lastError();
  }

  updateLightShowState();
}

void MainWindow::onEnvironmentChanged(EnvironmentThread::Reading reading) {
  m_lastEnvironmentReading = reading;

  LOG_INFO() << "Environment: " << reading.temperatureF << "F, "
             << reading.humidity << "%, " << reading.pressureHPa << " hPa";

  if (m_clockPage) {
    m_clockPage->setTempHumidity(reading.temperatureF, reading.humidity);
  }
}

void MainWindow::onMobileOptionsChanged(const Options &options) {
  if (m_options == options)
    return;
  m_options = options;
  std::cout << "Web Options Changed: " << std::endl;
  writeToServer = false;
  writeOptions(SETTINGS_PATH, m_options);
  m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
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

  m_settingsPage =
      Gtk::manage(new Settings(ICON_PATH, m_options, m_bluetoothState));

  m_settingsPage->signal_auto_sensor_toggled().connect([this](bool enabled) {
    m_options.sensor = enabled ? 1 : 0;
    writeOptions(SETTINGS_PATH, m_options);

    if (enabled) {
      const bool sensorWantsLightsOn = m_lightSensorThread.readOnce();

      m_options.on = sensorWantsLightsOn ? 1 : 0;
      writeOptions(SETTINGS_PATH, m_options);

      if (!m_powerSwitch.setEnabled(sensorWantsLightsOn)) {
        LOG_WARN() << "Failed to apply sensor state: "
                   << m_powerSwitch.lastError();
      }

      m_lightSensorThread.start();
    } else {
      m_lightSensorThread.stop();

      // Leave lights in their current state.
      // User can use the normal Lights button after this.
    }

    updateLightShowState();
  });

  m_settingsPage->signal_lights_toggled().connect([this](bool enabled) {
    if (m_powerSwitch.setEnabled(enabled)) {
      m_options.on = enabled ? 1 : 0;
      writeOptions(std::string(SETTINGS_PATH), m_options);
      updateLightShowState();
    } else {
      LOG_WARN() << "Failed to toggle lights: " << m_powerSwitch.lastError();
    }
  });

  m_settingsPage->signal_bluetooth_toggled().connect(
      [this](bool enabled) { startBluetoothTransition(enabled); });

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

  m_settingsPage->signal_edit_pattern_requested().connect(
      [this]() { showEditPatternPage(); });

  m_settingsPage->signal_edit_teams_requested().connect(
      [this]() { showTeamListPage(); });
}

void MainWindow::setBluetoothButtonEnabled(bool enabled) {
  if (m_settingsPage)
    m_settingsPage->set_bluetooth_enabled(enabled);
}

void MainWindow::startBluetoothTransition(bool enable) {
  if (m_btBusy.exchange(true)) {
    LOG_WARN() << "Bluetooth transition ignored because one is already running";
    return;
  }

  setBluetoothButtonEnabled(false);

  if (m_btWorker.joinable())
    m_btWorker.join();

  m_btWorker = std::thread([this, enable]() {
    if (m_shuttingDown) {
      std::lock_guard<std::mutex> lock(m_btResultMutex);
      m_btResultEnabled = enable;
      m_btResultSuccess = false;
      m_btResultToast.clear();
      return;
    }

    bool success = false;
    std::string toast;

    LOG_INFO() << "Bluetooth worker started -> " << (enable ? "ON" : "OFF");

    if (enable) {
      if (!m_btControl.powerOn()) {
        LOG_ERROR() << (m_btControl.lastError().empty()
                            ? "Failed to power on bluetooth"
                            : m_btControl.lastError());
      } else {
        if (!m_ampSwitch.setEnabled(true)) {
          LOG_WARN() << "Failed to turn amp on: " << m_ampSwitch.lastError();
        }
        m_bluetoothState = 1;

        if (!m_btControl.setPairable(true))
          LOG_WARN() << "Failed to set bluetooth pairable on";

        if (!m_btControl.setDiscoverable(true))
          LOG_WARN() << "Failed to set bluetooth discoverable on";

        if (!m_btControl.startScan()) {
          LOG_WARN() << "Failed to start bluetooth scan";
        } else {
          LOG_WARN() << "Bluetooth pairing mode on";
        }

        success = true;
      }

    } else {
      if (!m_ampSwitch.setEnabled(false)) {
        LOG_WARN() << "Failed to turn amp off: " << m_ampSwitch.lastError();
      }
      m_btControl.disconnectAllDevices();
      m_btControl.stopScan();
      m_btControl.setDiscoverable(false);
      m_btControl.setPairable(false);
      m_bluezAgent.stop();

      if (!m_btControl.powerOff()) {
        LOG_ERROR() << (m_btControl.lastError().empty()
                            ? "Failed to power off bluetooth"
                            : m_btControl.lastError());
      } else {
        m_bluetoothState = 0;
        toast = "Bluetooth off";
        success = true;
      }
    }

    {
      std::lock_guard<std::mutex> lock(m_btResultMutex);
      m_btResultEnabled = enable;
      m_btResultSuccess = success;
      m_btResultToast = toast;
    }

    m_btUiDispatcher.emit();
  });
}

void MainWindow::onBluetoothWorkerFinished() {
  if (m_btWorker.joinable())
    m_btWorker.join();

  bool enable = false;
  bool success = false;
  std::string toast;

  {
    std::lock_guard<std::mutex> lock(m_btResultMutex);
    enable = m_btResultEnabled;
    success = m_btResultSuccess;
    toast = m_btResultToast;
  }

  if (success && enable) {
    if (m_bluetoothPollConn.connected())
      m_bluetoothPollConn.disconnect();

    m_bluetoothPollConn = Glib::signal_timeout().connect_seconds(
        sigc::mem_fun(*this, &MainWindow::onBluetoothPollTick), 2);
  } else if (!enable) {
    stopBluetoothPolling();
  }

  if (!toast.empty())
    showShortToast(toast);

  setBluetoothButtonEnabled(true);
  m_btBusy = false;

  LOG_INFO() << "Bluetooth worker finished success=" << success
             << " enabled=" << enable;
  updateLightShowState();
}

void MainWindow::showThemesPage() {
  destroyTemporaryPage("themes");

  m_themesPage = Gtk::manage(
      new Themes(std::string(ICON_PATH), m_themes, m_options.theme));

  m_themesPage->signal_theme_selected().connect([this](int index) {
    m_options.theme = index;
    writeOptions(std::string(SETTINGS_PATH), m_options);
    m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
  });

  m_themesPage->signal_done().connect([this]() { showHomePage(); });

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
    m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
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
      std::string(ICON_PATH), r, g, b, UiMetrics::color_picker_size(),
      UiMetrics::color_bar_size(), KEY_PAD_PIXEL_SIZE));

  m_deltaAllPage->signal_color_changed().connect([this](int r, int g, int b) {
    for (int i = 0; i < NUM_OF_LEDS; i++) {
      m_ledInfo[i].redVal = r;
      m_ledInfo[i].grnVal = g;
      m_ledInfo[i].bluVal = b;
    }
    writeLEDInfo(SETTINGS_PATH, m_ledInfo);
    auto mask =
        TeensyClient::mask24FromBitString(std::string(NUM_OF_LEDS, '1'));
    m_teensyClient.applyMaskedRGB(mask, r, g, b);
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
                     UiMetrics::color_picker_size(),
                     UiMetrics::color_bar_size(), KEY_PAD_PIXEL_SIZE));

  m_deltaGroupPage->signal_group_color_changed().connect(
      [this](int group, int r, int g, int b) {
        m_groupSelection = group;
        std::string maskString = "";
        for (int i = 0; i < NUM_OF_LEDS; i++) {
          if (m_ledInfo[i].group == group) {
            maskString += '1';
            m_ledInfo[i].redVal = r;
            m_ledInfo[i].grnVal = g;
            m_ledInfo[i].bluVal = b;
          } else
            maskString += '0';
        }
        writeLEDInfo(SETTINGS_PATH, m_ledInfo);
        auto mask = TeensyClient::mask24FromBitString(maskString);
        m_teensyClient.applyMaskedRGB(mask, static_cast<uint8_t>(r),
                                      static_cast<uint8_t>(g),
                                      static_cast<uint8_t>(b));
      });

  m_deltaGroupPage->signal_done().connect([this]() { showHomePage(); });

  m_stack.add(*m_deltaGroupPage, "delta_group");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_deltaGroupPage);
  m_stack.queue_draw();
}

void MainWindow::createTeamListPage() {
  if (m_teamList)
    return;

  m_teamList = Gtk::manage(
      new TeamList(ICON_PATH, std::string(SETTINGS_PATH) + "/teams.db"));

  m_teamList->signal_cancel().connect([this]() {
    removeTeamListPage();
    showSettingsPage();
  });

  m_teamList->signal_add_team_requested().connect([this]() {
    TeamRecord blank;
    showEditTeamPage(blank);
  });

  m_teamList->signal_edit_team_requested().connect(
      [this](TeamRecord team) { showEditTeamPage(team); });
}

void MainWindow::showTeamListPage() {
  createTeamListPage();

  if (m_teamList)
    m_teamList->reload();

  if (!m_stack.get_child_by_name("team_list"))
    m_stack.add(*m_teamList, "team_list");

  m_stack.show_all_children();
  m_stack.set_visible_child(*m_teamList);
  m_stack.queue_draw();
}

void MainWindow::removeTeamListPage() {
  if (!m_teamList)
    return;

  m_stack.remove(*m_teamList);
  m_teamList = nullptr;
}

void MainWindow::createEditTeamPage(const TeamRecord &team) {
  removeEditTeamPage();

  m_editTeam = Gtk::manage(new EditTeam(
      ICON_PATH, (std::string(SETTINGS_PATH) + "/teams.db"), team));

  m_editTeam->signal_cancel().connect([this]() {
    removeEditTeamPage();
    showTeamListPage();
  });

  m_editTeam->signal_saved().connect([this]() {
    removeEditTeamPage();
    if (m_teamList)
      m_teamList->reload();
    showTeamListPage();
  });

  m_stack.add(*m_editTeam, "edit_team");
  m_editTeam->signal_validation_failed().connect(
      [this](const std::string &msg) { showShortToast(msg); });
}

void MainWindow::showEditTeamPage(const TeamRecord &team) {
  createEditTeamPage(team);
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_editTeam);
  m_stack.queue_draw();
}

void MainWindow::removeEditTeamPage() {
  if (!m_editTeam)
    return;

  m_stack.remove(*m_editTeam);
  m_editTeam = nullptr;
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

  TeamRecord team;
  team.name = "OpenGL Test Cube";
  team.c1.set_rgba(0.20, 0.70, 1.00, 1.0);
  team.c2.set_rgba(0.18, 0.18, 0.24, 1.0);

  TeamStats stats;
  stats.ranking = 1;
  stats.wins = 99;
  stats.losses = 1;
  stats.recordText = "99-1";

  GameInfo gameInfo;
  gameInfo.home = "CUBE";
  gameInfo.away = "GTK";
  gameInfo.venue = "MainWindow Arena";

  m_gameDayPage = Gtk::manage(new Engine(team, stats, gameInfo));
  m_gameDayPage->start();

  const bool returnToClock = m_clockVisible;

  m_stack.add(*m_gameDayPage, "game_day");
  showPage("game_day");
  m_stack.set_visible_child(*m_gameDayPage);
  m_stack.show_all_children();
  show_all_children();

  Glib::signal_timeout().connect_seconds(
      [this, returnToClock]() -> bool {
        if (m_gameDayPage)
          m_gameDayPage->stop();

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

bool MainWindow::onBluetoothPollTick() {
  if (!m_bluetoothState)
    return false;

  const std::string oldStatus = m_btControl.lastStatus();

  m_btControl.scanAvailableDevices();
  m_btControl.scanPairedDevices();
  m_btControl.trustAllPairedDevices();

  const std::string newStatus = m_btControl.lastStatus();
  if (!newStatus.empty() && newStatus != oldStatus) {
    showShortToast(newStatus);
  }

  auto best = m_btControl.getBestDevice();
  if (best && best->connected) {
    return true;
  }

  if (m_btControl.autoReconnectBestDevice()) {
    const std::string reconnectStatus = m_btControl.lastStatus();
    if (!reconnectStatus.empty() && reconnectStatus != newStatus) {
      showShortToast(reconnectStatus);
    }
  }

  return true;
}

void MainWindow::stopBluetoothPolling() {
  if (m_bluetoothPollConn.connected())
    m_bluetoothPollConn.disconnect();
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
  else if (pageName == "edit_pattern")
    m_editPatternPage = nullptr;
  else if (pageName == "team_list")
    m_teamList = nullptr;
  else if (pageName == "edit_team")
    m_editTeam = nullptr;
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
  destroyTemporaryPage("edit_pattern");
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

bool MainWindow::isSportsSchedule(const Schedule &s) {
  return s.name.rfind("TEAM_", 0) == 0;
}

void MainWindow::restoreManualLedsAsync() {
  std::thread([this]() {
    for (int i = 0; i < NUM_OF_LEDS; i++) {
      uint32_t mask = (1u << i);

      m_teensyClient.applyMaskedRGB(mask,
                                    static_cast<uint8_t>(m_ledInfo[i].redVal),
                                    static_cast<uint8_t>(m_ledInfo[i].grnVal),
                                    static_cast<uint8_t>(m_ledInfo[i].bluVal));

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }).detach();
}

void MainWindow::applyCurrentScheduleState() {
  auto active = ClockThread::instance().activeSchedulesSnapshot();

  const Schedule *winner = nullptr;

  // First pass: sports schedules win
  for (const auto &base : m_schedule) {
    for (const auto &a : active) {
      if (a.name != base.name)
        continue;

      if (isSportsSchedule(a) && a.themeID > 0) {
        winner = &a;
      }
    }
  }

  // Second pass: normal scheduled theme if no sports active
  if (!winner) {
    for (const auto &base : m_schedule) {
      for (const auto &a : active) {
        if (a.name != base.name)
          continue;

        if (!isSportsSchedule(a) && a.themeID > 0) {
          winner = &a;
        }
      }
    }
  }

  if (winner) {
    m_options.theme = winner->themeID;
    m_options.ptrn = (m_options.ptrn == 0) ? 1 : m_options.ptrn;
    writeOptions(SETTINGS_PATH, m_options);
    m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
    return;
  }

  m_options.theme = 0;
  m_options.ptrn = 0;
  writeOptions(SETTINGS_PATH, m_options);
  m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
  restoreManualLedsAsync();
}

void MainWindow::onScheduleStarted(const Schedule &schedule) {
  std::cout << "StartTheme: " << schedule.name << std::endl;
  applyCurrentScheduleState();
}

void MainWindow::onScheduleEnded(const Schedule &schedule) {
  std::cout << "EndTheme: " << schedule.name << std::endl;
  applyCurrentScheduleState();
}

std::string normalizeTeamFileName(const std::string &name) {
  std::string s = name;

  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (s.rfind("the ", 0) == 0)
    s.erase(0, 4);

  for (char &c : s) {
    if (c == ' ')
      c = '_';
  }

  return s + "_logo.png";
}

void removeOldTeamSchedules(std::vector<Schedule> &list,
                            const std::vector<TeamRecord> &teams) {
  std::set<std::string> valid;

  for (const auto &t : teams) {
    valid.insert("TEAM_" + normalizeTeamFileName(t.name));
  }

  list.erase(std::remove_if(list.begin(), list.end(),
                            [&](const Schedule &s) {
                              if (s.name.rfind("TEAM_", 0) != 0)
                                return false;
                              return valid.find(s.name) == valid.end();
                            }),
             list.end());
}

std::vector<Schedule> MainWindow::getActiveSportsSchedules() {
  std::vector<Schedule> result;

  auto active = ClockThread::instance().activeSchedulesSnapshot();

  for (const auto &s : active) {
    // You NEED a reliable way to identify sports schedules
    // Option 1 (recommended): prefix name
    // Option 2: add a flag later

    if (s.name.rfind("TEAM_", 0) == 0) { // starts with "TEAM_"
      result.push_back(s);
    }
  }

  return result;
}

int findScheduleIndexByName(const std::vector<Schedule> &list,
                            const std::string &name) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void upsertSchedule(std::vector<Schedule> &list, const Schedule &newSchedule) {
  int idx = findScheduleIndexByName(list, newSchedule.name);

  if (idx >= 0) {
    list[idx] = newSchedule; // ✅ update
  } else {
    list.push_back(newSchedule); // ✅ insert
  }
}

bool MainWindow::isGameDay(const std::string &date) {
  std::time_t t = std::time(nullptr);
  std::tm *now = std::localtime(&t);

  int month = now->tm_mon + 1;
  int day = now->tm_mday;

  char today[16];
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
    std::string cmd = std::string(DOORBELL_SOUND_DEVICE) + " " +
                      std::string(SETTINGS_PATH) + "/sounds/doorbell.ogg";

    system(cmd.c_str());
  }
}

void MainWindow::updateOptions(const Options &options) {
  m_options = options;
  writeOptions(std::string(SETTINGS_PATH), m_options);
  LOG_INFO() << "Options updated";
  m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);
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
      std::string(ICON_PATH), *it, UiMetrics::color_picker_size(),
      UiMetrics::color_bar_size(), 96));

  m_editThemePage->signal_save_requested().connect([this, themeId](
                                                       Theme updatedTheme) {
    auto it2 = std::find_if(
        m_themes.begin(), m_themes.end(),
        [&updatedTheme](const Theme &t) { return t.id == updatedTheme.id; });

    if (it2 != m_themes.end()) {
      *it2 = updatedTheme;
      writeThemeColors(std::string(SETTINGS_PATH), m_themes);
      sendThemeToTeensyAsync(themeId, updatedTheme.name, updatedTheme.colors);
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

void MainWindow::showEditPatternPage() {
  LOG_INFO() << "showEditPatternPage requested";

  destroyTemporaryPage("edit_pattern");

  m_editPatternPage =
      Gtk::manage(new EditPattern(std::string(ICON_PATH), m_pattern));

  m_editPatternPage->signal_pattern_speed_preview().connect(
      [this](int patternId, int speed) {
        LOG_INFO() << "Pattern speed preview id=" << patternId
                   << " speed=" << speed;

        // Live preview only. No DB save here.
        m_teensyClient.applyPatternSpeed(static_cast<uint8_t>(patternId),
                                         static_cast<uint8_t>(speed));
      });

  m_editPatternPage->signal_save().connect(
      [this](const std::vector<Pattern> &patterns) {
        LOG_INFO() << "Saving pattern speeds";

        m_pattern = patterns;
        writePatternSpeeds(std::string(SETTINGS_PATH), m_pattern);

        // After save, turn active pattern off.
        m_options.ptrn = 0;
        writeOptions(std::string(SETTINGS_PATH), m_options);
        m_teensyClient.applyThemePattern(m_options.theme, 0);

        // Send saved speeds to Teensy.
        sendPatternSpeedsToTeensyAsync(m_pattern);

        showSettingsPage();
      });

  m_editPatternPage->signal_cancel().connect([this]() {
    LOG_INFO() << "EditPattern canceled";

    // Discard unsaved speed edits and restore current saved pattern/theme.
    m_teensyClient.applyThemePattern(m_options.theme, m_options.ptrn);

    showSettingsPage();
  });

  m_stack.add(*m_editPatternPage, "edit_pattern");
  m_stack.show_all_children();
  m_stack.set_visible_child(*m_editPatternPage);
  m_stack.queue_draw();
}

void MainWindow::showShortToast(const std::string &message) {
  if (message.empty())
    return;

  if (m_lastShortToastMessage == message)
    return;

  m_lastShortToastMessage = message;

  if (m_toastHideConn.connected())
    m_toastHideConn.disconnect();

  m_toast.showMessage(message);

  m_toastHideConn = Glib::signal_timeout().connect_seconds(
      [this]() -> bool {
        m_toast.hideMessage();
        m_lastShortToastMessage.clear();
        return false;
      },
      5);
}

void MainWindow::onPowerSwitchChanged(bool enabled) {
  LOG_INFO() << "MainWindow: power switch changed -> "
             << (enabled ? "ON" : "OFF");

  m_options.on = enabled ? 1 : 0;
  if (!m_shuttingDown)
    writeOptions(SETTINGS_PATH, m_options);
  updateLightShowState();
}

void MainWindow::onBluetoothPowerChanged(bool enabled) {
  LOG_INFO() << "MainWindow: bluetooth power changed -> "
             << (enabled ? "ON" : "OFF");

  m_bluetoothState = enabled ? 1 : 0;

  if (!enabled) {
    stopBluetoothPolling();
  }

  updateLightShowState();
}

void MainWindow::updateLightShowState() {
  if (m_shuttingDown)
    return;

  const bool shouldRun = (m_options.on && m_bluetoothState);

  LOG_INFO() << "LightShow shouldRun=" << shouldRun
             << " running=" << m_lightShowRunning;

  if (shouldRun) {
    if (!m_lightShowRunning)
      startLightShow();
  } else {
    if (m_lightShowRunning)
      stopLightShow();
  }
}

void MainWindow::startLightShow() {
  std::lock_guard<std::mutex> lock(m_lightShowMutex);

  if (m_lightShowRunning)
    return;

  LOG_INFO() << "Starting LightShow";

  m_lightShow = std::make_unique<LightShow>(
      NUM_OF_LEDS, "alsa_output.platform-soc_sound.pro-output-0.monitor");

  if (!m_lightShow->start()) {
    LOG_ERROR() << "LightShow failed to start";
    m_lightShow.reset();
    return;
  }

  m_lightShowRunning = true;
}

void MainWindow::stopLightShow() {
  std::lock_guard<std::mutex> lock(m_lightShowMutex);

  if (!m_lightShowRunning)
    return;

  LOG_INFO() << "Stopping LightShow";

  if (m_lightShow) {
    m_lightShow->stop();
    m_lightShow.reset();
  }

  m_lightShowRunning = false;

  for (const auto &led : m_ledInfo) {
    const uint32_t mask =
        (1u << led.redPin) | (1u << led.grnPin) | (1u << led.bluPin);

    m_teensyClient.applyMaskedRGB(mask, led.redVal, led.grnVal, led.bluVal);

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
  }
}

void MainWindow::sendThemeToTeensyAsync(int themeId,
                                        const std::string &themeName,
                                        const std::vector<RGB_Color> &colors) {

  if (m_themeSendBusy.exchange(true)) {
    showShortToast("Theme update already running");
    return;
  }

  std::thread([this, themeId, themeName, colors]() {
    bool ok = false;
    std::string msg;

    do {
      if (!m_teensyClient.sendThemeColors(static_cast<uint8_t>(themeId),
                                          colors)) {
        msg = "Failed to send '" + themeName + "'";
        break;
      }

      uint8_t status = TeensyClient::FILE_ERROR;

      for (int i = 0; i < 20; ++i) {
        if (!m_teensyClient.readFileStatus(status)) {
          msg = "Failed to read Teensy status";
          break;
        }

        if (status == TeensyClient::FILE_SUCCESS) {
          ok = true;
          msg = "Theme '" + themeName + "' updated";
          break;
        }

        if (status == TeensyClient::FILE_ERROR) {
          msg = "Theme '" + themeName + "' failed";
          break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      }

      if (!ok && msg.empty()) {
        msg = "Theme '" + themeName + "' timed out";
      }

    } while (false);

    Glib::signal_idle().connect_once([this, msg]() {
      showShortToast(msg);
      m_themeSendBusy = false;
    });
  }).detach();
}

void MainWindow::sendPatternSpeedsToTeensyAsync(
    const std::vector<Pattern> &patterns) {
  LOG_INFO() << "Sending pattern speeds to Teensy";

  std::thread([this, patterns]() {
    bool ok = false;
    std::string msg;

    do {
      if (!m_teensyClient.sendPatternSpeeds(patterns)) {
        msg = "Failed to send pattern speeds";
        break;
      }

      uint8_t status = TeensyClient::FILE_ERROR;

      for (int i = 0; i < 20; ++i) {
        if (!m_teensyClient.readFileStatus(status)) {
          msg = "Failed to read Teensy pattern status";
          break;
        }

        if (status == TeensyClient::FILE_SUCCESS) {
          ok = true;
          msg = "Pattern speeds updated";
          break;
        }

        if (status == TeensyClient::FILE_ERROR) {
          msg = "Pattern speed update failed";
          break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      }

      if (!ok && msg.empty())
        msg = "Pattern speed update timed out";

    } while (false);

    Glib::signal_idle().connect_once([this, msg]() { showShortToast(msg); });
  }).detach();
}

void MainWindow::onNewYear(int year) {
  const bool changed = updateMoveableHolidayDates(year);
  LOG_INFO() << "Handling yearly holiday update for " << year;
  if (changed) {
    showShortToast("Holiday dates updated for " + std::to_string(year));
    applyCurrentScheduleState();
  }
}

bool MainWindow::isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int MainWindow::daysInMonth(int year, int month) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2)
    return isLeapYear(year) ? 29 : 28;
  return kDays[month - 1];
}

// returns 0=Sunday, 1=Monday, ... 6=Saturday
int MainWindow::weekdaySunday0(int year, int month, int day) {
  std::tm tmv{};
  tmv.tm_year = year - 1900;
  tmv.tm_mon = month - 1;
  tmv.tm_mday = day;
  tmv.tm_hour = 12; // avoid weird DST edge nonsense
  tmv.tm_isdst = -1;
  std::mktime(&tmv);
  return tmv.tm_wday;
}

std::string MainWindow::mmdd(int month, int day) {
  std::ostringstream ss;
  ss << std::setfill('0') << std::setw(2) << month << "/" << std::setfill('0')
     << std::setw(2) << day;
  return ss.str();
}

std::pair<int, int> MainWindow::easterSunday(int year) {
  const int a = year % 19;
  const int b = year / 100;
  const int c = year % 100;
  const int d = b / 4;
  const int e = b % 4;
  const int f = (b + 8) / 25;
  const int g = (b - f + 1) / 3;
  const int h = (19 * a + b - d - g + 15) % 30;
  const int i = c / 4;
  const int k = c % 4;
  const int l = (32 + 2 * e + 2 * i - h - k) % 7;
  const int m = (a + 11 * h + 22 * l) / 451;
  const int month = (h + l - 7 * m + 114) / 31;
  const int day = ((h + l - 7 * m + 114) % 31) + 1;
  return {month, day};
}

std::pair<int, int> MainWindow::lastMondayOfMay(int year) {
  int day = 31;
  while (weekdaySunday0(year, 5, day) != 1)
    --day;
  return {5, day};
}

std::pair<int, int> MainWindow::firstMondayOfSeptember(int year) {
  int day = 1;
  while (weekdaySunday0(year, 9, day) != 1)
    ++day;
  return {9, day};
}

std::pair<int, int> MainWindow::fourthThursdayOfNovember(int year) {
  int count = 0;
  for (int day = 1; day <= 30; ++day) {
    if (weekdaySunday0(year, 11, day) == 4) {
      ++count;
      if (count == 4)
        return {11, day};
    }
  }
  return {11, 22}; // fallback, should never happen
}

bool MainWindow::updateMoveableHolidayDates(int year) {
  bool changed = false;

  const auto [eMonth, eDay] = easterSunday(year);
  const auto [memMonth, memDay] = lastMondayOfMay(year);
  const auto [labMonth, labDay] = firstMondayOfSeptember(year);
  const auto [thanksMonth, thanksDay] = fourthThursdayOfNovember(year);

  // Easter weekend = Good Friday through Easter Sunday
  int goodFridayMonth = eMonth;
  int goodFridayDay = eDay - 2;

  if (goodFridayDay <= 0) {
    goodFridayMonth -= 1;
    if (goodFridayMonth <= 0)
      goodFridayMonth = 12; // should never happen for Easter, but safe
    goodFridayDay += daysInMonth(year, goodFridayMonth);
  }

  const std::string easterStartDate = mmdd(goodFridayMonth, goodFridayDay);
  const std::string easterEndDate = mmdd(eMonth, eDay);

  const std::string memorialDate = mmdd(memMonth, memDay);
  const std::string laborDate = mmdd(labMonth, labDay);
  const std::string thanksgivingEndDate = mmdd(thanksMonth, thanksDay);

  // Christmas starts the day after Thanksgiving
  int christmasStartMonth = thanksMonth;
  int christmasStartDay = thanksDay + 1;

  if (christmasStartDay > daysInMonth(year, christmasStartMonth)) {
    christmasStartDay = 1;
    christmasStartMonth += 1;
    if (christmasStartMonth > 12)
      christmasStartMonth = 1;
  }

  const std::string christmasStartDate =
      mmdd(christmasStartMonth, christmasStartDay);

  for (auto &s : m_schedule) {
    if (s.name == "Easter") {
      if (s.sDate != easterStartDate || s.sTime != "00:00" ||
          s.eDate != easterEndDate || s.eTime != "23:59") {
        s.sDate = easterStartDate;
        s.sTime = "00:00";
        s.eDate = easterEndDate;
        s.eTime = "23:59";
        changed = true;
      }

    } else if (s.name == "Memorial Day") {
      if (s.sDate != memorialDate || s.sTime != "00:00" ||
          s.eDate != memorialDate || s.eTime != "23:59") {
        s.sDate = memorialDate;
        s.sTime = "00:00";
        s.eDate = memorialDate;
        s.eTime = "23:59";
        changed = true;
      }

    } else if (s.name == "Labor Day") {
      if (s.sDate != laborDate || s.sTime != "00:00" || s.eDate != laborDate ||
          s.eTime != "23:59") {
        s.sDate = laborDate;
        s.sTime = "00:00";
        s.eDate = laborDate;
        s.eTime = "23:59";
        changed = true;
      }

    } else if (s.name == "Thanksgiving") {
      if (s.sDate != "11/01" || s.sTime != "00:00" ||
          s.eDate != thanksgivingEndDate || s.eTime != "23:59") {
        s.sDate = "11/01";
        s.sTime = "00:00";
        s.eDate = thanksgivingEndDate;
        s.eTime = "23:59";
        changed = true;
      }

    } else if (s.name == "Christmas") {
      if (s.sDate != christmasStartDate || s.sTime != "00:00" ||
          s.eDate != "12/31" || s.eTime != "23:59") {
        s.sDate = christmasStartDate;
        s.sTime = "00:00";
        s.eDate = "12/31";
        s.eTime = "23:59";
        changed = true;
      }
    }
  }

  if (changed) {
    writeSchedule(std::string(SETTINGS_PATH), m_schedule);
    ClockThread::instance().setSchedules(m_schedule);
    LOG_INFO() << "Updated moveable holiday dates for year " << year;
  } else {
    LOG_INFO() << "No moveable holiday date changes needed for year " << year;
  }

  return changed;
}

MainWindow::~MainWindow() {
  LOG_INFO() << "MainWindow dtor begin";

  m_shuttingDown = true;
  m_bluetoothState = 0;

  stopLightShow();

  if (!m_ampSwitch.setEnabled(false)) {
    LOG_WARN() << "Failed to turn amp off in dtor: " << m_ampSwitch.lastError();
  }

  stopBluetoothPolling();

  if (m_btWorker.joinable()) {
    LOG_WARN() << "Bluetooth worker still running during shutdown; Joining";
    m_btWorker.join();
  }

  if (!setBluetoothRfkillBlocked(true)) {
    LOG_WARN() << "Failed to rfkill-block bluetooth in dtor";
  } else {
    LOG_INFO() << "Bluetooth rfkill-blocked in dtor";
  }

  if (m_toastHideConn.connected())
    m_toastHideConn.disconnect();

  if (m_idleClockConn.connected())
    m_idleClockConn.disconnect();

  if (m_restartCountdownConn.connected())
    m_restartCountdownConn.disconnect();

  if (m_themeConn.connected())
    m_themeConn.disconnect();

  if (m_newHourConn.connected())
    m_newHourConn.disconnect();

  if (m_newYearConn.connected())
    m_newYearConn.disconnect();

  if (m_newMinuteConn.connected())
    m_newMinuteConn.disconnect();

  if (m_scheduledEventConn.connected())
    m_scheduledEventConn.disconnect();

  m_lightSensorThread.stop();

  if (!m_powerSwitch.setEnabled(false)) {
    LOG_WARN() << "Failed to turn power off in dtor: "
               << m_powerSwitch.lastError();
  }
  m_doorbellThread.stop();

  if (m_mobileLightsPoller)
    m_mobileLightsPoller->stop();

  m_environmentConn.disconnect();
  m_environmentThread.stop();

  destroyAllTemporaryPages();

  LOG_INFO() << "MainWindow dtor complete";
}
