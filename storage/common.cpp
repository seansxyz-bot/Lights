#include "common.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sqlite3.h>
#include <sstream>
#include <utility>

namespace {
std::string trimCopy(std::string s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

std::string envValue(const char *name) {
  const char *value = std::getenv(name);
  return value ? trimCopy(value) : "";
}

int parseIntOrDefault(const std::string &value, int fallback) {
  try {
    size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    return consumed == value.size() ? parsed : fallback;
  } catch (...) {
    return fallback;
  }
}

void applyConfigValue(AppConfig &cfg, const std::string &key,
                      const std::string &value) {
  if (key == "settings_path")
    cfg.settingsPath = value;
  else if (key == "api_base_url")
    cfg.apiBaseUrl = value;
  else if (key == "gpio_chip")
    cfg.gpioChip = value;
  else if (key == "teensy_switch_pin")
    cfg.teensySwitchPin = parseIntOrDefault(value, cfg.teensySwitchPin);
  else if (key == "shift_register_switch_pin")
    cfg.shiftRegisterSwitchPin =
        parseIntOrDefault(value, cfg.shiftRegisterSwitchPin);
  else if (key == "amp_switch_pin")
    cfg.ampSwitchPin = parseIntOrDefault(value, cfg.ampSwitchPin);
  else if (key == "i2c_bus")
    cfg.i2cBus = value;
  else if (key == "lightshow_monitor")
    cfg.lightShowMonitor = value;
  else if (key == "bluetooth_sink")
    cfg.bluetoothSink = value;
  else if (key == "doorbell_sink")
    cfg.doorbellSink = value;
}

void readConfigFile(AppConfig &cfg, const std::string &path) {
  std::ifstream in(path);
  if (!in)
    return;

  std::string line;
  while (std::getline(in, line)) {
    const auto comment = line.find('#');
    if (comment != std::string::npos)
      line = line.substr(0, comment);

    const auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;

    std::string key = trimCopy(line.substr(0, eq));
    std::string value = trimCopy(line.substr(eq + 1));
    if (key.empty() || value.empty())
      continue;

    std::replace(key.begin(), key.end(), '-', '_');
    applyConfigValue(cfg, key, value);
  }
}

AppConfig loadAppConfig() {
  AppConfig cfg;

  const std::string home = envValue("HOME").empty() ? HOME_DIR : envValue("HOME");
  const std::string explicitConfig = envValue("LIGHTS_CONFIG");
  if (!explicitConfig.empty())
    readConfigFile(cfg, explicitConfig);

  readConfigFile(cfg, home + "/.config/lights/config");
  readConfigFile(cfg, home + "/.config/lights/lights.conf");
  readConfigFile(cfg, std::string(SETTINGS_PATH) + "/lights.conf");

  const std::pair<const char *, const char *> stringEnv[] = {
      {"LIGHTS_SETTINGS_PATH", "settings_path"},
      {"LIGHTS_API_BASE_URL", "api_base_url"},
      {"LIGHTS_GPIO_CHIP", "gpio_chip"},
      {"LIGHTS_I2C_BUS", "i2c_bus"},
      {"LIGHTSHOW_MONITOR", "lightshow_monitor"},
      {"LIGHTS_BLUETOOTH_SINK", "bluetooth_sink"},
      {"LIGHTS_DOORBELL_SINK", "doorbell_sink"},
  };

  for (const auto &[envName, key] : stringEnv) {
    const std::string value = envValue(envName);
    if (!value.empty())
      applyConfigValue(cfg, key, value);
  }

  const std::pair<const char *, const char *> intEnv[] = {
      {"LIGHTS_TEENSY_SWITCH_PIN", "teensy_switch_pin"},
      {"LIGHTS_SHIFT_REGISTER_SWITCH_PIN", "shift_register_switch_pin"},
      {"LIGHTS_AMP_SWITCH_PIN", "amp_switch_pin"},
  };

  for (const auto &[envName, key] : intEnv) {
    const std::string value = envValue(envName);
    if (!value.empty())
      applyConfigValue(cfg, key, value);
  }

  return cfg;
}

bool execSQL(sqlite3 *db, const char *sql) {
  char *errMsg = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    LOG_ERROR() << "SQLite schema error: " << (errMsg ? errMsg : "unknown");
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

void ignoreDuplicateColumn(sqlite3 *db, const char *sql) {
  char *errMsg = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    const std::string msg = errMsg ? errMsg : "";
    if (msg.find("duplicate column") == std::string::npos) {
      LOG_WARN() << "SQLite migration skipped: " << msg;
    }
    sqlite3_free(errMsg);
  }
}

bool schedulesThemeIdIsPrimaryKey(sqlite3 *db) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, "PRAGMA table_info(schedules);", -1, &stmt,
                         nullptr) != SQLITE_OK) {
    return false;
  }

  bool result = false;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char *name = sqlite3_column_text(stmt, 1);
    const int pk = sqlite3_column_int(stmt, 5);
    if (name && std::string(reinterpret_cast<const char *>(name)) == "theme_id" &&
        pk != 0) {
      result = true;
      break;
    }
  }

  sqlite3_finalize(stmt);
  return result;
}

