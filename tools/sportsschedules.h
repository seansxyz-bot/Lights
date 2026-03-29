#pragma once

#include "httphelper.h"
#include "settingsrw.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#define KRAKEN_URL "140082"
#define SEAHAWKS_URL                                                           \
  "https://www.thesportsdb.com/api/v1/json/123/eventsnext.php?id=134920"
#define MARINERS_URL                                                           \
  "https://www.thesportsdb.com/api/v1/json/123/eventsnext.php?id=135268"

#define THE_KRAKEN 0
#define THE_MARINERS 1
#define THE_SEAHAWKS 2

using json = nlohmann::json;

class SportsSchedules {
public:
  SportsSchedules();

  std::tm convertToLocalTime(const std::string &utcTime) const;
  void formatTime(const std::string &time, GameInfo &gi) const;
  GameInfo getNextGame(int team);

private:
  bool onOrAfterToday(const std::string &utcTime) const;

private:
  HttpHelper m_http;
};
