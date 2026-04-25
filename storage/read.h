#pragma once

#include "common.h"

std::vector<LEDData> readLEDInfo(std::string path);
Options readOptions(std::string path);
std::vector<Schedule> readSchedule(std::string path);
std::vector<Theme> readThemeColors(const std::string &dbPath);
std::vector<Pattern> readPatternSpeeds(std::string path);
