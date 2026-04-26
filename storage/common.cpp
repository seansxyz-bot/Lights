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
} // namespace

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
