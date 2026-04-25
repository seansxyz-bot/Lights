#pragma once

#include "common.h"

int writeLEDInfo(std::string path, std::vector<LEDData> data);
int writeOptions(std::string path, Options data);
int writeSchedule(std::string path, std::vector<Schedule> data);
int writeThemeColors(const std::string dbPath,
                     const std::vector<Theme> &themes);
int writePatternSpeeds(std::string path, const std::vector<Pattern> &patterns);
