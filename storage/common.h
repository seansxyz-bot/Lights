#pragma once

#include "../drivers/i2c/teensyclient.h"
#include "../drivers/network/httphelper.h"
#include "../models/types.h"
#include "../utils/logger.h"
#include "../utils/parserhelper.h"

#include <string>
#include <vector>

#if (UBUNTU == 1)
#define HOME_DIR "/home/dev"
#define SETTINGS_PATH "/home/dev/.lightcontroller"
#else
#define HOME_DIR "/home/lights"
#define SETTINGS_PATH "/home/lights/.local/share/lights"
#endif

struct AppConfig {
  std::string settingsPath = SETTINGS_PATH;
  std::string apiBaseUrl = "http://192.168.1.100/lights_api/";
  std::string gpioChip = "/dev/gpiochip0";
  int teensySwitchPin = 9;
  int shiftRegisterSwitchPin = 10;
  int ampSwitchPin = 24;
  std::string i2cBus = "/dev/i2c-1";
  std::string lightShowMonitor;
  std::string bluetoothSink;
  std::string doorbellSink;
};

const AppConfig &appConfig();
const std::string &runtimeSettingsPath();

bool ensureCoreSchema(const std::string &dbPath);
bool ensureSportsSchema(const std::string &dbPath);
