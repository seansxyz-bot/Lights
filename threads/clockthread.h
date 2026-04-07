#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtkmm.h>

// Adjust this include path if your Schedule struct lives somewhere else.
#include "../tools/readerwriter.h"

class ClockThread {
public:
  static ClockThread &instance();

  void start();
  void stop();

  std::string dateText() const;
  std::string timeText() const;

  void setSchedules(const std::vector<Schedule> &schedules);
  std::vector<Schedule> schedulesSnapshot() const;

  bool isScheduleActive(const std::string &name) const;
  std::vector<Schedule> activeSchedulesSnapshot() const;

  sigc::signal<void> &signal_tick();
  sigc::signal<void, int> &signal_new_hour();
  sigc::signal<void, int> &signal_new_minute();
  sigc::signal<void, Schedule> &signal_schedule_started();
  sigc::signal<void, Schedule> &signal_schedule_ended();

private:
  ClockThread();
  ~ClockThread();

  ClockThread(const ClockThread &) = delete;
  ClockThread &operator=(const ClockThread &) = delete;

private:
  struct RuntimeSchedule {
    Schedule sched;
    bool active = false;
  };

  struct PendingEmit {
    enum class Type {
      Tick,
      NewHour,
      NewMinute,
      ScheduleStarted,
      ScheduleEnded
    };

    Type type;
    int hour = -1;
    int minute = -1;
    Schedule schedule;
  };

private:
  void threadLoop();
  void processMainThreadDispatch();

  static bool parseTime24(const std::string &time24, int &hour, int &minute);
  static bool parseDateMMDD(const std::string &dateMMDD, int &month, int &day);

  static bool makeDateTimeForYear(const std::string &dateMMDD,
                                  const std::string &time24, int year,
                                  std::chrono::system_clock::time_point &out);

  static bool
  isScheduleActiveNow(const Schedule &sched,
                      const std::chrono::system_clock::time_point &now);

  static std::string formatDate(const std::tm &tm);
  static std::string formatTime(const std::tm &tm);

private:
  mutable std::mutex m_mutex;
  std::thread m_thread;
  std::atomic<bool> m_running{false};

  std::string m_dateText;
  std::string m_timeText;

  int m_lastHour{-1};
  int m_lastMinute{-1};

  std::vector<RuntimeSchedule> m_schedules;
  std::vector<PendingEmit> m_pendingEmits;

  Glib::Dispatcher m_dispatcher;

  sigc::signal<void> m_signalTick;
  sigc::signal<void, int> m_signalNewHour;
  sigc::signal<void, int> m_signalNewMinute;
  sigc::signal<void, Schedule> m_signalScheduleStarted;
  sigc::signal<void, Schedule> m_signalScheduleEnded;
};
