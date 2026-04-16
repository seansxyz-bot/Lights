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
    INSERT INTO themes (theme_id, name, color_index, r, g, b)
    VALUES (?, ?, ?, ?, ?, ?);
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
      sqlite3_bind_int(stmt, 3, i);
      sqlite3_bind_int(stmt, 4, c.r);
      sqlite3_bind_int(stmt, 5, c.g);
      sqlite3_bind_int(stmt, 6, c.b);

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
