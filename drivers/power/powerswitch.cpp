#include "powerswitch.h"

#include <chrono>
#include <gpiod.h>
#include <thread>

namespace {

// GPIO 9  (pin 21 / TEENSY_SWITCH): LOW = OFF, HIGH = ON
// GPIO 10 (pin 19 / SR_SWITCH):     HIGH = OFF, LOW = ON

constexpr int teensyValueForEnabled(bool enabled) { return enabled ? 1 : 0; }
constexpr int srValueForEnabled(bool enabled) { return enabled ? 0 : 1; }

} // namespace

PowerSwitch::PowerSwitch(const std::string &chipName) : m_chipName(chipName) {}

std::string PowerSwitch::lastError() const { return m_lastError; }

bool PowerSwitch::setEnabled(bool enabled) {
  m_lastError.clear();

  gpiod_chip *chip = gpiod_chip_open_by_name(m_chipName.c_str());
  if (!chip) {
    m_lastError = "PowerSwitch: failed to open gpio chip";
    return false;
  }

  gpiod_line *teensyLine = gpiod_chip_get_line(chip, TEENSY_SWITCH);
  gpiod_line *srLine = gpiod_chip_get_line(chip, SR_SWITCH);

  if (!teensyLine || !srLine) {
    m_lastError = "PowerSwitch: failed to get GPIO lines";
    gpiod_chip_close(chip);
    return false;
  }

  // Request both lines with their current target state as initial values.
  if (gpiod_line_request_output(teensyLine, "lights-powerswitch",
                                teensyValueForEnabled(false)) < 0) {
    m_lastError = "PowerSwitch: failed to request TEENSY_SWITCH as output";
    gpiod_chip_close(chip);
    return false;
  }

  if (gpiod_line_request_output(srLine, "lights-powerswitch",
                                srValueForEnabled(false)) < 0) {
    m_lastError = "PowerSwitch: failed to request SR_SWITCH as output";
    gpiod_line_release(teensyLine);
    gpiod_chip_close(chip);
    return false;
  }

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

  gpiod_line_release(srLine);
  gpiod_line_release(teensyLine);
  gpiod_chip_close(chip);

  if (rc1 < 0 || rc2 < 0) {
    m_lastError = "PowerSwitch: failed to set GPIO values";
    return false;
  }

  m_signalPowerChanged.emit(enabled);
  return true;
}
