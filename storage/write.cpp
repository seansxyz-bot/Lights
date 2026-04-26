#include "write.h"

#include <iostream>
#include <sqlite3.h>

namespace {

bool execSQL(sqlite3 *db, const char *sql) {
  char *errMsg = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQLite error: " << (errMsg ? errMsg : "unknown") << "\n";
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

} // namespace

int writeLEDInfo(std::string path, std::vector<LEDData> data) {
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

  if (!execSQL(db, "DELETE FROM led_info;")) {
    execSQL(db, "ROLLBACK;");
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
    std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
    execSQL(db, "ROLLBACK;");
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
      std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
      ok = false;
      break;
    }
  }

  sqlite3_finalize(stmt);

  if (!ok) {
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

  if (writeToServer) {
    http.sendLEDsAsync("http://192.168.1.100/lights_apis/sync.php", data,
                       DEVICE);
  }

  return 1;
}

int writeOptions(std::string path, Options data) {
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
    std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
    sqlite3_close(db);
    return 0;
  }

  auto writeOne = [&](const std::string &name, int value) -> bool {
    std::cout << "ONN - " << name << " - " << value << std::endl;
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, value);

    return sqlite3_step(stmt) == SQLITE_DONE;
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

int writeSchedule(std::string path, std::vector<Schedule> data) {
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

  if (!execSQL(db, "DELETE FROM schedules;")) {
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  const char *sql = R"(
    INSERT INTO schedules
    (theme_id, name, start_date, start_time, end_date, end_time, enabled)
    VALUES (?, ?, ?, ?, ?, ?, ?);
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  bool ok = true;

  for (const auto &s : data) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    sqlite3_bind_int(stmt, 1, s.themeID);
    sqlite3_bind_text(stmt, 2, s.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, s.sDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, s.sTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, s.eDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, s.eTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, s.enabled);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
      ok = false;
      break;
    }
  }

  sqlite3_finalize(stmt);

  if (!ok) {
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

int writeThemeColors(std::string path, const std::vector<Theme> &themes) {
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

  const char *sql = R"(
    INSERT INTO themes (theme_id, name, fileName, color_index, r, g, b)
    VALUES (?, ?, ?, ?, ?, ?, ?);
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  bool ok = true;

  for (const auto &theme : themes) {
    for (int i = 0; i < static_cast<int>(theme.colors.size()); ++i) {
      const auto &c = theme.colors[i];

      sqlite3_reset(stmt);
      sqlite3_clear_bindings(stmt);

      sqlite3_bind_int(stmt, 1, theme.id);
      sqlite3_bind_text(stmt, 2, theme.name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, theme.fileName.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 4, i);
      sqlite3_bind_int(stmt, 5, c.r);
      sqlite3_bind_int(stmt, 6, c.g);
      sqlite3_bind_int(stmt, 7, c.b);

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Insert failed: " << sqlite3_errmsg(db) << "\n";
        ok = false;
        break;
      }
    }
    if (!ok)
      break;
  }

  sqlite3_finalize(stmt);

  if (!ok) {
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

int writePatternSpeeds(std::string path, const std::vector<Pattern> &patterns) {
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

  const char *sql = R"(
    INSERT INTO pattern_speeds (id, speed)
    VALUES (?, ?)
    ON CONFLICT(id) DO UPDATE SET speed = excluded.speed;
  )";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
    execSQL(db, "ROLLBACK;");
    sqlite3_close(db);
    return 0;
  }

  bool ok = true;

  for (const auto &p : patterns) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    sqlite3_bind_int(stmt, 1, p.id);
    sqlite3_bind_int(stmt, 2, p.speed);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::cerr << "Insert/update pattern speed failed: " << sqlite3_errmsg(db)
                << "\n";
      ok = false;
      break;
    }
  }

  sqlite3_finalize(stmt);

  if (!ok) {
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

int writeTeam(const std::string &dbPath, TeamRecord &team) {
  ensureSportsSchema(dbPath);

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return 0;
  }

  const char *insertSql = R"(
    INSERT INTO teams
    (name, league, team_code, home_away, api_team_id, enabled, display_order,
     icon_path, theme_name, theme_id, next_game_url_template, next_game_parser,
     live_game_url_template, live_game_parser, next_game_utc, last_home_score,
     last_away_score, last_game_id, last_checked_utc)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
  )";

  const char *updateSql = R"(
    UPDATE teams SET
      name = ?, league = ?, team_code = ?, home_away = ?, api_team_id = ?,
      enabled = ?, display_order = ?, icon_path = ?, theme_name = ?,
      theme_id = ?, next_game_url_template = ?, next_game_parser = ?,
      live_game_url_template = ?, live_game_parser = ?, next_game_utc = ?,
      last_home_score = ?, last_away_score = ?, last_game_id = ?,
      last_checked_utc = ?
    WHERE id = ?;
  )";

  sqlite3_stmt *stmt = nullptr;
  const bool isUpdate = team.id > 0;
  const char *sql = isUpdate ? updateSql : insertSql;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR() << "writeTeam prepare failed: " << sqlite3_errmsg(db);
    sqlite3_close(db);
    return 0;
  }

  int col = 1;
  sqlite3_bind_text(stmt, col++, team.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.league.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.teamCode.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.homeAway.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.apiTeamId.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, col++, team.enabled);
  sqlite3_bind_int(stmt, col++, team.displayOrder);
  sqlite3_bind_text(stmt, col++, team.iconPath.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.themeName.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, col++, team.themeID);
  sqlite3_bind_text(stmt, col++, team.nextGameUrlTemplate.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.nextGameParser.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.liveGameUrlTemplate.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.liveGameParser.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.nextGameUtc.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, col++, team.lastHomeScore);
  sqlite3_bind_int(stmt, col++, team.lastAwayScore);
  sqlite3_bind_text(stmt, col++, team.lastGameId.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, col++, team.lastCheckedUtc.c_str(), -1,
                    SQLITE_TRANSIENT);

  if (isUpdate)
    sqlite3_bind_int(stmt, col++, team.id);

  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  if (!ok)
    LOG_ERROR() << "writeTeam failed: " << sqlite3_errmsg(db);

  sqlite3_finalize(stmt);

  if (ok && !isUpdate)
    team.id = static_cast<int>(sqlite3_last_insert_rowid(db));

  sqlite3_close(db);
  return ok ? 1 : 0;
}

