#pragma once

#include <string>

class AmpSwitch {
public:
  static constexpr int AMP_SWITCH = 24;

  explicit AmpSwitch(const std::string &chipName = "/dev/gpiochip0",
                     int ampSwitch = AMP_SWITCH);
  bool setEnabled(bool enabled);
  std::string lastError() const;

private:
  std::string m_chipName;
  int m_ampSwitch = AMP_SWITCH;
  std::string m_lastError;
};
