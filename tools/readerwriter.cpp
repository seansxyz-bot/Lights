#include "readerwriter.h"
#include <algorithm>
#include <sqlite3.h>

HttpHelper http;
std::atomic<bool> writeToServer{true};

std::vector<LEDData> readLEDInfo(const std::string path) {
  const std::string dbPath = path + "/lights.db";
  std::vector<LEDData> data;

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);
    return data;
  }

  const char *countSql = "SELECT COUNT(*) FROM led_info;";
  sqlite3_stmt *countStmt = nullptr;

  if (sqlite3_prepare_v2(db, countSql, -1, &countStmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(countStmt) == SQLITE_ROW) {
    }
  }
  sqlite3_finalize(countStmt);

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

int writeLEDInfo(const std::string path, const std::vector<LEDData> data) {
  const std::string dbPath = path + "/lights.db";

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);
    return 0;
  }

  char *errMsg = nullptr;
  if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) !=
      SQLITE_OK) {
    std::cerr << "Failed to begin transaction: "
              << (errMsg ? errMsg : "unknown error") << "\n";
    sqlite3_free(errMsg);
    sqlite3_close(db);
    return 0;
  }

  if (sqlite3_exec(db, "DELETE FROM led_info;", nullptr, nullptr, &errMsg) !=
      SQLITE_OK) {
    std::cerr << "Failed to clear led_info: "
              << (errMsg ? errMsg : "unknown error") << "\n";
    sqlite3_free(errMsg);
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return 0;
  }

  const char *sql = R"(
    INSERT INTO led_info
    (name, led_group, red_pin, red_value, grn_pin, grn_value, blu_pin, blu_value)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?);
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare led_info insert: " << sqlite3_errmsg(db)
              << "\n";
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return 0;
  }

  bool ok = true;

  for (const auto &d : data) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    sqlite3_bind_text(stmt, 1, d.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, d.group);
    sqlite3_bind_int(stmt, 3, d.redPin);
    sqlite3_bind_int(stmt, 4, d.redVal);
    sqlite3_bind_int(stmt, 5, d.grnPin);
    sqlite3_bind_int(stmt, 6, d.grnVal);
    sqlite3_bind_int(stmt, 7, d.bluPin);
    sqlite3_bind_int(stmt, 8, d.bluVal);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::cerr << "Failed writing LED row '" << d.name
                << "': " << sqlite3_errmsg(db) << "\n";
      ok = false;
      break;
    }
  }

  sqlite3_finalize(stmt);

  if (ok) {
    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
      std::cerr << "Failed to commit led_info write: "
                << (errMsg ? errMsg : "unknown error") << "\n";
      sqlite3_free(errMsg);
      ok = false;
    }
  } else {
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
  }

  sqlite3_close(db);

  if (ok && writeToServer) {
    http.sendLEDsAsync("http://192.168.1.100/lights_apis/sync.php", data,
                       DEVICE);
  }

  return ok ? 1 : 0;
}

Options readOptions(const std::string path) {
  const std::string dbPath = path + "/lights.db";
  Options opts{};
  opts.sensor = 1;
  opts.on = 1;
  opts.theme = 0;
  opts.ptrn = 0;

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);

    if (writeToServer) {
      http.sendOptionsAsync("http://192.168.1.100/lights_apis/sync.php", opts,
                            DEVICE);
    }
    return opts;
  }

  const char *sql = "SELECT name, value FROM options;";
  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const unsigned char *nameText = sqlite3_column_text(stmt, 0);
      int value = sqlite3_column_int(stmt, 1);

      if (!nameText)
        continue;

      std::string name(reinterpret_cast<const char *>(nameText));

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

int writeOptions(const std::string path, const Options data) {
  const std::string dbPath = path + "/lights.db";
  sqlite3 *db = nullptr;

  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);
    return 0;
  }

  const char *sql = R"(
    INSERT INTO options (name, value)
    VALUES (?, ?)
    ON CONFLICT(name) DO UPDATE SET value = excluded.value;
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare options upsert: " << sqlite3_errmsg(db)
              << "\n";
    sqlite3_close(db);
    return 0;
  }

  auto writeOne = [&](const std::string &name, int value) -> bool {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, value);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::cerr << "Failed writing option '" << name
                << "': " << sqlite3_errmsg(db) << "\n";
      return false;
    }
    return true;
  };

  bool ok = true;
  ok &= writeOne("auto", data.sensor);
  ok &= writeOne("on", data.on);
  ok &= writeOne("theme", data.theme);
  ok &= writeOne("ptrn", data.ptrn);

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (ok && writeToServer) {
    http.sendOptionsAsync("http://192.168.1.100/lights_apis/sync.php", data,
                          DEVICE);
  }

  return ok ? 1 : 0;
}

