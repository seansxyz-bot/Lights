#include "gpiohelper.h"

#include <gpiod.h>
#include <iostream>

GPIOHelper::GPIOHelper(const std::string &chipPath) {
  m_chip = gpiod_chip_open(chipPath.c_str());
  if (!m_chip) {
    std::cerr << "Failed to open GPIO chip: " << chipPath << "\n";
  }
}

GPIOHelper::~GPIOHelper() {
  if (m_chip) {
    gpiod_chip_close(m_chip);
    m_chip = nullptr;
  }
}

bool GPIOHelper::read(int lineNum, bool activeLow) {
  if (!m_chip)
    return false;

  bool result = false;
  unsigned int offset = static_cast<unsigned int>(lineNum);

  gpiod_request_config *reqCfg = gpiod_request_config_new();
  gpiod_line_config *lineCfg = gpiod_line_config_new();
  gpiod_line_settings *settings = gpiod_line_settings_new();
  gpiod_line_request *request = nullptr;

  if (!reqCfg || !lineCfg || !settings) {
    std::cerr << "Failed to allocate libgpiod objects\n";
    goto done;
  }

  gpiod_request_config_set_consumer(reqCfg, "lights-app");
  gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

  if (activeLow)
    gpiod_line_settings_set_active_low(settings, true);

  if (gpiod_line_config_add_line_settings(lineCfg, &offset, 1, settings) < 0) {
    std::cerr << "Failed to add line settings for GPIO " << lineNum << "\n";
    goto done;
  }

  request = gpiod_chip_request_lines(m_chip, reqCfg, lineCfg);
  if (!request) {
    std::cerr << "Failed to request GPIO " << lineNum << "\n";
    goto done;
  }

  {
    const int value = gpiod_line_request_get_value(request, offset);
    if (value < 0) {
      std::cerr << "Failed to read GPIO " << lineNum << "\n";
      goto done;
    }
    result = (value == 1);
  }

done:
  if (request)
    gpiod_line_request_release(request);
  if (settings)
    gpiod_line_settings_free(settings);
  if (lineCfg)
    gpiod_line_config_free(lineCfg);
  if (reqCfg)
    gpiod_request_config_free(reqCfg);

  return result;
}