bool migrateSchedulesPrimaryKey(sqlite3 *db) {
  if (!schedulesThemeIdIsPrimaryKey(db))
    return true;

  LOG_WARN() << "Migrating schedules table to remove theme_id primary key";

  const char *sql = R"(
    BEGIN TRANSACTION;
    CREATE TABLE schedules_migration_new (
      theme_id INTEGER NOT NULL DEFAULT 0,
      name TEXT NOT NULL DEFAULT '',
      start_date TEXT NOT NULL DEFAULT '',
      start_time TEXT NOT NULL DEFAULT '',
      end_date TEXT NOT NULL DEFAULT '',
      end_time TEXT NOT NULL DEFAULT '',
      enabled INTEGER NOT NULL DEFAULT 1
    );
    INSERT INTO schedules_migration_new
      (theme_id, name, start_date, start_time, end_date, end_time, enabled)
    SELECT theme_id, name, start_date, start_time, end_date, end_time, enabled
    FROM schedules
    ORDER BY rowid;
    DROP TABLE schedules;
    ALTER TABLE schedules_migration_new RENAME TO schedules;
    COMMIT;
  )";

  char *errMsg = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    LOG_ERROR() << "Failed to migrate schedules table: "
                << (errMsg ? errMsg : "unknown");
    sqlite3_free(errMsg);
    execSQL(db, "ROLLBACK;");
    return false;
  }

  return true;
}

bool seedTeam(sqlite3 *db, const char *name, const char *league,
              const char *teamCode, const char *apiTeamId, int displayOrder,
              const char *nextUrl, const char *nextParser, const char *liveUrl,
              const char *liveParser) {
  const char *sql = R"(
    INSERT INTO teams
      (name, league, team_code, home_away, api_team_id, enabled, display_order,
       icon_path, theme_name, theme_id, next_game_url_template, next_game_parser,
       live_game_url_template, live_game_parser)
    VALUES (?, ?, ?, 'home', ?, 1, ?, '', '', 0, ?, ?, ?, ?)
    ON CONFLICT(league, team_code) DO UPDATE SET
      name = excluded.name,
      api_team_id = excluded.api_team_id,
      display_order = excluded.display_order,
      next_game_url_template = excluded.next_game_url_template,
      next_game_parser = excluded.next_game_parser,
      live_game_url_template = excluded.live_game_url_template,
      live_game_parser = excluded.live_game_parser;
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR() << "Failed to prepare sports team seed: " << sqlite3_errmsg(db);
    return false;
  }

  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, league, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, teamCode, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, apiTeamId, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 5, displayOrder);
  sqlite3_bind_text(stmt, 6, nextUrl, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, nextParser, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, liveUrl, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, liveParser, -1, SQLITE_TRANSIENT);

  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok)
    LOG_ERROR() << "Failed to seed sports team " << name << ": "
                << sqlite3_errmsg(db);
  sqlite3_finalize(stmt);
  return ok;
}