std::vector<Schedule> readSchedule(const std::string path1) {
  std::vector<Schedule> data;
  const std::string dbPath = path1 + "/lights.db";

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << std::endl;
    if (db)
      sqlite3_close(db);
    return data;
  }

  const char *sql = "SELECT theme_id,      name,      start_date,      "
                    "start_time,      end_date,      end_time,      enabled"
                    " FROM schedules ORDER BY theme_id;";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      Schedule s;
      s.themeID = sqlite3_column_int(stmt, 0);
      s.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
      s.sDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
      s.sTime = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
      s.eDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
      s.eTime = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
      s.enabled = sqlite3_column_int(stmt, 6);
      data.push_back(s);
    }
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return data;
}

int writeSchedule(const std::string path, const std::vector<Schedule> data) {
  const std::string dbPath = path + "/lights.db";
  std::cout << dbPath << std::endl;
  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << std::endl;
    if (db)
      sqlite3_close(db);
    return 0;
  }

  sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
  sqlite3_exec(db, "DELETE FROM schedules;", nullptr, nullptr, nullptr);

  const char *sql =
      "INSERT INTO schedules "
      "(theme_id, name, start_date, start_time, end_date, end_time, enabled) "
      "VALUES (?, ?, ?, ?, ?, ?, ?);";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  for (const auto &s : data) {
    sqlite3_reset(stmt);
    sqlite3_bind_int(stmt, 1, s.themeID);
    sqlite3_bind_text(stmt, 2, s.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s.sDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, s.sTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, s.eDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, s.eTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, s.enabled);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
      sqlite3_close(db);
      return 0;
    }
    sqlite3_clear_bindings(stmt);
  }

  sqlite3_finalize(stmt);
  sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
  sqlite3_close(db);
  return 1;
}

GameInfo readNextGame(std::string path, std::string fileName) {
  std::string fullPath = path + fileName;
  // Attempt to open the file for reading
  std::ifstream file(fullPath);
  if (!file.is_open()) {
    GameInfo bad;
    bad.id = -1;
    std::cerr << "Failed to open the file." << std::endl;
    return bad;
  }
  GameInfo d;
  file >> d.id >> d.gameState >> d.home >> d.away >> d.militaryTime >>
      d.standardTime >> d.displayedDateTime >> d.dateTimeUTC >>
      d.scheduledDate >> d.scoreHome >> d.scoreAway;

  std::replace(d.home.begin(), d.home.end(), '_', ' ');
  std::replace(d.away.begin(), d.away.end(), '_', ' ');
  std::replace(d.standardTime.begin(), d.standardTime.end(), '_', ' ');
  std::replace(d.displayedDateTime.begin(), d.displayedDateTime.end(), '_',
               ' ');
  std::replace(d.dateTimeUTC.begin(), d.dateTimeUTC.end(), '_', ' ');
  std::replace(d.scheduledDate.begin(), d.scheduledDate.end(), '_', ' ');

  return d;
}

int writeNextGame(std::string path, std::string fileName, GameInfo d) {
  std::string fullPath = path + fileName;
  std::cout << "Writing " << fullPath << std::endl;
  std::ofstream file(fullPath);
  int success = 0;
  if (file.is_open()) {

    std::replace(d.home.begin(), d.home.end(), ' ', '_');
    std::replace(d.away.begin(), d.away.end(), ' ', '_');
    std::replace(d.standardTime.begin(), d.standardTime.end(), ' ', '_');
    std::replace(d.displayedDateTime.begin(), d.displayedDateTime.end(), ' ',
                 '_');
    std::replace(d.dateTimeUTC.begin(), d.dateTimeUTC.end(), ' ', '_');
    std::replace(d.scheduledDate.begin(), d.scheduledDate.end(), ' ', '_');

    file << d.id << "\t" << d.gameState << "\t" << d.home << "\t" << d.away
         << "\t" << d.militaryTime << "\t" << d.standardTime << "\t"
         << d.displayedDateTime << "\t" << d.dateTimeUTC << "\t"
         << d.scheduledDate << "\t" << d.scoreHome << "\t" << d.scoreAway;
    success = 1;
  }
  file.close();
  return success;
}

