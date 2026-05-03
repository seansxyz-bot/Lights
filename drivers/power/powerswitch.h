#pragma once

#include <sigc++/sigc++.h>
#include <string>

class PowerSwitch {
public:
  static constexpr int SR_SWITCH = 10;    // ON == LOW
  static constexpr int TEENSY_SWITCH = 9; // ON == HIGH

  explicit PowerSwitch(const std::string &chipName = "/dev/gpiochip0",
                       int teensySwitch = TEENSY_SWITCH,
                       int srSwitch = SR_SWITCH);

  bool setEnabled(bool enabled);
  std::string lastError() const;

  sigc::signal<void, bool> &signal_power_changed() {
    return m_signalPowerChanged;
  }

private:
  std::string m_chipName;
  int m_teensySwitch = TEENSY_SWITCH;
  int m_srSwitch = SR_SWITCH;
  std::string m_lastError;
  sigc::signal<void, bool> m_signalPowerChanged;
};
