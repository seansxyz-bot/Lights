#include "sportsschedules.h"

#include <chrono>

SportsSchedules::SportsSchedules() = default;

std::tm SportsSchedules::convertToLocalTime(const std::string &utcTime) const {
  std::tm tm{};
  std::istringstream ss(utcTime);

  // Parse ISO-8601 UTC / Zulu time, ex: 2025-09-22T00:00:00Z
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

  if (ss.fail()) {
    ss.clear();
    ss.str(utcTime);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  }

  time_t utc = timegm(&tm);
  std::tm *local = std::localtime(&utc);

  if (local)
    return *local;

  return std::tm{};
}

void SportsSchedules::formatTime(const std::string &time, GameInfo &gi) const {
  gi.dateTimeUTC = time;

  std::tm local = convertToLocalTime(time);
  char buffer[64];

  std::strftime(buffer, sizeof(buffer), "%H:%M", &local);
  gi.militaryTime = buffer;

  std::strftime(buffer, sizeof(buffer), "%I:%M %p", &local);
  gi.standardTime = buffer;

  std::strftime(buffer, sizeof(buffer), "%m-%d @ %I:%M %p", &local);
  gi.displayedDateTime = buffer;

  std::strftime(buffer, sizeof(buffer), "%m/%d", &local);
  gi.scheduledDate = buffer;
}

GameInfo SportsSchedules::getNextGame(int team) {
  std::string url;
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm *tm = std::localtime(&t);

  std::ostringstream ss;
  ss << std::put_time(tm, "%Y-%m-%d");
  std::string date = ss.str();

  switch (team) {
  case THE_KRAKEN: {
    url = "https://api-web.nhle.com/v1/club-schedule/SEA/week/" + date;

    std::string response = m_http.get(url);
    GameInfo out{};

    auto j = json::parse(response, nullptr, false);
    if (j.is_discarded() || !j.contains("games"))
      return out;

    for (auto &game : j["games"]) {
      std::string time = game["startTimeUTC"].get<std::string>();

      if (onOrAfterToday(time) &&
          game["gameState"].get<std::string>() != "OFF") {
        GameInfo info{};
        formatTime(time, info);
        info.id = game["id"];
        info.gameState = game["gameState"];
        info.away = game["awayTeam"]["commonName"]["default"];
        info.home = game["homeTeam"]["commonName"]["default"];
        return info;
      }
    }

    return out;
  }

  case THE_MARINERS: {
    url = "https://statsapi.mlb.com/api/v1/schedule"
          "?hydrate=team,lineups&sportId=1&startDate=" +
          date + "&endDate=" + date + "&teamId=136";

    std::string response = m_http.get(url);
    GameInfo out{};

    auto j = json::parse(response, nullptr, false);
    if (j.is_discarded() || !j.contains("dates"))
      return out;

    for (auto &dateEntry : j["dates"]) {
      if (!dateEntry.contains("games"))
        continue;

      for (auto &game : dateEntry["games"]) {
        if (!game.contains("gameDate"))
          continue;

        std::string time = game["gameDate"].get<std::string>();

        GameInfo info{};
        formatTime(time, info);
        info.id = game["gamePk"];

        if (game.contains("status") &&
            game["status"].contains("abstractGameState"))
          info.gameState = game["status"]["abstractGameState"];

        if (game.contains("teams")) {
          if (game["teams"].contains("away") &&
              game["teams"]["away"].contains("team") &&
              game["teams"]["away"]["team"].contains("name")) {
            info.away = game["teams"]["away"]["team"]["name"];
          }

          if (game["teams"].contains("home") &&
              game["teams"]["home"].contains("team") &&
              game["teams"]["home"]["team"].contains("name")) {
            info.home = game["teams"]["home"]["team"]["name"];
          }
        }

        if (onOrAfterToday(time))
          return info;
      }
    }

    return out;
  }

  case THE_SEAHAWKS: {
    url =
        "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard";

    std::string response = m_http.get(url);
    GameInfo out{};

    auto j = json::parse(response, nullptr, false);
    if (j.is_discarded() || !j.contains("events"))
      return out;

    for (auto &event : j["events"]) {
      if (!event.contains("date"))
        continue;

      std::string time = event["date"].get<std::string>();

      if (!onOrAfterToday(time))
        continue;

      if (!event.contains("competitions") || event["competitions"].empty())
        continue;

      auto &comp = event["competitions"][0];
      if (!comp.contains("competitors"))
        continue;

      GameInfo info{};
      formatTime(time, info);

      if (event.contains("id"))
        info.id = std::stoi(event["id"].get<std::string>());

      if (event.contains("status") && event["status"].contains("type") &&
          event["status"]["type"].contains("description")) {
        info.gameState = event["status"]["type"]["description"];
      }

      for (auto &teamEntry : comp["competitors"]) {
        if (!teamEntry.contains("team") ||
            !teamEntry["team"].contains("displayName"))
          continue;

        std::string teamName = teamEntry["team"]["displayName"];
        std::string homeAway = teamEntry.value("homeAway", "");

        if (homeAway == "home")
          info.home = teamName;
        else if (homeAway == "away")
          info.away = teamName;
      }

      return info;
    }

    return out;
  }
  }

  return GameInfo{};
}

bool SportsSchedules::onOrAfterToday(const std::string &utcTime) const {
  std::tm local = convertToLocalTime(utcTime);

  std::time_t now = std::time(nullptr);
  std::tm today = *std::localtime(&now);

  return local.tm_year > today.tm_year ||
         (local.tm_year == today.tm_year && local.tm_yday >= today.tm_yday);
}