static bool execSQL(sqlite3 *db, const char *sql) {
  char *errMsg = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQLite error: " << (errMsg ? errMsg : "unknown") << "\n";
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

std::vector<Theme> readThemeColors(const std::string &path) {
  const std::string dbPath = path + "/lights.db";
  std::cout << dbPath << std::endl;

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
    ORDER BY theme_id, color_index
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

  for (auto &theme : out) {
    if (theme.colors.empty()) {
      theme.colors.push_back({255, 255, 255});
    }
  }

  return out;
}

int writeThemeColors(const std::string &path,
                     const std::vector<Theme> &themes) {
  const std::string dbPath = path + "/lights.db";
  sqlite3 *db = nullptr;

  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Failed to open DB: " << dbPath << "\n";
    if (db)
      sqlite3_close(db);
    return 0;
  }

  if (!execSQL(db, "BEGIN TRANSACTION;")) {
    sqlite3_close(db);
    return 0;
  }

  if (!execSQL(db, "DELETE FROM themes;")) {
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  const char *insertSql = R"(
    INSERT INTO themes (theme_id, name, color_index, r, g, b)
    VALUES (?, ?, ?, ?, ?, ?)
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare insert statement: " << sqlite3_errmsg(db)
              << "\n";
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  for (const auto &theme : themes) {
    if (theme.colors.empty()) {
      sqlite3_reset(stmt);
      sqlite3_clear_bindings(stmt);

      sqlite3_bind_int(stmt, 1, theme.id);
      sqlite3_bind_text(stmt, 2, theme.name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 3, 0);
      sqlite3_bind_int(stmt, 4, 255);
      sqlite3_bind_int(stmt, 5, 255);
      sqlite3_bind_int(stmt, 6, 255);

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed inserting default color for theme: " << theme.name
                  << " error: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(stmt);
        execSQL(db, "ROLLBACK;");
        sqlite3_close(db);
        return 0;
      }

      continue;
    }

    for (int i = 0; i < static_cast<int>(theme.colors.size()); ++i) {
      const auto &c = theme.colors[i];

      sqlite3_reset(stmt);
      sqlite3_clear_bindings(stmt);

      sqlite3_bind_int(stmt, 1, theme.id);
      sqlite3_bind_text(stmt, 2, theme.name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 3, i);
      sqlite3_bind_int(stmt, 4, c.r);
      sqlite3_bind_int(stmt, 5, c.g);
      sqlite3_bind_int(stmt, 6, c.b);

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed inserting color for theme: " << theme.name
                  << " error: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(stmt);
        execSQL(db, "ROLLBACK;");
        sqlite3_close(db);
        return 0;
      }
    }
  }

  sqlite3_finalize(stmt);

  if (!execSQL(db, "COMMIT;")) {
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  sqlite3_close(db);
  return 1;
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
      "FROM teams ORDER BY display_order, name;";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      TeamRecord t;
      t.id = sqlite3_column_int(stmt, 0);
      t.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)
                                                  ? sqlite3_column_text(stmt, 1)
                                                  : (const unsigned char *)"");
      t.league = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 2) ? sqlite3_column_text(stmt, 2)
                                       : (const unsigned char *)"");
      t.teamCode = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 3) ? sqlite3_column_text(stmt, 3)
                                       : (const unsigned char *)"");
      t.nextGameUrlTemplate = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 4) ? sqlite3_column_text(stmt, 4)
                                       : (const unsigned char *)"");
      t.nextGameParser = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 5) ? sqlite3_column_text(stmt, 5)
                                       : (const unsigned char *)"");
      t.liveGameUrlTemplate = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 6) ? sqlite3_column_text(stmt, 6)
                                       : (const unsigned char *)"");
      t.liveGameParser = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 7) ? sqlite3_column_text(stmt, 7)
                                       : (const unsigned char *)"");
      t.apiTeamId = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 8) ? sqlite3_column_text(stmt, 8)
                                       : (const unsigned char *)"");
      t.enabled = sqlite3_column_int(stmt, 9);
      t.displayOrder = sqlite3_column_int(stmt, 10);
      t.themeName = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 11) ? sqlite3_column_text(stmt, 11)
                                        : (const unsigned char *)"");
      t.nextGameUtc = reinterpret_cast<const char *>(
          sqlite3_column_text(stmt, 12) ? sqlite3_column_text(stmt, 12)
                                        : (const unsigned char *)"");
      teams.push_back(t);
    }
  }

  if (stmt)
    sqlite3_finalize(stmt);
  sqlite3_close(db);
  return teams;
}

