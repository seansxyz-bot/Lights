#pragma once

#define PIN_SENSOR 17
#define PIN_DOORBELL 22

#include <gpiod.h>
#include <string>

class GPIOHelper {
public:
  GPIOHelper(const std::string &chipName = "gpiochip0");
  ~GPIOHelper();

  bool read(int line, bool activeLow = true);

private:
  gpiod_chip *m_chip = nullptr;
};
