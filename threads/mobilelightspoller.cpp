#include "mobilelightspoller.h"

#include <chrono>
namespace {
void normalizeLEDOrder(std::vector<LEDData> &leds) {
  std::sort(leds.begin(), leds.end(), [](const LEDData &a, const LEDData &b) {
    return a.redPin < b.redPin;
  });
}

void normalizeScheduleOrder(std::vector<Schedule> &schedules) {
  std::sort(schedules.begin(), schedules.end(),
            [](const Schedule &a, const Schedule &b) {
              return a.themeID < b.themeID;
            });
}
} // namespace

MobileLightsPoller::MobileLightsPoller(const std::vector<LEDData> &ledInfo,
                                       const Options &options,
                                       const std::vector<Schedule> &schedule)
    : m_ledInfo(ledInfo), m_options(options), m_schedule(schedule) {
  normalizeLEDOrder(m_ledInfo);
  normalizeScheduleOrder(m_schedule);
  m_dispatcher.connect(sigc::mem_fun(*this, &MobileLightsPoller::onDispatch));
}

MobileLightsPoller::~MobileLightsPoller() { stop(); }

void MobileLightsPoller::start() {
  bool expected = false;
  if (!m_running.compare_exchange_strong(expected, true)) {
    return;
  }

  m_thread = std::thread(&MobileLightsPoller::threadMain, this);
}

void MobileLightsPoller::stop() {
  bool expected = true;
  if (!m_running.compare_exchange_strong(expected, false)) {
    return;
  }

  if (m_thread.joinable()) {
    m_thread.join();
  }
}

Options MobileLightsPoller::optionsSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_options;
}

std::vector<LEDData> MobileLightsPoller::ledInfoSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_ledInfo;
}

std::vector<Schedule> MobileLightsPoller::scheduleSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_schedule;
}

sigc::signal<void> &MobileLightsPoller::signal_changed() {
  return m_signalChanged;
}

sigc::signal<void, Options> &MobileLightsPoller::signal_options_changed() {
  return m_signalOptionsChanged;
}

sigc::signal<void, std::vector<LEDData>> &
MobileLightsPoller::signal_leds_changed() {
  return m_signalLEDsChanged;
}

sigc::signal<void, std::vector<Schedule>> &
MobileLightsPoller::signal_schedules_changed() {
  return m_signalSchedulesChanged;
}