bool writeTeam(const std::string &dbPath, const TeamRecord &team) {
  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return false;
  }

  const char *insertSql =
      "INSERT INTO teams ("
      "name, league, team_code, "
      "next_game_url_template, next_game_parser, "
      "live_game_url_template, live_game_parser, "
      "api_team_id, enabled, display_order, theme_name, next_game_utc"
      ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

  const char *updateSql = "UPDATE teams SET "
                          "name=?, league=?, team_code=?, "
                          "next_game_url_template=?, next_game_parser=?, "
                          "live_game_url_template=?, live_game_parser=?, "
                          "api_team_id=?, enabled=?, display_order=?, "
                          "theme_name=?, next_game_utc=? "
                          "WHERE id=?;";

  sqlite3_stmt *stmt = nullptr;
  const char *sql = (team.id <= 0) ? insertSql : updateSql;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  int idx = 1;
  sqlite3_bind_text(stmt, idx++, team.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, team.league.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, team.teamCode.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, team.nextGameUrlTemplate.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, team.nextGameParser.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, team.liveGameUrlTemplate.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, team.liveGameParser.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, team.apiTeamId.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, idx++, team.enabled);
  sqlite3_bind_int(stmt, idx++, team.displayOrder);
  sqlite3_bind_text(stmt, idx++, team.themeName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, team.nextGameUtc.c_str(), -1,
                    SQLITE_TRANSIENT);

  if (team.id > 0)
    sqlite3_bind_int(stmt, idx++, team.id);

  const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return ok;
}

bool deleteTeam(const std::string &dbPath, int id) {
  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return false;
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = "DELETE FROM teams WHERE id=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  sqlite3_bind_int(stmt, 1, id);
  const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return ok;
}

#include <sqlite3.h>

bool saveLedRestoreState(const std::string &dbPath,
                         const std::vector<LEDData> &leds) {
  sqlite3 *db = nullptr;

  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    LOG_ERROR() << "saveLedRestoreState: failed to open DB";
    return false;
  }

  const char *createSql = "CREATE TABLE IF NOT EXISTS led_restore ("
                          "led_index INTEGER PRIMARY KEY,"
                          "red INTEGER NOT NULL,"
                          "green INTEGER NOT NULL,"
                          "blue INTEGER NOT NULL);";

  if (sqlite3_exec(db, createSql, nullptr, nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR() << "saveLedRestoreState: failed to create table";
    sqlite3_close(db);
    return false;
  }

  sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

  sqlite3_exec(db, "DELETE FROM led_restore;", nullptr, nullptr, nullptr);

  const char *insertSql =
      "INSERT INTO led_restore (led_index, red, green, blue) "
      "VALUES (?, ?, ?, ?);";

  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR() << "saveLedRestoreState: prepare failed";
    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return false;
  }

  for (size_t i = 0; i < leds.size(); ++i) {
    sqlite3_reset(stmt);

    sqlite3_bind_int(stmt, 1, static_cast<int>(i));
    sqlite3_bind_int(stmt, 2, leds[i].redVal);
    sqlite3_bind_int(stmt, 3, leds[i].grnVal);
    sqlite3_bind_int(stmt, 4, leds[i].bluVal);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      LOG_ERROR() << "saveLedRestoreState: insert failed at index " << i;
      sqlite3_finalize(stmt);
      sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
      sqlite3_close(db);
      return false;
    }
  }

  sqlite3_finalize(stmt);

  if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
    LOG_ERROR() << "saveLedRestoreState: commit failed";
    sqlite3_close(db);
    return false;
  }

  sqlite3_close(db);

  LOG_INFO() << "Saved LED restore state (" << leds.size() << " LEDs)";
  return true;
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
    int idx = sqlite3_column_int(stmt, 0);
    int r = sqlite3_column_int(stmt, 1);
    int g = sqlite3_column_int(stmt, 2);
    int b = sqlite3_column_int(stmt, 3);

    if (idx >= 0 && static_cast<size_t>(idx) < leds.size()) {
      leds[idx].redVal = r;
      leds[idx].grnVal = g;
      leds[idx].bluVal = b;
      count++;
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
