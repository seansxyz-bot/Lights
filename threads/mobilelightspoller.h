#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <glibmm/dispatcher.h>
#include <sigc++/sigc++.h>

#include "../tools/readerwriter.h"

class MobileLightsPoller {
public:
  MobileLightsPoller() = default;
  MobileLightsPoller(const std::vector<LEDData> &ledInfo,
                     const Options &options,
                     const std::vector<Schedule> &schedule);
  ~MobileLightsPoller();

  void start();
  void stop();

  Options optionsSnapshot() const;
  std::vector<LEDData> ledInfoSnapshot() const;
  std::vector<Schedule> scheduleSnapshot() const;

  sigc::signal<void> &signal_changed();
  sigc::signal<void, Options> &signal_options_changed();
  sigc::signal<void, std::vector<LEDData>> &signal_leds_changed();
  sigc::signal<void, std::vector<Schedule>> &signal_schedules_changed();

private:
  void threadMain();

  bool pollOptions();
  bool pollLEDs();
  bool pollSchedules();

  void onDispatch();

  static bool parseOptionsJson(const std::string &response, Options &out);
  static bool parseLEDsJson(const std::string &response,
                            std::vector<LEDData> &out);
  static bool parseSchedulesJson(const std::string &response,
                                 std::vector<Schedule> &out);

private:
  HttpHelper m_http;

  std::atomic<bool> m_running{false};
  std::thread m_thread;

  mutable std::mutex m_mutex;

  std::vector<LEDData> m_ledInfo;
  Options m_options{};
  std::vector<Schedule> m_schedule;

  std::atomic<bool> m_changedPending{false};
  std::atomic<bool> m_optionsChangedPending{false};
  std::atomic<bool> m_ledsChangedPending{false};
  std::atomic<bool> m_schedulesChangedPending{false};

  Glib::Dispatcher m_dispatcher;

  sigc::signal<void> m_signalChanged;
  sigc::signal<void, Options> m_signalOptionsChanged;
  sigc::signal<void, std::vector<LEDData>> m_signalLEDsChanged;
  sigc::signal<void, std::vector<Schedule>> m_signalSchedulesChanged;

  const std::string m_ledUrl = "http://192.168.1.100/lights_apis/led/";
  const std::string m_optUrl = "http://192.168.1.100/lights_apis/opt/";
  const std::string m_schUrl = "http://192.168.1.100/lights_apis/sch/";
};
