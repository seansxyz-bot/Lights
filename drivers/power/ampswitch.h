#pragma once

#include <string>

class AmpSwitch {
public:
  static constexpr int AMP_SWITCH = 24;

  explicit AmpSwitch(const std::string &chipName = "/dev/gpiochip0");
  bool setEnabled(bool enabled);
  std::string lastError() const;

private:
  std::string m_chipName;
  std::string m_lastError;
};
