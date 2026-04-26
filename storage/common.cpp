#include "common.h"

#include <sqlite3.h>

HttpHelper http;
std::atomic<bool> writeToServer{true};

namespace {
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
} // namespace

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
      ('ptrn', 0);
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
      last_game_id TEXT NOT NULL DEFAULT '',
      last_checked_utc TEXT NOT NULL DEFAULT ''
    );
  )");

  ok &= execSQL(db, "CREATE UNIQUE INDEX IF NOT EXISTS idx_teams_code "
                    "ON teams(team_code);");

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
      "ALTER TABLE teams ADD COLUMN last_game_id TEXT NOT NULL DEFAULT '';",
      "ALTER TABLE teams ADD COLUMN last_checked_utc TEXT NOT NULL DEFAULT '';",
  };

  for (const char *sql : teamColumns) {
    char *errMsg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
      sqlite3_free(errMsg);
    }
  }

  sqlite3_close(db);
  return ok;
}