int teamIdForLeagueCode(sqlite3 *db, const char *league, const char *teamCode) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(
          db, "SELECT id FROM teams WHERE league = ? AND team_code = ?;", -1,
          &stmt, nullptr) != SQLITE_OK) {
    return 0;
  }
  sqlite3_bind_text(stmt, 1, league, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, teamCode, -1, SQLITE_TRANSIENT);
  int id = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    id = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return id;
}

bool seedTeamColor(sqlite3 *db, int teamId, const char *role, int r, int g,
                   int b, int order) {
  const char *sql = R"(
    INSERT INTO team_colors
      (team_id, color_role, r, g, b, display_order)
    VALUES (?, ?, ?, ?, ?, ?)
    ON CONFLICT(team_id, color_role) DO UPDATE SET
      r = excluded.r,
      g = excluded.g,
      b = excluded.b,
      display_order = excluded.display_order;
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;
  sqlite3_bind_int(stmt, 1, teamId);
  sqlite3_bind_text(stmt, 2, role, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 3, r);
  sqlite3_bind_int(stmt, 4, g);
  sqlite3_bind_int(stmt, 5, b);
  sqlite3_bind_int(stmt, 6, order);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool seedDefaultSports(sqlite3 *db) {
  bool ok = true;
  ok &= execSQL(db, "DELETE FROM themes WHERE theme_id IN (16,17,18);");
  ok &= execSQL(db, "UPDATE schedules SET theme_id = 0 WHERE theme_id IN "
                    "(16,17,18);");

  ok &= seedTeam(db, "Seattle Mariners", "MLB", "SEA", "136", 20,
                 "https://statsapi.mlb.com/api/v1/schedule?sportId=1&teamId={api_team_id}&season=2026&hydrate=team,linescore",
                 "mlb_schedule",
                 "https://statsapi.mlb.com/api/v1.1/game/{game_id}/feed/live",
                 "mlb_live");
  ok &= seedTeam(db, "Seattle Seahawks", "NFL", "SEA", "26", 30,
                 "https://site.api.espn.com/apis/site/v2/sports/football/nfl/teams/{api_team_id}/schedule",
                 "nfl_schedule",
                 "https://site.api.espn.com/apis/site/v2/sports/football/nfl/summary?event={game_id}",
                 "nfl_live");
  ok &= seedTeam(db, "Seattle Kraken", "NHL", "SEA", "SEA", 10,
                 "https://api-web.nhle.com/v1/club-schedule/{api_team_id}/week/now",
                 "nhl_schedule",
                 "https://api-web.nhle.com/v1/gamecenter/{game_id}/landing",
                 "nhl_live");

  const int mariners = teamIdForLeagueCode(db, "MLB", "SEA");
  const int seahawks = teamIdForLeagueCode(db, "NFL", "SEA");
  const int kraken = teamIdForLeagueCode(db, "NHL", "SEA");

  if (mariners > 0) {
    ok &= seedTeamColor(db, mariners, "home_1", 0, 92, 92, 0);
    ok &= seedTeamColor(db, mariners, "home_2", 196, 206, 211, 1);
    ok &= seedTeamColor(db, mariners, "away_1", 12, 35, 64, 2);
    ok &= seedTeamColor(db, mariners, "away_2", 0, 92, 92, 3);
  }
  if (seahawks > 0) {
    ok &= seedTeamColor(db, seahawks, "home_1", 0, 34, 68, 0);
    ok &= seedTeamColor(db, seahawks, "home_2", 105, 190, 40, 1);
    ok &= seedTeamColor(db, seahawks, "away_1", 165, 172, 175, 2);
    ok &= seedTeamColor(db, seahawks, "away_2", 0, 34, 68, 3);
  }
  if (kraken > 0) {
    ok &= seedTeamColor(db, kraken, "home_1", 0, 22, 40, 0);
    ok &= seedTeamColor(db, kraken, "home_2", 153, 0, 51, 1);
    ok &= seedTeamColor(db, kraken, "away_1", 104, 178, 183, 2);
    ok &= seedTeamColor(db, kraken, "away_2", 0, 22, 40, 3);
  }

  return ok;
}
} // namespace

