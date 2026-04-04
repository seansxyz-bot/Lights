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
#include "tools/gpiohelper.h"
#include "tools/parserhelper.h"
#include "tools/powerswitch.h"
#include <gtkmm.h>
#include <string>
#include <vector>

class Home;
class Themes;
class Patterns;
class DeltaAll;
class DeltaGroup;
class Engine;
class EditThemes;
class EditThemePage;

class MainWindow : public Gtk::Window {
public:
  MainWindow();
  virtual ~MainWindow();

private:
  // ---------- app data ----------
  std::vector<LEDData> m_ledInfo;
  Options m_options;
  std::vector<Schedule> m_schedule;
  std::vector<Theme> m_themes;

  PowerSwitch m_powerThread;
  int m_groupSelection = 0;
  int m_bluetoothState = 0;
  GPIOHelper gpio;

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

private:
  // ---------- setup ----------
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
  void refreshTodayGameSchedules();
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
