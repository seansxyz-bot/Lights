#include "readerwriter.h"
#include <algorithm>
#include <sqlite3.h>

HttpHelper http;
std::atomic<bool> writeToServer{true};

std::vector<LEDData> readLEDInfo(std::string path) {
  path += std::string("/led_info");
  // Attempt to open the file for reading
  std::vector<LEDData> data;
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Failed to open the file." << std::endl;
    return data;
  }
  // Read the file line by line
  while (!file.eof()) {
    LEDData d;
    file >> d.name >> d.group >> d.redPin >> d.redVal >> d.grnPin >> d.grnVal >>
        d.bluPin >> d.bluVal;
    data.push_back(d);
  }

  data.pop_back();

  if (writeToServer) {
    // http.sendLEDsAsync("https://lights.seansxyz.com/lights_apis/sync.php",
    // data,
    //                    DEVICE);
    http.sendLEDsAsync("http://192.168.1.100/lights_apis/sync.php", data,
                       DEVICE);
  }

  return data;
}

int writeLEDInfo(std::string path, std::vector<LEDData> data) {
  path += std::string("/led_info");
  std::ofstream file(path);
  int success = 0;
  if (file.is_open()) {
    const int len = data.size();
    for (int i = 0; i < len; i++) {
      LEDData d = data[i];
      file << d.name << "\t" << d.group << "\t" << d.redPin << "\t" << d.redVal
           << "\t" << d.grnPin << "\t" << d.grnVal << "\t" << d.bluPin << "\t"
           << d.bluVal << std::endl;
    }
    success = 1;
  }

  if (writeToServer) {
    //   http.sendLEDsAsync("https://lights.seansxyz.com/lights_apis/sync.php",
    //   data,
    //                      DEVICE);
    http.sendLEDsAsync("http://192.168.1.100/lights_apis/sync.php", data,
                       DEVICE);
  }
  file.close();
  return success;
}

Options readOptions(std::string path) {
  // Attempt to open the file for reading
  std::ifstream file(path + std::string("/option"));
  Options opts;
  if (!file.is_open()) {
    opts.sensor = 1;
    opts.on = 1;
    opts.theme = 0;
    opts.ptrn = 0;
    writeOptions(path, opts);
    return readOptions(path);
  }

  std::string holder;

  // Read the file line by line
  file >> holder >> opts.sensor >> holder >> opts.on >> holder >> opts.theme >>
      holder >> opts.ptrn >> holder;

  if (writeToServer) {
    // http.sendOptionsAsync("https://lights.seansxyz.com/lights_apis/sync.php",
    //                       opts, DEVICE);
    http.sendOptionsAsync("http://192.168.1.100/lights_apis/sync.php", opts,
                          DEVICE);
  }
  return opts;
}

int writeOptions(std::string path, Options data) {
  std::ofstream file(path + std::string("/option"));
  int success = 0;
  if (file.is_open()) {
    file << "Auto" << "\t" << data.sensor << std::endl;
    file << "ON" << "\t" << data.on << std::endl;
    file << "TH" << "\t" << data.theme << std::endl;
    file << "PT" << "\t" << data.ptrn << std::endl;
    success = 1;
  }
  if (writeToServer) {
    // http.sendOptionsAsync("https://lights.seansxyz.com/lights_apis/sync.php",
    //                       data, DEVICE);
    http.sendOptionsAsync("http://192.168.1.100/lights_apis/sync.php", data,
                          DEVICE);
  }
  file.close();
  return success;
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

  const char *themeSql = R"(
    SELECT id, name
    FROM themes
    ORDER BY id
  )";

  sqlite3_stmt *themeStmt = nullptr;
  if (sqlite3_prepare_v2(db, themeSql, -1, &themeStmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare theme query: " << sqlite3_errmsg(db)
              << "\n";
    sqlite3_close(db);
    return out;
  }

  while (sqlite3_step(themeStmt) == SQLITE_ROW) {
    Theme t;
    t.id = sqlite3_column_int(themeStmt, 0);

    const unsigned char *nameText = sqlite3_column_text(themeStmt, 1);
    t.name = nameText ? reinterpret_cast<const char *>(nameText) : "";

    out.push_back(t);
  }
  sqlite3_finalize(themeStmt);

  const char *colorSql = R"(
    SELECT r, g, b
    FROM theme_colors
    WHERE theme_id = ?
    ORDER BY color_order
  )";

  sqlite3_stmt *colorStmt = nullptr;
  if (sqlite3_prepare_v2(db, colorSql, -1, &colorStmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare color query: " << sqlite3_errmsg(db)
              << "\n";
    sqlite3_close(db);
    return out;
  }

  for (auto &theme : out) {
    sqlite3_reset(colorStmt);
    sqlite3_clear_bindings(colorStmt);
    sqlite3_bind_int(colorStmt, 1, theme.id);

    while (sqlite3_step(colorStmt) == SQLITE_ROW) {
      RGB_Color c;
      c.r = static_cast<uint8_t>(sqlite3_column_int(colorStmt, 0));
      c.g = static_cast<uint8_t>(sqlite3_column_int(colorStmt, 1));
      c.b = static_cast<uint8_t>(sqlite3_column_int(colorStmt, 2));
      theme.colors.push_back(c);
    }

    if (theme.colors.empty()) {
      theme.colors.push_back({255, 255, 255});
    }
  }

  sqlite3_finalize(colorStmt);
  sqlite3_close(db);
  return out;
}

