#pragma once

#include "common.h"

std::vector<LEDData> readLEDInfo(std::string path);
Options readOptions(std::string path);
std::vector<Schedule> readSchedule(std::string path);
std::vector<Theme> readThemeColors(const std::string &dbPath);
std::vector<Pattern> readPatternSpeeds(std::string path);
std::vector<TeamRecord> readTeams(const std::string &dbPath);
std::vector<TeamAnimation> readTeamAnimations(const std::string &dbPath,
                                              int teamId);
std::vector<TeamAnimation>
readTeamAnimationsByType(const std::string &dbPath, int teamId,
                         const std::string &animationType);
