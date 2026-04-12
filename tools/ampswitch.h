#pragma once
#define AMP_SWITCH 24

#include <string>

class AmpSwitch {
public:
  explicit AmpSwitch(const std::string &chipName = "gpiochip0");
  bool setEnabled(bool enabled);
  std::string lastError() const;

private:
  std::string m_chipName;
  std::string m_lastError;
};
