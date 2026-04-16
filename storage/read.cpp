#include "read.h"

#include <fstream>
#include <iostream>
#include <sqlite3.h>

std::vector<LEDData> readLEDInfo(std::string path) {
  const std::string dbPath = path + "/lights.db";
  std::vector<LEDData> data;

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);
    return data;
  }

  const char *sql = R"(
    SELECT name, led_group, red_pin, red_value,
           grn_pin, grn_value, blu_pin, blu_value
    FROM led_info
    ORDER BY red_pin;
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare led_info query: " << sqlite3_errmsg(db)
              << "\n";
    sqlite3_close(db);
    return data;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    LEDData d;

    const unsigned char *nameText = sqlite3_column_text(stmt, 0);
    d.name = nameText ? reinterpret_cast<const char *>(nameText) : "";
    d.group = sqlite3_column_int(stmt, 1);
    d.redPin = sqlite3_column_int(stmt, 2);
    d.redVal = sqlite3_column_int(stmt, 3);
    d.grnPin = sqlite3_column_int(stmt, 4);
    d.grnVal = sqlite3_column_int(stmt, 5);
    d.bluPin = sqlite3_column_int(stmt, 6);
    d.bluVal = sqlite3_column_int(stmt, 7);

    data.push_back(d);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (writeToServer) {
    http.sendLEDsAsync("http://192.168.1.100/lights_apis/sync.php", data,
                       DEVICE);
  }

  return data;
}

Options readOptions(std::string path) {
  const std::string dbPath = path + "/lights.db";
  Options opts{};

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);
    return opts;
  }

  const char *sql = "SELECT name, value FROM options;";
  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *nameText = sqlite3_column_text(stmt, 0);
      const int value = sqlite3_column_int(stmt, 1);

      if (!nameText)
        continue;

      const std::string name(reinterpret_cast<const char *>(nameText));

      if (name == "auto")
        opts.sensor = value;
      else if (name == "on")
        opts.on = value;
      else if (name == "theme")
        opts.theme = value;
      else if (name == "ptrn")
        opts.ptrn = value;
    }
  } else {
    std::cerr << "Failed to prepare options query: " << sqlite3_errmsg(db)
              << "\n";
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (writeToServer) {
    http.sendOptionsAsync("http://192.168.1.100/lights_apis/sync.php", opts,
                          DEVICE);
  }

  return opts;
}

std::vector<Schedule> readSchedule(std::string path) {
  std::vector<Schedule> data;
  const std::string dbPath = path + "/lights.db";

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);
    return data;
  }

  const char *sql = "SELECT theme_id, name, start_date, start_time, end_date, "
                    "end_time, enabled "
                    "FROM schedules "
                    "ORDER BY theme_id;";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      Schedule s;

      s.themeID = sqlite3_column_int(stmt, 0);

      const unsigned char *nameText = sqlite3_column_text(stmt, 1);
      const unsigned char *sDateText = sqlite3_column_text(stmt, 2);
      const unsigned char *sTimeText = sqlite3_column_text(stmt, 3);
      const unsigned char *eDateText = sqlite3_column_text(stmt, 4);
      const unsigned char *eTimeText = sqlite3_column_text(stmt, 5);

      s.name = nameText ? reinterpret_cast<const char *>(nameText) : "";
      s.sDate = sDateText ? reinterpret_cast<const char *>(sDateText) : "";
      s.sTime = sTimeText ? reinterpret_cast<const char *>(sTimeText) : "";
      s.eDate = eDateText ? reinterpret_cast<const char *>(eDateText) : "";
      s.eTime = eTimeText ? reinterpret_cast<const char *>(eTimeText) : "";
      s.enabled = sqlite3_column_int(stmt, 6);

      data.push_back(s);
    }
  } else {
    std::cerr << "Failed to prepare schedules query: " << sqlite3_errmsg(db)
              << "\n";
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return data;
}

std::vector<Theme> readThemeColors(const std::string &path) {
  const std::string dbPath = path + "/lights.db";
  std::vector<Theme> out;

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);
    return out;
  }

  const char *sql = R"(
    SELECT theme_id, name, color_index, r, g, b
    FROM themes
    ORDER BY theme_id, color_index;
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare theme query: " << sqlite3_errmsg(db)
              << "\n";
    sqlite3_close(db);
    return out;
  }

  int currentThemeId = -1;
  Theme *currentTheme = nullptr;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const int themeId = sqlite3_column_int(stmt, 0);

    if (themeId != currentThemeId) {
      Theme t;
      t.id = themeId;

      const unsigned char *nameText = sqlite3_column_text(stmt, 1);
      t.name = nameText ? reinterpret_cast<const char *>(nameText) : "";

      out.push_back(t);
      currentTheme = &out.back();
      currentThemeId = themeId;
    }

    if (currentTheme) {
      RGB_Color c;
      c.r = static_cast<uint8_t>(sqlite3_column_int(stmt, 3));
      c.g = static_cast<uint8_t>(sqlite3_column_int(stmt, 4));
      c.b = static_cast<uint8_t>(sqlite3_column_int(stmt, 5));
      currentTheme->colors.push_back(c);
    }
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return out;
}

