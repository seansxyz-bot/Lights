#pragma once

#include "../drivers/i2c/teensyclient.h"
#include "../drivers/network/httphelper.h"
#include "../models/types.h"
#include "../utils/logger.h"
#include "../utils/parserhelper.h"

#include <atomic>
#include <string>
#include <vector>

#if (UBUNTU == 1)
#define HOME_DIR "/home/dev"
#define SETTINGS_PATH "/home/dev/.lightcontroller"
#else
#define HOME_DIR "/home/lights"
#define SETTINGS_PATH "/home/lights/.local/share/lights"
#endif

extern HttpHelper http;
extern std::atomic<bool> writeToServer;

bool ensureCoreSchema(const std::string &dbPath);
bool ensureSportsSchema(const std::string &dbPath);
