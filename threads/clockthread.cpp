#include "clockthread.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

ClockThread &ClockThread::instance() {
  static ClockThread inst;
  return inst;
}

ClockThread::ClockThread() {
  m_dispatcher.connect(
      sigc::mem_fun(*this, &ClockThread::processMainThreadDispatch));

  std::time_t now = std::time(nullptr);
  std::tm local_tm{};
  localtime_r(&now, &local_tm);

  m_dateText = formatDate(local_tm);
  m_timeText = formatTime(local_tm);
  m_lastHour = local_tm.tm_hour;
  m_lastMinute = local_tm.tm_min;
  m_lastYear = local_tm.tm_year + 1900;
}

ClockThread::~ClockThread() { stop(); }

void ClockThread::start() {
  bool expected = false;
  if (!m_running.compare_exchange_strong(expected, true))
    return;

  m_thread = std::thread(&ClockThread::threadLoop, this);
}

void ClockThread::stop() {
  bool expected = true;
  if (!m_running.compare_exchange_strong(expected, false))
    return;

  if (m_thread.joinable())
    m_thread.join();
}

std::string ClockThread::dateText() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_dateText;
}

std::string ClockThread::timeText() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_timeText;
}

void ClockThread::setSchedules(const std::vector<Schedule> &schedules) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_schedules.clear();
  m_schedules.reserve(schedules.size());

  const auto now = std::chrono::system_clock::now();

  for (const auto &s : schedules) {
    RuntimeSchedule rs;
    rs.sched = s;
    rs.active = isScheduleActiveNow(s, now);
    m_schedules.push_back(rs);
  }
}

std::vector<Schedule> ClockThread::schedulesSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<Schedule> out;
  out.reserve(m_schedules.size());

  for (const auto &rs : m_schedules)
    out.push_back(rs.sched);

  return out;
}

bool ClockThread::isScheduleActive(const std::string &name) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto &rs : m_schedules) {
    if (rs.sched.name == name)
      return rs.active;
  }

  return false;
}

std::vector<Schedule> ClockThread::activeSchedulesSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<Schedule> out;
  for (const auto &rs : m_schedules) {
    if (rs.active)
      out.push_back(rs.sched);
  }

  return out;
}

sigc::signal<void> &ClockThread::signal_tick() { return m_signalTick; }

sigc::signal<void, int> &ClockThread::signal_new_hour() {
  return m_signalNewHour;
}

sigc::signal<void, int> &ClockThread::signal_new_minute() {
  return m_signalNewMinute;
}

sigc::signal<void, Schedule> &ClockThread::signal_schedule_started() {
  return m_signalScheduleStarted;
}

sigc::signal<void, Schedule> &ClockThread::signal_schedule_ended() {
  return m_signalScheduleEnded;
}

sigc::signal<void, int> &ClockThread::signal_new_year() {
  return m_signalNewYear;
}

bool ClockThread::parseTime24(const std::string &time24, int &hour,
                              int &minute) {
  if (time24.size() != 5 || time24[2] != ':')
    return false;

  try {
    hour = std::stoi(time24.substr(0, 2));
    minute = std::stoi(time24.substr(3, 2));
  } catch (...) {
    return false;
  }

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
    return false;

  return true;
}

bool ClockThread::parseDateMMDD(const std::string &dateMMDD, int &month,
                                int &day) {
  if (dateMMDD.size() != 5 || dateMMDD[2] != '/')
    return false;

  try {
    month = std::stoi(dateMMDD.substr(0, 2));
    day = std::stoi(dateMMDD.substr(3, 2));
  } catch (...) {
    return false;
  }

  if (month < 1 || month > 12)
    return false;

  if (day < 1 || day > 31)
    return false;

  return true;
}

bool ClockThread::makeDateTimeForYear(
    const std::string &dateMMDD, const std::string &time24, int year,
    std::chrono::system_clock::time_point &out) {
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;

  if (!parseDateMMDD(dateMMDD, month, day))
    return false;

  if (!parseTime24(time24, hour, minute))
    return false;

  std::tm tmv{};
  tmv.tm_year = year - 1900;
  tmv.tm_mon = month - 1;
  tmv.tm_mday = day;
  tmv.tm_hour = hour;
  tmv.tm_min = minute;
  tmv.tm_sec = 0;
  tmv.tm_isdst = -1;

  std::time_t target_t = std::mktime(&tmv);
  if (target_t == -1)
    return false;

  // Verify mktime did not normalize an invalid date into something else.
  std::tm verify{};
  localtime_r(&target_t, &verify);

  if (verify.tm_year != tmv.tm_year || verify.tm_mon != tmv.tm_mon ||
      verify.tm_mday != tmv.tm_mday || verify.tm_hour != tmv.tm_hour ||
      verify.tm_min != tmv.tm_min) {
    return false;
  }

  out = std::chrono::system_clock::from_time_t(target_t);
  return true;
}