int writeThemeColors(const std::string &path,
                     const std::vector<Theme> &themes) {
  const std::string &dbPath = path + "/themes.db";
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

  if (!execSQL(db, "DELETE FROM theme_colors;") ||
      !execSQL(db, "DELETE FROM themes;")) {
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  const char *insertThemeSql = R"(
    INSERT INTO themes (id, name)
    VALUES (?, ?)
  )";

  const char *insertColorSql = R"(
    INSERT INTO theme_colors (theme_id, color_index, r, g, b)
    VALUES (?, ?, ?, ?, ?)
  )";

  sqlite3_stmt *themeStmt = nullptr;
  sqlite3_stmt *colorStmt = nullptr;

  if (sqlite3_prepare_v2(db, insertThemeSql, -1, &themeStmt, nullptr) !=
          SQLITE_OK ||
      sqlite3_prepare_v2(db, insertColorSql, -1, &colorStmt, nullptr) !=
          SQLITE_OK) {
    std::cerr << "Failed to prepare insert statements\n";
    if (themeStmt)
      sqlite3_finalize(themeStmt);
    if (colorStmt)
      sqlite3_finalize(colorStmt);
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  for (const auto &theme : themes) {
    sqlite3_reset(themeStmt);
    sqlite3_clear_bindings(themeStmt);

    sqlite3_bind_int(themeStmt, 1, theme.id);
    sqlite3_bind_text(themeStmt, 2, theme.name.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(themeStmt) != SQLITE_DONE) {
      std::cerr << "Failed inserting theme: " << theme.name << "\n";
      sqlite3_finalize(themeStmt);
      sqlite3_finalize(colorStmt);
      execSQL(db, "ROLLBACK;");
      sqlite3_close(db);
      return 0;
    }

    for (int i = 0; i < static_cast<int>(theme.colors.size()); i++) {
      const auto &c = theme.colors[i];

      sqlite3_reset(colorStmt);
      sqlite3_clear_bindings(colorStmt);

      sqlite3_bind_int(colorStmt, 1, theme.id);
      sqlite3_bind_int(colorStmt, 2, i);
      sqlite3_bind_int(colorStmt, 3, c.r);
      sqlite3_bind_int(colorStmt, 4, c.g);
      sqlite3_bind_int(colorStmt, 5, c.b);

      if (sqlite3_step(colorStmt) != SQLITE_DONE) {
        std::cerr << "Failed inserting color for theme: " << theme.name << "\n";
        sqlite3_finalize(themeStmt);
        sqlite3_finalize(colorStmt);
        execSQL(db, "ROLLBACK;");
        sqlite3_close(db);
        return 0;
      }
    }
  }

  sqlite3_finalize(themeStmt);
  sqlite3_finalize(colorStmt);

  int maxId = 0;
  for (const auto &theme : themes) {
    if (theme.id > maxId)
      maxId = theme.id;
  }

  std::string seqSql =
      "UPDATE sqlite_sequence SET seq = " + std::to_string(maxId) +
      " WHERE name = 'themes';";

  if (!execSQL(db, seqSql.c_str())) {
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

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
