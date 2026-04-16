#include "gpiohelper.h"
#include <iostream>

GPIOHelper::GPIOHelper(const std::string &chipName) {
  m_chip = gpiod_chip_open_by_name(chipName.c_str());
  if (!m_chip) {
    std::cerr << "Failed to open GPIO chip\n";
  }
}

GPIOHelper::~GPIOHelper() {
  if (m_chip) {
    gpiod_chip_close(m_chip);
  }
}

bool GPIOHelper::read(int lineNum, bool activeLow) {
#ifdef MOCK_HARDWARE
  return false;
#else
  if (!m_chip)
    return false;

  gpiod_line *line = gpiod_chip_get_line(m_chip, lineNum);
  if (!line)
    return false;

  if (gpiod_line_request_input(line, "lights-app") < 0) {
    return false;
  }

  int value = gpiod_line_get_value(line);

  gpiod_line_release(line);

  if (value < 0)
    return false;

  return activeLow ? (value == 0) : (value == 1);
#endif
}