bool ClockThread::isScheduleActiveNow(
    const Schedule &sched, const std::chrono::system_clock::time_point &now) {
  if (!sched.enabled)
    return false;

  std::time_t now_t = std::chrono::system_clock::to_time_t(now);
  std::tm now_tm{};
  localtime_r(&now_t, &now_tm);

  const int year = now_tm.tm_year + 1900;

  std::chrono::system_clock::time_point startThisYear;
  std::chrono::system_clock::time_point endThisYear;

  if (!makeDateTimeForYear(sched.sDate, sched.sTime, year, startThisYear))
    return false;

  if (!makeDateTimeForYear(sched.eDate, sched.eTime, year, endThisYear))
    return false;

  // Normal same-year window
  if (startThisYear <= endThisYear) {
    return now >= startThisYear && now <= endThisYear;
  }

  // Cross-year window, e.g. 12/31 -> 01/01
  // Active if we're after the start in the current year
  // OR before the end in the current year.
  return now >= startThisYear || now <= endThisYear;
}

std::string ClockThread::formatDate(const std::tm &tm) {
  std::ostringstream ss;
  ss << std::put_time(&tm, "%m/%d/%Y");
  return ss.str();
}

std::string ClockThread::formatTime(const std::tm &tm) {
  std::ostringstream ss;
  ss << std::put_time(&tm, "%I:%M:%S");
  return ss.str();
}

void ClockThread::threadLoop() {
  using clock = std::chrono::system_clock;
  using namespace std::chrono;

  const auto sleepStep = milliseconds(20);

  while (m_running) {
    const auto now = clock::now();
    const std::time_t now_t = clock::to_time_t(now);

    std::tm local_tm{};
    localtime_r(&now_t, &local_tm);

    const std::string newDate = formatDate(local_tm);
    const std::string newTime = formatTime(local_tm);
    const int currentYear = local_tm.tm_year + 1900;

    bool needDispatch = false;

    {
      std::lock_guard<std::mutex> lock(m_mutex);

      if (local_tm.tm_hour != m_lastHour) {
        m_lastHour = local_tm.tm_hour;

        PendingEmit pe;
        pe.type = PendingEmit::Type::NewHour;
        pe.hour = local_tm.tm_hour;
        m_pendingEmits.push_back(pe);
        needDispatch = true;
      }

      if (local_tm.tm_min != m_lastMinute) {
        m_lastMinute = local_tm.tm_min;

        PendingEmit pe;
        pe.type = PendingEmit::Type::NewMinute;
        pe.minute = local_tm.tm_min;
        m_pendingEmits.push_back(pe);
        needDispatch = true;
      }

      if (newDate != m_dateText || newTime != m_timeText) {
        m_dateText = newDate;
        m_timeText = newTime;

        PendingEmit pe;
        pe.type = PendingEmit::Type::Tick;
        m_pendingEmits.push_back(pe);
        needDispatch = true;
      }

      if (currentYear != m_lastYear) {
        m_lastYear = currentYear;

        PendingEmit pe;
        pe.type = PendingEmit::Type::NewYear;
        pe.year = currentYear;
        m_pendingEmits.push_back(pe);
        needDispatch = true;
      }

      for (auto &rs : m_schedules) {
        const bool nowActive = isScheduleActiveNow(rs.sched, now);

        if (!rs.active && nowActive) {
          rs.active = true;

          PendingEmit pe;
          pe.type = PendingEmit::Type::ScheduleStarted;
          pe.schedule = rs.sched;
          m_pendingEmits.push_back(pe);
          needDispatch = true;
        } else if (rs.active && !nowActive) {
          rs.active = false;

          PendingEmit pe;
          pe.type = PendingEmit::Type::ScheduleEnded;
          pe.schedule = rs.sched;
          m_pendingEmits.push_back(pe);
          needDispatch = true;
        }
      }
    }

    if (needDispatch)
      m_dispatcher.emit();

    std::this_thread::sleep_for(sleepStep);
  }
}

void ClockThread::processMainThreadDispatch() {
  std::vector<PendingEmit> emits;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    emits.swap(m_pendingEmits);
  }

  for (const auto &e : emits) {
    switch (e.type) {
    case PendingEmit::Type::Tick:
      m_signalTick.emit();
      break;

    case PendingEmit::Type::NewHour:
      m_signalNewHour.emit(e.hour);
      break;

    case PendingEmit::Type::NewMinute:
      m_signalNewMinute.emit(e.minute);
      break;

    case PendingEmit::Type::ScheduleStarted:
      m_signalScheduleStarted.emit(e.schedule);
      break;

    case PendingEmit::Type::ScheduleEnded:
      m_signalScheduleEnded.emit(e.schedule);
      break;
    case PendingEmit::Type::NewYear:
      m_signalNewYear.emit(e.year);
      break;
    }
  }
}