const AppConfig &appConfig() {
  static const AppConfig cfg = loadAppConfig();
  return cfg;
}

const std::string &runtimeSettingsPath() { return appConfig().settingsPath; }

bool ensureCoreSchema(const std::string &dbPath) {
  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    LOG_ERROR() << "ensureCoreSchema: failed to open " << dbPath;
    if (db)
      sqlite3_close(db);
    return false;
  }

  bool ok = true;

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS options (
      name TEXT PRIMARY KEY,
      value INTEGER NOT NULL DEFAULT 0
    );
  )");

  ok &= execSQL(db, R"(
    INSERT OR IGNORE INTO options (name, value) VALUES
      ('auto', 0),
      ('on', 0),
      ('theme', 0),
      ('ptrn', 0),
      ('bluetooth', 0);
  )");

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS led_info (
      name TEXT NOT NULL DEFAULT '',
      led_group INTEGER NOT NULL DEFAULT 0,
      red_pin INTEGER NOT NULL,
      red_value INTEGER NOT NULL DEFAULT 0,
      grn_pin INTEGER NOT NULL,
      grn_value INTEGER NOT NULL DEFAULT 0,
      blu_pin INTEGER NOT NULL,
      blu_value INTEGER NOT NULL DEFAULT 0
    );
  )");

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS themes (
      theme_id INTEGER NOT NULL,
      name TEXT NOT NULL DEFAULT '',
      fileName TEXT NOT NULL DEFAULT '',
      color_index INTEGER NOT NULL,
      r INTEGER NOT NULL DEFAULT 0,
      g INTEGER NOT NULL DEFAULT 0,
      b INTEGER NOT NULL DEFAULT 0,
      PRIMARY KEY(theme_id, color_index)
    );
  )");

  ok &= execSQL(db, "DELETE FROM themes WHERE theme_id IN (16,17,18);");

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS schedules (
      theme_id INTEGER NOT NULL DEFAULT 0,
      name TEXT NOT NULL DEFAULT '',
      start_date TEXT NOT NULL DEFAULT '',
      start_time TEXT NOT NULL DEFAULT '',
      end_date TEXT NOT NULL DEFAULT '',
      end_time TEXT NOT NULL DEFAULT '',
      enabled INTEGER NOT NULL DEFAULT 1
    );
  )");

  ok &= migrateSchedulesPrimaryKey(db);
  ok &= execSQL(db, "UPDATE schedules SET theme_id = 0 WHERE theme_id IN "
                    "(16,17,18);");

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS pattern_speeds (
      id INTEGER PRIMARY KEY,
      speed INTEGER NOT NULL DEFAULT 50
    );
  )");

  ok &= execSQL(db, R"(
    INSERT OR IGNORE INTO pattern_speeds (id, speed) VALUES
      (2, 50),
      (3, 50),
      (4, 50),
      (5, 50),
      (6, 50),
      (7, 50),
      (8, 50);
  )");

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS bluetooth_devices (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      mac_address TEXT NOT NULL UNIQUE,
      name TEXT NOT NULL DEFAULT '',
      device_type TEXT NOT NULL DEFAULT '',
      trusted INTEGER NOT NULL DEFAULT 0,
      paired INTEGER NOT NULL DEFAULT 0,
      blocked INTEGER NOT NULL DEFAULT 0,
      connected INTEGER NOT NULL DEFAULT 0,
      discovered INTEGER NOT NULL DEFAULT 0,
      display_order INTEGER NOT NULL DEFAULT 0,
      last_seen_utc TEXT NOT NULL DEFAULT '',
      last_connected_utc TEXT NOT NULL DEFAULT '',
      connect_count INTEGER NOT NULL DEFAULT 0
    );
  )");

  ignoreDuplicateColumn(
      db, "ALTER TABLE bluetooth_devices ADD COLUMN name TEXT NOT NULL DEFAULT "
          "'';");
  ignoreDuplicateColumn(
      db, "ALTER TABLE bluetooth_devices ADD COLUMN device_type TEXT NOT NULL "
          "DEFAULT '';");
  ignoreDuplicateColumn(
      db, "ALTER TABLE bluetooth_devices ADD COLUMN trusted INTEGER NOT NULL "
          "DEFAULT 0;");
  ignoreDuplicateColumn(
      db, "ALTER TABLE bluetooth_devices ADD COLUMN paired INTEGER NOT NULL "
          "DEFAULT 0;");
  ignoreDuplicateColumn(
      db, "ALTER TABLE bluetooth_devices ADD COLUMN blocked INTEGER NOT NULL "
          "DEFAULT 0;");
  ignoreDuplicateColumn(
      db, "ALTER TABLE bluetooth_devices ADD COLUMN connected INTEGER NOT NULL "
          "DEFAULT 0;");
  ignoreDuplicateColumn(
      db,
      "ALTER TABLE bluetooth_devices ADD COLUMN discovered INTEGER NOT NULL "
      "DEFAULT 0;");
  ignoreDuplicateColumn(
      db, "ALTER TABLE bluetooth_devices ADD COLUMN display_order INTEGER NOT "
          "NULL DEFAULT 0;");
  ignoreDuplicateColumn(
      db, "ALTER TABLE bluetooth_devices ADD COLUMN last_seen_utc TEXT NOT "
          "NULL DEFAULT '';");
  ignoreDuplicateColumn(
      db,
      "ALTER TABLE bluetooth_devices ADD COLUMN last_connected_utc TEXT NOT "
      "NULL DEFAULT '';");
  ignoreDuplicateColumn(
      db,
      "ALTER TABLE bluetooth_devices ADD COLUMN connect_count INTEGER NOT "
      "NULL DEFAULT 0;");

  ok &= execSQL(db, "CREATE INDEX IF NOT EXISTS idx_led_info_red_pin "
                    "ON led_info(red_pin);");
  ok &= execSQL(db, "CREATE INDEX IF NOT EXISTS idx_led_info_group "
                    "ON led_info(led_group);");
  ok &= execSQL(db, "CREATE INDEX IF NOT EXISTS idx_schedules_enabled "
                    "ON schedules(enabled);");
  ok &= execSQL(db, "CREATE INDEX IF NOT EXISTS idx_bluetooth_devices_order "
                    "ON bluetooth_devices(display_order, name);");

  sqlite3_close(db);
  return ok;
}

