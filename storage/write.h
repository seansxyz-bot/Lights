#pragma once

#include "common.h"

int writeLEDInfo(std::string path, std::vector<LEDData> data);
int writeOptions(std::string path, Options data);
int writeSchedule(std::string path, std::vector<Schedule> data);
int writeThemeColors(const std::string dbPath,
                     const std::vector<Theme> &themes);
int writePatternSpeeds(std::string path, const std::vector<Pattern> &patterns);
int writeTeam(const std::string &dbPath, TeamRecord &team);
int deleteTeam(const std::string &dbPath, int teamId);
int writeTeamAnimations(const std::string &dbPath, int teamId,
                        const std::vector<TeamAnimation> &animations);
int deleteTeamAnimation(const std::string &dbPath, int animationId);
int updateTeamNextGame(const std::string &dbPath, int teamId,
                       const std::string &nextGameUtc,
                       const std::string &gameId,
                       const std::string &checkedUtc);
int updateSportsLiveState(const std::string &dbPath, int teamId,
                          const std::string &gameId,
                          const std::string &gameState, int homeScore,
                          int awayScore, const std::string &pollUtc,
                          bool active);
int updateSportsAnimatedScore(const std::string &dbPath, int teamId,
                              int homeScore, int awayScore);