std::vector<TeamRecord> readTeams(const std::string &dbPath) {
  std::vector<TeamRecord> teams;
  sqlite3 *db = nullptr;

  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return teams;
  }

  const char *sql =
      "SELECT id, name, league, team_code, "
      "next_game_url_template, next_game_parser, "
      "live_game_url_template, live_game_parser, "
      "api_team_id, enabled, display_order, theme_name, next_game_utc "
      "FROM teams "
      "ORDER BY display_order, name;";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      TeamRecord t;

      t.id = sqlite3_column_int(stmt, 0);

      const unsigned char *nameText = sqlite3_column_text(stmt, 1);
      const unsigned char *leagueText = sqlite3_column_text(stmt, 2);
      const unsigned char *teamCodeText = sqlite3_column_text(stmt, 3);
      const unsigned char *nextGameUrlTemplateText =
          sqlite3_column_text(stmt, 4);
      const unsigned char *nextGameParserText = sqlite3_column_text(stmt, 5);
      const unsigned char *liveGameUrlTemplateText =
          sqlite3_column_text(stmt, 6);
      const unsigned char *liveGameParserText = sqlite3_column_text(stmt, 7);
      const unsigned char *apiTeamIdText = sqlite3_column_text(stmt, 8);
      const unsigned char *themeNameText = sqlite3_column_text(stmt, 11);
      const unsigned char *nextGameUtcText = sqlite3_column_text(stmt, 12);

      t.name = nameText ? reinterpret_cast<const char *>(nameText) : "";
      t.league = leagueText ? reinterpret_cast<const char *>(leagueText) : "";
      t.teamCode =
          teamCodeText ? reinterpret_cast<const char *>(teamCodeText) : "";
      t.nextGameUrlTemplate =
          nextGameUrlTemplateText
              ? reinterpret_cast<const char *>(nextGameUrlTemplateText)
              : "";
      t.nextGameParser =
          nextGameParserText
              ? reinterpret_cast<const char *>(nextGameParserText)
              : "";
      t.liveGameUrlTemplate =
          liveGameUrlTemplateText
              ? reinterpret_cast<const char *>(liveGameUrlTemplateText)
              : "";
      t.liveGameParser =
          liveGameParserText
              ? reinterpret_cast<const char *>(liveGameParserText)
              : "";
      t.apiTeamId =
          apiTeamIdText ? reinterpret_cast<const char *>(apiTeamIdText) : "";
      t.enabled = sqlite3_column_int(stmt, 9);
      t.displayOrder = sqlite3_column_int(stmt, 10);
      t.themeName =
          themeNameText ? reinterpret_cast<const char *>(themeNameText) : "";
      t.nextGameUtc = nextGameUtcText
                          ? reinterpret_cast<const char *>(nextGameUtcText)
                          : "";

      teams.push_back(t);
    }
  }

  if (stmt)
    sqlite3_finalize(stmt);
  sqlite3_close(db);
  return teams;
}

bool loadLedRestoreState(const std::string &dbPath,
                         std::vector<LEDData> &leds) {
  sqlite3 *db = nullptr;

  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    LOG_ERROR() << "loadLedRestoreState: failed to open DB";
    return false;
  }

  const char *sql = "SELECT led_index, red, green, blue "
                    "FROM led_restore "
                    "ORDER BY led_index;";

  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR() << "loadLedRestoreState: prepare failed";
    sqlite3_close(db);
    return false;
  }

  size_t count = 0;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const int idx = sqlite3_column_int(stmt, 0);
    const int r = sqlite3_column_int(stmt, 1);
    const int g = sqlite3_column_int(stmt, 2);
    const int b = sqlite3_column_int(stmt, 3);

    if (idx >= 0 && static_cast<size_t>(idx) < leds.size()) {
      leds[idx].redVal = r;
      leds[idx].grnVal = g;
      leds[idx].bluVal = b;
      ++count;
    }
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (count == 0) {
    LOG_WARN() << "loadLedRestoreState: no data found";
    return false;
  }

  LOG_INFO() << "Loaded LED restore state (" << count << " LEDs)";
  return true;
}
