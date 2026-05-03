#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <glibmm/dispatcher.h>
#include <sigc++/sigc++.h>

#include "../storage/read.h"
#include "../storage/write.h"

class MobileLightsPoller {
public:
  MobileLightsPoller() = default;
  MobileLightsPoller(const std::vector<LEDData> &ledInfo,
                     const Options &options,
                     const std::vector<Schedule> &schedule,
                     std::string baseUrl = "");
  ~MobileLightsPoller();

  void start();
  void stop();

  void pushLedUpdate(const std::vector<LEDData> &ledInfo);
  void pushOptionsUpdate(const Options &options);
  void pushScheduleUpdate(const std::vector<Schedule> &schedule);
  void pushThemesUpdate(const std::vector<Theme> &themes);
  void pushPatternsUpdate(const std::vector<Pattern> &patterns);
  void pushTeamsUpdate(const std::vector<TeamRecord> &teams);
  void setExtendedSnapshots(const std::vector<Theme> &themes,
                            const std::vector<Pattern> &patterns,
                            const std::vector<TeamRecord> &teams);
  void requestStartupSync(const std::vector<LEDData> &ledInfo,
                          const Options &options,
                          const std::vector<Schedule> &schedule,
                          const std::vector<Theme> &themes,
                          const std::vector<Pattern> &patterns,
                          const std::vector<TeamRecord> &teams);

  Options optionsSnapshot() const;
  std::vector<LEDData> ledInfoSnapshot() const;
  std::vector<Schedule> scheduleSnapshot() const;
  std::vector<Theme> themesSnapshot() const;
  std::vector<Pattern> patternsSnapshot() const;
  std::vector<TeamRecord> teamsSnapshot() const;

  sigc::signal<void> &signal_changed();
  sigc::signal<void, Options> &signal_options_changed();
  sigc::signal<void, std::vector<LEDData>> &signal_leds_changed();
  sigc::signal<void, std::vector<Schedule>> &signal_schedules_changed();
  sigc::signal<void, std::vector<Theme>> &signal_themes_changed();
  sigc::signal<void, std::vector<Pattern>> &signal_patterns_changed();
  sigc::signal<void, std::vector<TeamRecord>> &signal_teams_changed();
  sigc::signal<void> &signal_startup_sync_complete();

private:
  void threadMain();

  bool pollOptions();
  bool pollLEDs();
  bool pollSchedules();
  bool pollThemes();
  bool pollPatterns();
  bool pollTeams();
  bool pollAll();
  bool performStartupSync();

  void onDispatch();

  static bool parseOptionsJson(const std::string &response, Options &out);
  static bool parseLEDsJson(const std::string &response,
                            std::vector<LEDData> &out);
  static bool parseSchedulesJson(const std::string &response,
                                 std::vector<Schedule> &out);
  static bool parseThemesJson(const std::string &response,
                              std::vector<Theme> &out);
  static bool parsePatternsJson(const std::string &response,
                                std::vector<Pattern> &out);
  static bool parseTeamsJson(const std::string &response,
                             std::vector<TeamRecord> &out);
  static std::string normalizeBaseUrl(std::string baseUrl);

private:
  HttpHelper m_http;

  std::atomic<bool> m_running{false};
  std::thread m_thread;

  mutable std::mutex m_mutex;

  std::vector<LEDData> m_ledInfo;
  Options m_options{};
  std::vector<Schedule> m_schedule;
  std::vector<Theme> m_themes;
  std::vector<Pattern> m_patterns;
  std::vector<TeamRecord> m_teams;
  bool m_pendingLedEcho = false;
  bool m_pendingOptionsEcho = false;
  bool m_pendingScheduleEcho = false;
  bool m_pendingThemesEcho = false;
  bool m_pendingPatternsEcho = false;
  bool m_pendingTeamsEcho = false;
  bool m_themesInitialized = false;
  bool m_patternsInitialized = false;
  bool m_teamsInitialized = false;
  bool m_startupSyncPending = false;

  std::atomic<bool> m_changedPending{false};
  std::atomic<bool> m_optionsChangedPending{false};
  std::atomic<bool> m_ledsChangedPending{false};
  std::atomic<bool> m_schedulesChangedPending{false};
  std::atomic<bool> m_themesChangedPending{false};
  std::atomic<bool> m_patternsChangedPending{false};
  std::atomic<bool> m_teamsChangedPending{false};
  std::atomic<bool> m_startupSyncCompletePending{false};

  Glib::Dispatcher m_dispatcher;

  sigc::signal<void> m_signalChanged;
  sigc::signal<void, Options> m_signalOptionsChanged;
  sigc::signal<void, std::vector<LEDData>> m_signalLEDsChanged;
  sigc::signal<void, std::vector<Schedule>> m_signalSchedulesChanged;
  sigc::signal<void, std::vector<Theme>> m_signalThemesChanged;
  sigc::signal<void, std::vector<Pattern>> m_signalPatternsChanged;
  sigc::signal<void, std::vector<TeamRecord>> m_signalTeamsChanged;
  sigc::signal<void> m_signalStartupSyncComplete;

  std::string m_baseUrl;
  std::string m_apiUrl;
  std::string m_allUrl;
  std::string m_ledUrl;
  std::string m_optUrl;
  std::string m_schUrl;
  std::string m_themesUrl;
  std::string m_patternsUrl;
  std::string m_teamsUrl;
};