void MobileLightsPoller::threadMain() {
  while (m_running.load()) {
    try {
      bool anyChanged = false;

      if (pollOptions()) {
        anyChanged = true;
      }
      if (pollLEDs()) {
        anyChanged = true;
      }
      if (pollSchedules()) {
        anyChanged = true;
      }

      if (anyChanged) {
        m_changedPending.store(true);
        m_dispatcher.emit();
      }
    } catch (const std::exception &e) {
      LOG_ERROR() << "MobileLightsPoller exception: " << e.what();
    } catch (...) {
      LOG_ERROR() << "MobileLightsPoller unknown exception";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

bool MobileLightsPoller::pollOptions() {
  const std::string response = m_http.get(m_optUrl);
  if (response.empty()) {
    return false;
  }

  Options newOptions{};
  if (!parseOptionsJson(response, newOptions)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  if (newOptions != m_options) {
    m_options = newOptions;
    m_optionsChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: options changed";
    return true;
  }

  return false;
}

bool MobileLightsPoller::pollLEDs() {
  const std::string response = m_http.get(m_ledUrl);
  if (response.empty()) {
    return false;
  }

  std::vector<LEDData> newLEDs;
  if (!parseLEDsJson(response, newLEDs)) {
    return false;
  }
  normalizeLEDOrder(newLEDs);

  std::lock_guard<std::mutex> lock(m_mutex);
  if (newLEDs != m_ledInfo) {
    m_ledInfo = newLEDs;
    m_ledsChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: leds changed";
    return true;
  }

  return false;
}

bool MobileLightsPoller::pollSchedules() {
  const std::string response = m_http.get(m_schUrl);
  if (response.empty()) {
    return false;
  }

  std::vector<Schedule> newSchedules;
  if (!parseSchedulesJson(response, newSchedules)) {
    return false;
  }
  normalizeScheduleOrder(newSchedules);

  std::lock_guard<std::mutex> lock(m_mutex);
  if (newSchedules != m_schedule) {
    m_schedule = newSchedules;
    m_schedulesChangedPending.store(true);
    LOG_INFO() << "MobileLightsPoller: schedules changed";
    return true;
  }

  return false;
}

void MobileLightsPoller::onDispatch() {
  const bool emitChanged = m_changedPending.exchange(false);
  const bool emitOptions = m_optionsChangedPending.exchange(false);
  const bool emitLEDs = m_ledsChangedPending.exchange(false);
  const bool emitSchedules = m_schedulesChangedPending.exchange(false);

  Options optionsCopy;
  std::vector<LEDData> ledsCopy;
  std::vector<Schedule> schedulesCopy;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (emitOptions) {
      optionsCopy = m_options;
    }
    if (emitLEDs) {
      ledsCopy = m_ledInfo;
    }
    if (emitSchedules) {
      schedulesCopy = m_schedule;
    }
  }

  if (emitChanged) {
    m_signalChanged.emit();
  }
  if (emitOptions) {
    m_signalOptionsChanged.emit(optionsCopy);
  }
  if (emitLEDs) {
    m_signalLEDsChanged.emit(ledsCopy);
  }
  if (emitSchedules) {
    m_signalSchedulesChanged.emit(schedulesCopy);
  }
}

namespace {
int jsonInt(const json &j, const char *key, int fallback = 0) {
  if (!j.contains(key) || j[key].is_null()) {
    return fallback;
  }

  const auto &v = j[key];

  if (v.is_number_integer()) {
    return v.get<int>();
  }

  if (v.is_number_float()) {
    return static_cast<int>(v.get<double>());
  }

  if (v.is_string()) {
    try {
      return std::stoi(v.get<std::string>());
    } catch (...) {
      return fallback;
    }
  }

  return fallback;
}

float jsonFloat(const json &j, const char *key, float fallback = 0.0f) {
  if (!j.contains(key) || j[key].is_null()) {
    return fallback;
  }

  const auto &v = j[key];

  if (v.is_number()) {
    return v.get<float>();
  }

  if (v.is_string()) {
    try {
      return std::stof(v.get<std::string>());
    } catch (...) {
      return fallback;
    }
  }

  return fallback;
}

std::string jsonString(const json &j, const char *key,
                       const std::string &fallback = "") {
  if (!j.contains(key) || j[key].is_null()) {
    return fallback;
  }

  const auto &v = j[key];

  if (v.is_string()) {
    return v.get<std::string>();
  }

  if (v.is_number_integer()) {
    return std::to_string(v.get<int>());
  }

  if (v.is_number_float()) {
    return std::to_string(v.get<double>());
  }

  return fallback;
}
} // namespace
bool MobileLightsPoller::parseOptionsJson(const std::string &response,
                                          Options &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid options JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("options")) {
    j = j["options"];
  }

  if (j.is_null()) {
    LOG_ERROR() << "MobileLightsPoller: options JSON data is null";
    return false;
  }

  if (!j.is_object()) {
    LOG_ERROR() << "MobileLightsPoller: options JSON not object";
    return false;
  }

  out.sensor = jsonInt(j, "auto", 0);
  out.on = jsonInt(j, "on", 0);
  out.theme = jsonInt(j, "theme", 0);
  out.ptrn = jsonInt(j, "pattern", 0);

  return true;
}

bool MobileLightsPoller::parseLEDsJson(const std::string &response,
                                       std::vector<LEDData> &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid leds JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("leds")) {
    j = j["leds"];
  }

  if (!j.is_array()) {
    LOG_ERROR() << "MobileLightsPoller: leds JSON not array";
    LOG_ERROR() << "MobileLightsPoller leds response: " << response;
    return false;
  }

  std::vector<LEDData> temp;
  for (const auto &item : j) {
    LEDData led;
    led.name = jsonString(item, "led_name", "");
    led.group = jsonInt(item, "led_group", 0);
    led.redPin = jsonInt(item, "pin_red", 0);
    led.grnPin = jsonInt(item, "pin_grn", 0);
    led.bluPin = jsonInt(item, "pin_blu", 0);
    led.redVal = jsonInt(item, "red_value", 0);
    led.grnVal = jsonInt(item, "grn_value", 0);
    led.bluVal = jsonInt(item, "blu_value", 0);
    temp.push_back(led);
  }

  out = std::move(temp);
  return true;
}

bool MobileLightsPoller::parseSchedulesJson(const std::string &response,
                                            std::vector<Schedule> &out) {
  json j = json::parse(response, nullptr, false);
  if (j.is_discarded()) {
    LOG_ERROR() << "MobileLightsPoller: invalid schedules JSON";
    return false;
  }

  if (j.is_object() && j.contains("data")) {
    j = j["data"];
  } else if (j.is_object() && j.contains("schedules")) {
    j = j["schedules"];
  }

  if (!j.is_array()) {
    LOG_ERROR() << "MobileLightsPoller: schedules JSON not array";
    LOG_ERROR() << "MobileLightsPoller schedules response: " << response;
    return false;
  }

  std::vector<Schedule> temp;
  for (const auto &item : j) {
    Schedule sch;
    sch.name = jsonString(item, "theme_name", "");
    sch.themeID = jsonInt(item, "theme_id", 0);
    sch.enabled = jsonInt(item, "theme_enabled", 0);
    sch.sDate = jsonString(item, "start_date", "");
    sch.sTime = jsonString(item, "start_time", "");
    sch.eDate = jsonString(item, "end_date", "");
    sch.eTime = jsonString(item, "end_time", "");
    temp.push_back(sch);
  }

  out = std::move(temp);
  return true;
}
