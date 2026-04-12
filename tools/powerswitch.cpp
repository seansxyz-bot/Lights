#include "powerswitch.h"

#include "logger.h"

#include <chrono>
#include <gpiod.h>

namespace {

// Your hardware logic:
// GPIO 9  (pin 21 / TEENSY_SWITCH): LOW = OFF, HIGH = ON
// GPIO 10 (pin 19 / SR_SWITCH):     HIGH = OFF, LOW = ON

constexpr int teensyValueForEnabled(bool enabled) { return enabled ? 1 : 0; }

constexpr int srValueForEnabled(bool enabled) { return enabled ? 0 : 1; }

} // namespace

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
  Logger::info() << "Power switched to " << (enabled ? "On" : "Off");

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingEnabled = enabled;
    options.on = enabled ? 1 : 0;
    writeOptions(SETTINGS_PATH, options);
  }

  m_cv.notify_one();
}

void PowerSwitch::threadMain() {
  gpiod_chip *chip = gpiod_chip_open_by_name(m_chipName.c_str());
  if (!chip) {
    LOG_ERROR() << "PowerSwitch: failed to open chip " << m_chipName;
    return;
  }

  gpiod_line *teensyLine = gpiod_chip_get_line(chip, TEENSY_SWITCH);
  gpiod_line *srLine = gpiod_chip_get_line(chip, SR_SWITCH);

  if (!teensyLine || !srLine) {
    LOG_ERROR() << "PowerSwitch: failed to get GPIO lines " << TEENSY_SWITCH
                << " and/or " << SR_SWITCH;
    gpiod_chip_close(chip);
    return;
  }

  // Safe startup state = OFF
  if (gpiod_line_request_output(teensyLine, "lights-powerswitch",
                                teensyValueForEnabled(false)) < 0) {
    LOG_ERROR() << "PowerSwitch: failed to request TEENSY_SWITCH as output";
    gpiod_chip_close(chip);
    return;
  }

  if (gpiod_line_request_output(srLine, "lights-powerswitch",
                                srValueForEnabled(false)) < 0) {
    LOG_ERROR() << "PowerSwitch: failed to request SR_SWITCH as output";
    gpiod_line_release(teensyLine);
    gpiod_chip_close(chip);
    return;
  }

  LOG_INFO() << "PowerSwitch: GPIO ready. Safe default = OFF";

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

    const bool enabled = *pending;

    LOG_INFO() << "PowerSwitch: setting state to " << (enabled ? "ON" : "OFF");

    // Order matters a little. On power-up, bring Teensy first, then SR.
    // On power-down, drop SR first, then Teensy.
    int rc1 = 0;
    int rc2 = 0;

    if (enabled) {
      rc1 = gpiod_line_set_value(teensyLine, teensyValueForEnabled(true));
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      rc2 = gpiod_line_set_value(srLine, srValueForEnabled(true));
    } else {
      rc1 = gpiod_line_set_value(srLine, srValueForEnabled(false));
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      rc2 = gpiod_line_set_value(teensyLine, teensyValueForEnabled(false));
    }

    if (rc1 < 0 || rc2 < 0) {
      LOG_ERROR() << "PowerSwitch: failed to set GPIO values";
      continue;
    }

    m_signalPowerChanged.emit(enabled);
  }

  // Safe shutdown state = OFF
  gpiod_line_set_value(srLine, srValueForEnabled(false));
  gpiod_line_set_value(teensyLine, teensyValueForEnabled(false));

  gpiod_line_release(srLine);
  gpiod_line_release(teensyLine);
  gpiod_chip_close(chip);

  LOG_INFO() << "PowerSwitch: stopped and set outputs to OFF";
}