int deleteTeam(const std::string &dbPath, int teamId) {
  ensureSportsSchema(dbPath);

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return 0;
  }

  if (!execSQL(db, "BEGIN TRANSACTION;")) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_stmt *stmt = nullptr;
  bool ok = true;

  const char *deleteAnimSql = "DELETE FROM team_animations WHERE team_id = ?;";
  if (sqlite3_prepare_v2(db, deleteAnimSql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, teamId);
    ok &= sqlite3_step(stmt) == SQLITE_DONE;
  } else {
    ok = false;
  }
  if (stmt)
    sqlite3_finalize(stmt);

  stmt = nullptr;
  const char *deleteStateSql = "DELETE FROM sports_live_state WHERE team_id = ?;";
  if (sqlite3_prepare_v2(db, deleteStateSql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, teamId);
    ok &= sqlite3_step(stmt) == SQLITE_DONE;
  } else {
    ok = false;
  }
  if (stmt)
    sqlite3_finalize(stmt);

  stmt = nullptr;
  const char *deleteTeamSql = "DELETE FROM teams WHERE id = ?;";
  if (sqlite3_prepare_v2(db, deleteTeamSql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, teamId);
    ok &= sqlite3_step(stmt) == SQLITE_DONE;
  } else {
    ok = false;
  }
  if (stmt)
    sqlite3_finalize(stmt);

  if (ok)
    ok = execSQL(db, "COMMIT;");
  else
    execSQL(db, "ROLLBACK;");

  sqlite3_close(db);
  return ok ? 1 : 0;
}

int writeTeamAnimations(const std::string &dbPath, int teamId,
                        const std::vector<TeamAnimation> &animations) {
  ensureSportsSchema(dbPath);

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return 0;
  }

  if (!execSQL(db, "BEGIN TRANSACTION;")) {
    sqlite3_close(db);
    return 0;
  }

  bool ok = true;

  sqlite3_stmt *deleteStmt = nullptr;
  const char *deleteSql = "DELETE FROM team_animations WHERE team_id = ?;";
  if (sqlite3_prepare_v2(db, deleteSql, -1, &deleteStmt, nullptr) ==
      SQLITE_OK) {
    sqlite3_bind_int(deleteStmt, 1, teamId);
    ok &= sqlite3_step(deleteStmt) == SQLITE_DONE;
  } else {
    ok = false;
  }
  if (deleteStmt)
    sqlite3_finalize(deleteStmt);

  const char *insertSql = R"(
    INSERT INTO team_animations
    (team_id, animation_type, file_path, enabled, display_order)
    VALUES (?, ?, ?, ?, ?);
  )";

  sqlite3_stmt *stmt = nullptr;
  if (ok && sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) !=
                SQLITE_OK) {
    ok = false;
  }

  if (ok) {
    for (const auto &a : animations) {
      if (a.filePath.empty())
        continue;

      sqlite3_reset(stmt);
      sqlite3_clear_bindings(stmt);
      sqlite3_bind_int(stmt, 1, teamId);
      sqlite3_bind_text(stmt, 2, a.animationType.c_str(), -1,
                        SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, a.filePath.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(stmt, 4, a.enabled);
      sqlite3_bind_int(stmt, 5, a.displayOrder);

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        ok = false;
        break;
      }
    }
  }

  if (stmt)
    sqlite3_finalize(stmt);

  if (ok)
    ok = execSQL(db, "COMMIT;");
  else
    execSQL(db, "ROLLBACK;");

  sqlite3_close(db);
  return ok ? 1 : 0;
}

