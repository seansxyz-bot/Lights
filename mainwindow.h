#pragma once

#include "utils/logger.h"

#if (UBUNTU == 1)
#define DOORBELL_SOUND_DEVICE                                                  \
  "paplay --device=alsa_output.pci-0000_00_05.0.analog-stereo"
#else
#define DOORBELL_SOUND_DEVICE ""
#endif

#include "bluetooth/bluezagent.h"
#include "bluetooth/btcontrol.h"
#include "drivers/gpio/gpiohelper.h"
#include "drivers/power/ampswitch.h"
#include "drivers/power/powerswitch.h"
#include "engine/lightshow.h"
#include "gui/clock.h"
#include "gui/editpattern.h"
#include "gui/editteam.h"
#include "gui/editthemepage.h"
#include "gui/editthemes.h"
#include "gui/lightshowsettings.h"
#include "gui/settings.h"
#include "gui/teamlist.h"
#include "gui/themes.h"
#include "gui/toastmessage.h"
#include "threads/doorbellthread.h"
#include "threads/environmentthread.h"
#include "threads/lightsensorthread.h"
#include "threads/mobilelightspoller.h"
#include "threads/sportspoller.h"
#include "utils/parserhelper.h"
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
class EditPattern;
class LightShowSettingsPage;

class MainWindow : public Gtk::Window {
public:
  MainWindow();
  virtual ~MainWindow();

private:
  void updateLightShowState();
  void startLightShow();
  void stopLightShow();
  bool setLightsPowerEnabled(bool enabled);
  bool waitForTeensyReady();

private:
  std::atomic<bool> m_shuttingDown{false};
  std::atomic<bool> m_themeSendBusy{false};
  std::mutex m_lightShowMutex;

  bool isSportsSchedule(const Schedule &s);
  void applyCurrentScheduleState();
  void restoreManualLedsAsync();

  void sendThemeToTeensyAsync(int themeId, const std::string &themeName,
                              const std::vector<RGB_Color> &colors);

  void sendPatternSpeedsToTeensyAsync(const std::vector<Pattern> &patterns);

  std::unique_ptr<LightShow> m_lightShow;
  bool m_lightShowRunning = false;
  bool m_sportsAnimationRunning = false;

  // ---------- app data ----------
  std::vector<LEDData> m_ledInfo;
  Options m_options;
  std::vector<Schedule> m_schedule;
  std::vector<Theme> m_themes;
  std::vector<Pattern> m_pattern;
  std::vector<TeamRecord> m_teams;
  std::vector<TeamRecord> m_liveSportsTeams;
  TeensyClient m_teensyClient;
  std::string m_lastShortToastMessage;
  bool m_lightsPowerEnabled = false;

  sigc::connection m_powerChangedConn;
  sigc::connection m_btPowerChangedConn;

  void onPowerSwitchChanged(bool enabled);
  void onBluetoothPowerChanged(bool enabled);

  PowerSwitch m_powerSwitch;
  int m_groupSelection = 0;
  int m_bluetoothState = 0;
  GPIOHelper gpio;
  BTControl m_btControl;
  BluezAgent m_bluezAgent;
  AmpSwitch m_ampSwitch;
  std::atomic<bool> m_ampEnabled{false};

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
  EditPattern *m_editPatternPage = nullptr;
  LightShowSettingsPage *m_lightShowSettingsPage = nullptr;
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
  LightSensorThread m_lightSensorThread;
  EnvironmentThread m_environmentThread;
  std::unique_ptr<MobileLightsPoller> m_mobileLightsPoller;
  std::unique_ptr<DailySportsPoller> m_dailySportsPoller;
  std::unique_ptr<LiveGamePoller> m_liveGamePoller;

  // ---------- app connections ----------
  sigc::connection m_themeConn;
  sigc::connection m_newHourConn;
  sigc::connection m_newMinuteConn;
  sigc::connection m_newYearConn;
  sigc::connection m_doorbellConn;
  sigc::connection m_lightSensorConn;
  sigc::connection m_environmentConn;
  sigc::connection m_bluetoothPollConn;
  sigc::connection m_bluetoothEnableTimeoutConn;
  sigc::connection m_toastHideConn;
  sigc::connection m_sportsAnimationTimeoutConn;

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
  void showThemesPage();
  void showPatternPage();
  void showDeltaAllPage();
  void showDeltaGroupPage();
  void showClockPage();
  void showGameDayPage();
  void showLightShowSettingsPage();

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
  void onSportsGamesChecked(std::vector<SportsNextGameEvent> events);
  void onSportsLiveUpdate(SportsLiveEvent event);
  void onSportsHomeScore(SportsLiveEvent event);
  void onSportsGameFinished(SportsLiveEvent event);
  void triggerSportsAnimation(const TeamRecord &team,
                              const std::string &animationType,
                              const GameInfo &gameInfo);
  void triggerHourlyGameDayAnimation();
  void refreshLiveSportsPoller();
  int themeIdForTeam(const TeamRecord &team) const;
  std::string chooseTeamAnimationPath(int teamId,
                                      const std::string &animationType) const;
  void onDoorbellChanged(bool pressed);
  void onLightSensorChanged(bool sensorWantsLightsOn);
  void onEnvironmentChanged(EnvironmentThread::Reading reading);
  EnvironmentThread::Reading m_lastEnvironmentReading{};

  void createTeamListPage();
  void createEditTeamPage(const TeamRecord &team);
  void showTeamListPage();
  void showEditTeamPage(const TeamRecord &team);
  void removeEditTeamPage();
  void removeTeamListPage();

  // ---------- helpers ----------
  void showEditThemesPage();
  void showEditThemePage(int themeId);
  void showEditPatternPage();
  void applyLightShowControl(LightShowControl control, float value);

  void updateOptions(const Options &options);
  void updateScheduleEntry(int index, const Schedule &entry);
  void getAvgColor(int allOrGroup, int &r, int &g, int &b);
  void onNewYear(int year);
  bool updateMoveableHolidayDates(int year);

  static bool isLeapYear(int year);
  static int daysInMonth(int year, int month);
  static int weekdaySunday0(int year, int month, int day);

  static std::string mmdd(int month, int day);
  static std::pair<int, int> easterSunday(int year);
  static std::pair<int, int> lastMondayOfMay(int year);
  static std::pair<int, int> firstMondayOfSeptember(int year);
  static std::pair<int, int> fourthThursdayOfNovember(int year);
};
