#include "ampswitch.h"

#include <gpiod.h>

AmpSwitch::AmpSwitch(const std::string &chipName) : m_chipName(chipName) {}

std::string AmpSwitch::lastError() const { return m_lastError; }

bool AmpSwitch::setEnabled(bool enabled) {
  m_lastError.clear();

  gpiod_chip *chip = gpiod_chip_open_by_name(m_chipName.c_str());
  if (!chip) {
    m_lastError = "Failed to open gpio chip";
    return false;
  }

  gpiod_line *line = gpiod_chip_get_line(chip, AMP_SWITCH);
  if (!line) {
    m_lastError = "Failed to get AMP_SWITCH line";
    gpiod_chip_close(chip);
    return false;
  }

  if (gpiod_line_request_output(line, "lights-amp-switch", 1) < 0) {
    m_lastError = "Failed to request AMP_SWITCH as output";
    gpiod_chip_close(chip);
    return false;
  }

  // Adjust this if your relay logic is inverted
  const int value = enabled ? 0 : 1;

  if (gpiod_line_set_value(line, value) < 0) {
    m_lastError = "Failed to set AMP_SWITCH value";
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return false;
  }

  gpiod_line_release(line);
  gpiod_chip_close(chip);
  return true;
}