int deleteTeamAnimation(const std::string &dbPath, int animationId) {
  ensureSportsSchema(dbPath);

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return 0;
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = "DELETE FROM team_animations WHERE id = ?;";
  bool ok = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, animationId);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
  }

  if (stmt)
    sqlite3_finalize(stmt);
  sqlite3_close(db);
  return ok ? 1 : 0;
}

int updateTeamNextGame(const std::string &dbPath, int teamId,
                       const std::string &nextGameUtc,
                       const std::string &gameId,
                       const std::string &checkedUtc) {
  ensureSportsSchema(dbPath);

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return 0;
  }

  const char *sql = R"(
    UPDATE teams
    SET next_game_utc = ?, last_game_id = ?, last_checked_utc = ?
    WHERE id = ?;
  )";

  sqlite3_stmt *stmt = nullptr;
  bool ok = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, nextGameUtc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, gameId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, checkedUtc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, teamId);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
  }

  if (stmt)
    sqlite3_finalize(stmt);
  sqlite3_close(db);
  return ok ? 1 : 0;
}

int updateSportsLiveState(const std::string &dbPath, int teamId,
                          const std::string &gameId,
                          const std::string &gameState, int homeScore,
                          int awayScore, const std::string &pollUtc,
                          bool active) {
  ensureSportsSchema(dbPath);

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return 0;
  }

  const char *sql = R"(
    INSERT INTO sports_live_state
    (team_id, game_id, game_state, home_score, away_score, last_poll_utc, active)
    VALUES (?, ?, ?, ?, ?, ?, ?)
    ON CONFLICT(team_id) DO UPDATE SET
      game_id = excluded.game_id,
      game_state = excluded.game_state,
      home_score = excluded.home_score,
      away_score = excluded.away_score,
      last_poll_utc = excluded.last_poll_utc,
      active = excluded.active;
  )";

  sqlite3_stmt *stmt = nullptr;
  bool ok = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, teamId);
    sqlite3_bind_text(stmt, 2, gameId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, gameState.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, homeScore);
    sqlite3_bind_int(stmt, 5, awayScore);
    sqlite3_bind_text(stmt, 6, pollUtc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, active ? 1 : 0);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
  }

  if (stmt)
    sqlite3_finalize(stmt);

  if (ok) {
    const char *teamSql = R"(
      UPDATE teams
      SET last_home_score = ?, last_away_score = ?, last_game_id = ?,
          last_checked_utc = ?
      WHERE id = ?;
    )";

    sqlite3_stmt *teamStmt = nullptr;
    if (sqlite3_prepare_v2(db, teamSql, -1, &teamStmt, nullptr) ==
        SQLITE_OK) {
      sqlite3_bind_int(teamStmt, 1, homeScore);
      sqlite3_bind_int(teamStmt, 2, awayScore);
      sqlite3_bind_text(teamStmt, 3, gameId.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(teamStmt, 4, pollUtc.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(teamStmt, 5, teamId);
      ok = sqlite3_step(teamStmt) == SQLITE_DONE;
    } else {
      ok = false;
    }

    if (teamStmt)
      sqlite3_finalize(teamStmt);
  }

  sqlite3_close(db);
  return ok ? 1 : 0;
}

int updateSportsAnimatedScore(const std::string &dbPath, int teamId,
                              int homeScore, int awayScore) {
  ensureSportsSchema(dbPath);

  sqlite3 *db = nullptr;
  if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
    if (db)
      sqlite3_close(db);
    return 0;
  }

  const char *sql = R"(
    INSERT INTO sports_live_state
    (team_id, last_home_score_animated, last_away_score_animated)
    VALUES (?, ?, ?)
    ON CONFLICT(team_id) DO UPDATE SET
      last_home_score_animated = excluded.last_home_score_animated,
      last_away_score_animated = excluded.last_away_score_animated;
  )";

  sqlite3_stmt *stmt = nullptr;
  bool ok = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, teamId);
    sqlite3_bind_int(stmt, 2, homeScore);
    sqlite3_bind_int(stmt, 3, awayScore);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
  }

  if (stmt)
    sqlite3_finalize(stmt);
  sqlite3_close(db);
  return ok ? 1 : 0;
}
