#pragma once

#include "tools/logger.h"

#if (UBUNTU == 1)
#define BUTTON_WIDTH 384
#define BUTTON_MARGIN 32
#define COLOR_PICKER_SIZE 512
#define COLOR_BAR_SIZE 56
#else
#define BUTTON_WIDTH 239
#define BUTTON_MARGIN 20
#define COLOR_PICKER_SIZE 319
#define COLOR_BAR_SIZE 35
#endif

#include "gui/clock.h"
#include "gui/editteam.h"
#include "gui/editthemepage.h"
#include "gui/editthemes.h"
#include "gui/settings.h"
#include "gui/teamlist.h"
#include "gui/themes.h"
#include "gui/toastmessage.h"
#include "threads/doorbellthread.h"
#include "threads/mobilelightspoller.h"
#include "tools/ampswitch.h"
#include "tools/bluezagent.h"
#include "tools/btcontrol.h"
#include "tools/gpiohelper.h"
#include "tools/lightshow.h"
#include "tools/parserhelper.h"
#include "tools/powerswitch.h"
#include <atomic>
#include <gtkmm.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Home;
class Themes;
class Patterns;
class DeltaAll;
class DeltaGroup;
class Engine;
class EditThemes;
class EditThemePage;

enum class LedOverrideMode { None, Theme, LightShow };

class MainWindow : public Gtk::Window {
public:
  MainWindow();
  virtual ~MainWindow();

private:
  void updateLightShowState();
  void startLightShow();
  void stopLightShow();

private:
  std::atomic<bool> m_shuttingDown{false};
  std::atomic<bool> m_themeSendBusy{false};

  void sendThemeToTeensyAsync(int themeId, const std::string &themeName,
                              const std::vector<RGB_Color> &colors);
  std::unique_ptr<LightShow> m_lightShow;
  bool m_lightShowRunning = false;

  // ---------- app data ----------
  std::vector<LEDData> m_ledInfo;
  Options m_options;
  std::vector<Schedule> m_schedule;
  std::vector<Theme> m_themes;
  TeensyClient m_teensyClient;
  std::string m_lastShortToastMessage;

  sigc::connection m_powerChangedConn;
  sigc::connection m_btPowerChangedConn;

  void onPowerSwitchChanged(bool enabled);
  void onBluetoothPowerChanged(bool enabled);

  PowerSwitch m_powerThread;
  int m_groupSelection = 0;
  int m_bluetoothState = 0;
  GPIOHelper gpio;
  BTControl m_btControl;
  BluezAgent m_bluezAgent;
  AmpSwitch m_ampSwitch;

  // ---------- main shell ----------
  Gtk::Overlay m_overlay;
  Gtk::Stack m_stack;

  // ---------- persistent pages ----------
  Home *m_homePage = nullptr;
  ClockScreen *m_clockPage = nullptr;

  // ---------- lazy / temporary pages ----------
  Settings *m_settingsPage = nullptr;
  DeltaAll *m_deltaAllPage = nullptr;
  DeltaGroup *m_deltaGroupPage = nullptr;
  Themes *m_themesPage = nullptr;
  Patterns *m_patternPage = nullptr;
  Engine *m_gameDayPage = nullptr;
  EditThemes *m_editThemesPage = nullptr;
  EditThemePage *m_editThemePage = nullptr;
  TeamList *m_teamList = nullptr;
  EditTeam *m_editTeam = nullptr;
  std::atomic<bool> m_btBusy{false};
  std::thread m_btWorker;
  Glib::Dispatcher m_btUiDispatcher;

  std::mutex m_btResultMutex;
  bool m_btResultEnabled = false;
  bool m_btResultSuccess = false;
  std::string m_btResultToast;

  // ---------- restart toast ----------
  ToastMessage m_toast;
  Gtk::Widget *m_restartCancelBtn = nullptr;

  sigc::connection m_restartCountdownConn;
  int m_restartSecondsLeft = 0;

  // ---------- inactivity / screensaver ----------
  sigc::connection m_idleClockConn;
  bool m_clockVisible = false;

  // ---------- app threads ----------
  DoorbellThread m_doorbellThread;
  std::unique_ptr<MobileLightsPoller> m_mobileLightsPoller;

  // ---------- app connections ----------
  sigc::connection m_themeConn;
  sigc::connection m_newHourConn;
  sigc::connection m_newMinuteConn;
  sigc::connection m_scheduledEventConn;
  sigc::connection m_doorbellConn;
  sigc::connection m_bluetoothPollConn;
  sigc::connection m_toastHideConn;

private:
  // ---------- setup ----------
  void initializeStartupState();
  void loadSettings();
  void startThreads();
  void startConnections();
  void buildShell();
  void buildOverlay();
  void buildStack();
  void buildPages();
  void connectPageSignals();
  void onMobileOptionsChanged(const Options &options);
  void onMobileLEDsChanged(const std::vector<LEDData> &ledInfo);
  void onMobileSchedulesChanged(const std::vector<Schedule> &schedule);
  void disconnectAllBluetoothDevices();
  bool onBluetoothPollTick();
  void stopBluetoothPolling();
  void startBluetoothTransition(bool enable);
  void onBluetoothWorkerFinished();
  void setBluetoothButtonEnabled(bool enabled);

  // ---------- navigation ----------
  void showPage(const std::string &pageName);
  void showHomePage();
  void showSettingsPage();
  void showThemesPage(bool schedulerMode = false);
  void showPatternPage();
  void showDeltaAllPage();
  void showDeltaGroupPage();
  void showClockPage();
  void showGameDayPage();

  void destroyTemporaryPage(const std::string &pageName);
  void destroyAllTemporaryPages();
  void showShortToast(const std::string &message);

  // ---------- idle / screensaver ----------
  void resetIdleClockTimer();
  bool onIdleClockTimeout();
  void dismissClockPage();
  void onAnyEventAfter(GdkEvent *event);

  // ---------- restart ----------
  void startRestartCountdown();
  void updateRestartToastText();
  bool onRestartCountdownTick();
  void cancelRestartCountdown();
  void doRestart();

  // ---------- schedules / sports ----------
  void onScheduleStarted(const Schedule &schedules);
  void onScheduleEnded(const Schedule &schedules);
  std::vector<Schedule> getActiveSportsSchedules();
  bool isGameDay(const std::string &date);
  std::string addHours(const std::string &time24, int hours);
  void onDoorbellChanged(bool pressed);

  void createTeamListPage();
  void createEditTeamPage(const TeamRecord &team);
  void showTeamListPage();
  void showEditTeamPage(const TeamRecord &team);
  void removeEditTeamPage();
  void removeTeamListPage();

  // ---------- helpers ----------
  void showEditThemesPage();
  void showEditThemePage(int themeId);
  void updateOptions(const Options &options);
  void updateScheduleEntry(int index, const Schedule &entry);
  void getAvgColor(int allOrGroup, int &r, int &g, int &b);
};