bool ensureSportsSchema(const std::string &dbPath) {
  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    LOG_ERROR() << "ensureSportsSchema: failed to open " << dbPath;
    if (db)
      sqlite3_close(db);
    return false;
  }

  bool ok = true;

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS teams (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT NOT NULL DEFAULT '',
      league TEXT NOT NULL DEFAULT '',
      team_code TEXT NOT NULL DEFAULT '',
      home_away TEXT NOT NULL DEFAULT 'home',
      api_team_id TEXT NOT NULL DEFAULT '',
      enabled INTEGER NOT NULL DEFAULT 1,
      display_order INTEGER NOT NULL DEFAULT 0,
      icon_path TEXT NOT NULL DEFAULT '',
      theme_name TEXT NOT NULL DEFAULT '',
      theme_id INTEGER NOT NULL DEFAULT 0,
      next_game_url_template TEXT NOT NULL DEFAULT '',
      next_game_parser TEXT NOT NULL DEFAULT '',
      live_game_url_template TEXT NOT NULL DEFAULT '',
      live_game_parser TEXT NOT NULL DEFAULT '',
      next_game_utc TEXT NOT NULL DEFAULT '',
      last_home_score INTEGER NOT NULL DEFAULT -1,
      last_away_score INTEGER NOT NULL DEFAULT -1,
      score_animation_delay_seconds INTEGER NOT NULL DEFAULT 0,
      last_game_id TEXT NOT NULL DEFAULT '',
      last_checked_utc TEXT NOT NULL DEFAULT ''
    );
  )");

  ok &= execSQL(db, "DROP INDEX IF EXISTS idx_teams_code;");
  ok &= execSQL(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_teams_league_code "
                    "ON teams(league, team_code);");

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS team_colors (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      team_id INTEGER NOT NULL,
      color_role TEXT NOT NULL,
      r INTEGER NOT NULL,
      g INTEGER NOT NULL,
      b INTEGER NOT NULL,
      display_order INTEGER NOT NULL DEFAULT 0,
      UNIQUE(team_id, color_role),
      FOREIGN KEY(team_id) REFERENCES teams(id) ON DELETE CASCADE
    );
  )");

  ok &= execSQL(db, "CREATE INDEX IF NOT EXISTS idx_team_colors_team_order "
                    "ON team_colors(team_id, display_order);");

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS team_animations (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      team_id INTEGER NOT NULL,
      animation_type TEXT NOT NULL,
      file_path TEXT NOT NULL DEFAULT '',
      enabled INTEGER NOT NULL DEFAULT 1,
      display_order INTEGER NOT NULL DEFAULT 0,
      FOREIGN KEY(team_id) REFERENCES teams(id) ON DELETE CASCADE
    );
  )");

  ok &= execSQL(db, "CREATE INDEX IF NOT EXISTS idx_team_animations_team_type "
                    "ON team_animations(team_id, animation_type, enabled, "
                    "display_order);");

  ok &= execSQL(db, R"(
    CREATE TABLE IF NOT EXISTS sports_live_state (
      team_id INTEGER PRIMARY KEY,
      game_id TEXT NOT NULL DEFAULT '',
      game_state TEXT NOT NULL DEFAULT '',
      home_score INTEGER NOT NULL DEFAULT -1,
      away_score INTEGER NOT NULL DEFAULT -1,
      last_home_score_animated INTEGER NOT NULL DEFAULT -1,
      last_away_score_animated INTEGER NOT NULL DEFAULT -1,
      last_poll_utc TEXT NOT NULL DEFAULT '',
      active INTEGER NOT NULL DEFAULT 0,
      FOREIGN KEY(team_id) REFERENCES teams(id) ON DELETE CASCADE
    );
  )");

  const char *teamColumns[] = {
      "ALTER TABLE teams ADD COLUMN home_away TEXT NOT NULL DEFAULT 'home';",
      "ALTER TABLE teams ADD COLUMN icon_path TEXT NOT NULL DEFAULT '';",
      "ALTER TABLE teams ADD COLUMN theme_id INTEGER NOT NULL DEFAULT 0;",
      "ALTER TABLE teams ADD COLUMN last_home_score INTEGER NOT NULL DEFAULT -1;",
      "ALTER TABLE teams ADD COLUMN last_away_score INTEGER NOT NULL DEFAULT -1;",
      "ALTER TABLE teams ADD COLUMN score_animation_delay_seconds INTEGER NOT NULL DEFAULT 0;",
      "ALTER TABLE teams ADD COLUMN last_game_id TEXT NOT NULL DEFAULT '';",
      "ALTER TABLE teams ADD COLUMN last_checked_utc TEXT NOT NULL DEFAULT '';",
      "ALTER TABLE teams ADD COLUMN next_opponent_code TEXT NOT NULL DEFAULT '';",
      "ALTER TABLE teams ADD COLUMN next_opponent_name TEXT NOT NULL DEFAULT '';",
  };

  for (const char *sql : teamColumns) {
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
      sqlite3_free(errMsg);
    }
  }

  ok &= seedDefaultSports(db);

  sqlite3_close(db);
  return ok;
}
