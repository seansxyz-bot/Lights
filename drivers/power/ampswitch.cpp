#include "ampswitch.h"

#include <gpiod.h>

AmpSwitch::AmpSwitch(const std::string &chipName) : m_chipName(chipName) {}

std::string AmpSwitch::lastError() const { return m_lastError; }

bool AmpSwitch::setEnabled(bool enabled) {
  m_lastError.clear();

  gpiod_chip *chip = gpiod_chip_open(m_chipName.c_str());
  if (!chip) {
    m_lastError = "Failed to open gpio chip";
    return false;
  }

  gpiod_request_config *reqCfg = gpiod_request_config_new();
  gpiod_line_config *lineCfg = gpiod_line_config_new();
  gpiod_line_settings *settings = gpiod_line_settings_new();
  gpiod_line_request *request = nullptr;

  if (!reqCfg || !lineCfg || !settings) {
    m_lastError = "Failed to allocate libgpiod objects";
    if (settings)
      gpiod_line_settings_free(settings);
    if (lineCfg)
      gpiod_line_config_free(lineCfg);
    if (reqCfg)
      gpiod_request_config_free(reqCfg);
    gpiod_chip_close(chip);
    return false;
  }

  gpiod_request_config_set_consumer(reqCfg, "lights-amp-switch");
  gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

  const int value = enabled ? 0 : 1;
  const gpiod_line_value outValue =
      value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;

  gpiod_line_settings_set_output_value(settings, outValue);

  unsigned int offset = static_cast<unsigned int>(AMP_SWITCH);

  if (gpiod_line_config_add_line_settings(lineCfg, &offset, 1, settings) < 0) {
    m_lastError = "Failed to configure AMP_SWITCH line";
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineCfg);
    gpiod_request_config_free(reqCfg);
    gpiod_chip_close(chip);
    return false;
  }

  request = gpiod_chip_request_lines(chip, reqCfg, lineCfg);
  if (!request) {
    m_lastError = "Failed to request AMP_SWITCH as output";
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineCfg);
    gpiod_request_config_free(reqCfg);
    gpiod_chip_close(chip);
    return false;
  }

  if (gpiod_line_request_set_value(request, offset, outValue) < 0) {
    m_lastError = "Failed to set AMP_SWITCH value";
    gpiod_line_request_release(request);
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineCfg);
    gpiod_request_config_free(reqCfg);
    gpiod_chip_close(chip);
    return false;
  }

  gpiod_line_request_release(request);
  gpiod_line_settings_free(settings);
  gpiod_line_config_free(lineCfg);
  gpiod_request_config_free(reqCfg);
  gpiod_chip_close(chip);
  return true;
}
