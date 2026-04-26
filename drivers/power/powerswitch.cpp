#include "powerswitch.h"

#include "iostream"
#include <gpiod.h>

namespace {
gpiod_line_value toLineValue(int v) {
  return v ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
}

// Adjust these if your hardware logic is inverted.
int teensyValueForEnabled(bool enabled) { return enabled ? 1 : 0; }

int srValueForEnabled(bool enabled) { return enabled ? 0 : 1; }
} // namespace

PowerSwitch::PowerSwitch(const std::string &chipName) : m_chipName(chipName) {}

std::string PowerSwitch::lastError() const { return m_lastError; }

bool PowerSwitch::setEnabled(bool enabled) {
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

  gpiod_request_config_set_consumer(reqCfg, "lights-powerswitch");
  gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

  unsigned int offsets[2] = {
      static_cast<unsigned int>(TEENSY_SWITCH),
      static_cast<unsigned int>(SR_SWITCH),
  };

  if (gpiod_line_config_add_line_settings(lineCfg, offsets, 2, settings) < 0) {
    m_lastError = "Failed to configure power switch lines";
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineCfg);
    gpiod_request_config_free(reqCfg);
    gpiod_chip_close(chip);
    return false;
  }

  request = gpiod_chip_request_lines(chip, reqCfg, lineCfg);
  if (!request) {
    m_lastError = "Failed to request power switch lines";
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineCfg);
    gpiod_request_config_free(reqCfg);
    gpiod_chip_close(chip);
    return false;
  }

  int rc1 = 0;
  int rc2 = 0;

  if (enabled) {
    rc1 = gpiod_line_request_set_value(
        request, offsets[0], toLineValue(teensyValueForEnabled(true)));
    rc2 = gpiod_line_request_set_value(request, offsets[1],
                                       toLineValue(srValueForEnabled(true)));
  } else {
    rc1 = gpiod_line_request_set_value(request, offsets[1],
                                       toLineValue(srValueForEnabled(false)));
    rc2 = gpiod_line_request_set_value(
        request, offsets[0], toLineValue(teensyValueForEnabled(false)));
  }

  if (rc1 < 0 || rc2 < 0) {
    m_lastError = "Failed to set power switch GPIO values";
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
