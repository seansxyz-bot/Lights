#include "powerswitch.h"

#include "logger.h"

#include <chrono>

PowerSwitch::PowerSwitch(const std::string &chipName) : m_chipName(chipName) {}

PowerSwitch::~PowerSwitch() { stop(); }

void PowerSwitch::start() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_running)
    return;

  m_stopRequested = false;
  m_running = true;
  m_thread = std::thread(&PowerSwitch::threadMain, this);
}

void PowerSwitch::stop() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running)
      return;

    m_stopRequested = true;
  }

  m_cv.notify_one();

  if (m_thread.joinable())
    m_thread.join();

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = false;
    m_pendingEnabled.reset();
  }
}

void PowerSwitch::setEnabled(Options &options, bool enabled) {
  Logger::info() << "Power Switched to " << (enabled ? "On" : "off");
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingEnabled = enabled;
    options.on = enabled;
    writeOptions(SETTINGS_PATH, options);
  }

  m_cv.notify_one();
}

void PowerSwitch::threadMain() {
  PowerSwitch power(m_chipName);

  while (true) {
    std::optional<bool> pending;

    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_cv.wait(lock, [this]() {
        return m_stopRequested || m_pendingEnabled.has_value();
      });

      if (m_stopRequested)
        break;

      pending = m_pendingEnabled;
      m_pendingEnabled.reset();
    }

    if (!pending.has_value())
      continue;

    LOG_INFO() << "PowerSwitch: state set to " << (*pending ? "ON" : "OFF");

    if (*pending) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}
