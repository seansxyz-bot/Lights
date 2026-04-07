#include "btcontrol.h"

#include "logger.h"

#include <sqlite3.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <thread>

namespace {

class SqliteDB {
public:
  explicit SqliteDB(const std::string &path) : m_db(nullptr) {
    const int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
      if (m_db) {
        LOG_ERROR() << "sqlite3_open failed for " << path << ": "
                    << sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        m_db = nullptr;
      } else {
        LOG_ERROR() << "sqlite3_open failed for " << path;
      }
    }
  }

  ~SqliteDB() {
    if (m_db) {
      sqlite3_close(m_db);
      m_db = nullptr;
    }
  }

  bool valid() const { return m_db != nullptr; }
  sqlite3 *get() const { return m_db; }

private:
  sqlite3 *m_db;
};

} // namespace

BTControl::BTControl(const std::string &dbPath) : m_dbPath(dbPath) {}

bool BTControl::init() {
  LOG_INFO() << "BTControl init dbPath=" << m_dbPath;
  return createTables();
}

bool BTControl::createTables() const {
  SqliteDB db(m_dbPath);
  if (!db.valid())
    return false;

  const char *sql = R"sql(
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
            last_seen_utc TEXT NOT NULL DEFAULT '',
            last_connected_utc TEXT NOT NULL DEFAULT '',
            connect_count INTEGER NOT NULL DEFAULT 0
        );
    )sql";

  char *err = nullptr;
  const int rc = sqlite3_exec(db.get(), sql, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    LOG_ERROR() << "Failed to create bluetooth_devices table: "
                << (err ? err : "unknown sqlite error");
    if (err)
      sqlite3_free(err);
    return false;
  }

  LOG_INFO() << "bluetooth_devices table ready";
  return true;
}

std::string BTControl::nowUtc() {
  std::time_t t = std::time(nullptr);
  std::tm tmUtc{};
#if defined(_WIN32)
  gmtime_s(&tmUtc, &t);
#else
  gmtime_r(&t, &tmUtc);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tmUtc, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

std::string BTControl::trim(const std::string &s) {
  const std::string whitespace = " \t\r\n";
  const size_t start = s.find_first_not_of(whitespace);
  if (start == std::string::npos)
    return "";

  const size_t end = s.find_last_not_of(whitespace);
  return s.substr(start, end - start + 1);
}

bool BTControl::isValidMacAddress(const std::string &macAddress) {
  static const std::regex kMacRegex(R"(^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$)");
  return std::regex_match(macAddress, kMacRegex);
}

bool BTControl::runCommand(const std::string &cmd, std::string *output) {
  std::array<char, 512> buffer{};
  LOG_INFO() << "BT cmd: " << cmd;

  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    LOG_ERROR() << "BT popen failed for command: " << cmd;
    return false;
  }

  std::ostringstream oss;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    oss << buffer.data();
  }

  const int rc = pclose(pipe);
  const std::string out = oss.str();

  LOG_INFO() << "BT rc: " << rc;
  if (!out.empty())
    LOG_INFO() << "BT out: " << trim(out);

  if (output)
    *output = out;

  return rc == 0;
}

bool BTControl::parseBluetoothctlDevices(const std::string &text,
                                         std::vector<BTDevice> &devices) {
  std::istringstream iss(text);
  std::string line;

  while (std::getline(iss, line)) {
    line = trim(line);

    if (line.rfind("Device ", 0) != 0)
      continue;

    std::istringstream ls(line);
    std::string word;
    BTDevice d;

    ls >> word;
    ls >> d.macAddress;

    std::string rest;
    std::getline(ls, rest);
    d.name = trim(rest);
    d.lastSeenUtc = nowUtc();
    d.discovered = true;

    if (isValidMacAddress(d.macAddress))
      devices.push_back(d);
  }

  return true;
}

bool BTControl::parseBluetoothctlInfo(const std::string &macAddress,
                                      const std::string &text,
                                      BTDevice &device) {
  device.macAddress = macAddress;

  std::istringstream iss(text);
  std::string line;

  while (std::getline(iss, line)) {
    line = trim(line);

    auto readBool = [](const std::string &value) { return value == "yes"; };

    if (line.rfind("Name:", 0) == 0) {
      device.name = trim(line.substr(5));
    } else if (line.rfind("Alias:", 0) == 0 && device.name.empty()) {
      device.name = trim(line.substr(6));
    } else if (line.rfind("Icon:", 0) == 0) {
      device.deviceType = trim(line.substr(5));
    } else if (line.rfind("Trusted:", 0) == 0) {
      device.trusted = readBool(trim(line.substr(8)));
    } else if (line.rfind("Paired:", 0) == 0) {
      device.paired = readBool(trim(line.substr(7)));
    } else if (line.rfind("Bonded:", 0) == 0) {
      device.paired = readBool(trim(line.substr(7)));
    } else if (line.rfind("Blocked:", 0) == 0) {
      device.blocked = readBool(trim(line.substr(8)));
    } else if (line.rfind("Connected:", 0) == 0) {
      device.connected = readBool(trim(line.substr(10)));
    }
  }

  device.lastSeenUtc = nowUtc();
  return true;
}

bool BTControl::upsertDevice(const BTDevice &device) const {
  SqliteDB db(m_dbPath);
  if (!db.valid())
    return false;

  const char *sql = R"sql(
        INSERT INTO bluetooth_devices (
            mac_address, name, device_type, trusted, paired, blocked, connected,
            discovered, last_seen_utc, last_connected_utc, connect_count
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(mac_address) DO UPDATE SET
            name = excluded.name,
            device_type = excluded.device_type,
            trusted = excluded.trusted,
            paired = excluded.paired,
            blocked = excluded.blocked,
            connected = excluded.connected,
            discovered = excluded.discovered,
            last_seen_utc = excluded.last_seen_utc,
            last_connected_utc = CASE
                WHEN excluded.last_connected_utc != '' THEN excluded.last_connected_utc
                ELSE bluetooth_devices.last_connected_utc
            END,
            connect_count = CASE
                WHEN excluded.connect_count > bluetooth_devices.connect_count
                    THEN excluded.connect_count
                ELSE bluetooth_devices.connect_count
            END;
    )sql";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR() << "sqlite prepare failed in upsertDevice";
    return false;
  }

  sqlite3_bind_text(stmt, 1, device.macAddress.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, device.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, device.deviceType.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, device.trusted ? 1 : 0);
  sqlite3_bind_int(stmt, 5, device.paired ? 1 : 0);
  sqlite3_bind_int(stmt, 6, device.blocked ? 1 : 0);
  sqlite3_bind_int(stmt, 7, device.connected ? 1 : 0);
  sqlite3_bind_int(stmt, 8, device.discovered ? 1 : 0);
  sqlite3_bind_text(stmt, 9, device.lastSeenUtc.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 10, device.lastConnectedUtc.c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 11, device.connectCount);

  const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);

  if (!ok) {
    LOG_ERROR() << "sqlite step failed in upsertDevice for "
                << device.macAddress;
    return false;
  }

  LOG_INFO() << "BT device upserted: " << device.name << " ["
             << device.macAddress << "] paired=" << device.paired
             << " trusted=" << device.trusted
             << " connected=" << device.connected
             << " discovered=" << device.discovered;
  return true;
}

bool BTControl::markSeenNow(const std::string &macAddress) const {
  if (!isValidMacAddress(macAddress))
    return false;

  SqliteDB db(m_dbPath);
  if (!db.valid())
    return false;

  const char *sql =
      "UPDATE bluetooth_devices SET last_seen_utc = ? WHERE mac_address = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  const std::string now = nowUtc();
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, macAddress.c_str(), -1, SQLITE_TRANSIENT);

  const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

bool BTControl::setConnectedState(const std::string &macAddress,
                                  bool connected) const {
  if (!isValidMacAddress(macAddress))
    return false;

  SqliteDB db(m_dbPath);
  if (!db.valid())
    return false;

  const char *sql =
      "UPDATE bluetooth_devices SET connected = ? WHERE mac_address = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_int(stmt, 1, connected ? 1 : 0);
  sqlite3_bind_text(stmt, 2, macAddress.c_str(), -1, SQLITE_TRANSIENT);

  const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

bool BTControl::markConnectedNow(const std::string &macAddress) const {
  if (!isValidMacAddress(macAddress))
    return false;

  SqliteDB db(m_dbPath);
  if (!db.valid())
    return false;

  const char *sql = R"sql(
        UPDATE bluetooth_devices
        SET connected = 1,
            last_connected_utc = ?,
            last_seen_utc = ?,
            connect_count = connect_count + 1
        WHERE mac_address = ?;
    )sql";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  const std::string now = nowUtc();
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, macAddress.c_str(), -1, SQLITE_TRANSIENT);

  const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);

  if (ok)
    LOG_INFO() << "BT marked connected in DB: " << macAddress;

  return ok;
}

bool BTControl::powerOn() {
  std::string out;

  runCommand("rfkill unblock bluetooth", nullptr);

  for (int attempt = 0; attempt < 8; ++attempt) {
    out.clear();

    if (runCommand("bluetoothctl power on", &out)) {
      for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (isPoweredOn()) {
          if (!setSystemAlias("Light Controller")) {
            LOG_WARN() << "Bluetooth powered on but failed to set alias";
          } else
            setStatus("Bluetooth on");
          return true;
        }
      }
    } else {
      const std::string trimmed = trim(out);

      if (trimmed.find("org.bluez.Error.Busy") != std::string::npos) {
        LOG_WARN() << "BT power on busy, retrying... attempt=" << attempt + 1;

        for (int i = 0; i < 6; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(250));
          if (isPoweredOn()) {
            if (!setSystemAlias("Light Controller")) {
              LOG_WARN() << "Bluetooth powered on but failed to set alias";
            } else
              setStatus("Bluetooth on");
            return true;
          }
        }
      } else {
        setError(trimmed.empty() ? "Failed to power on bluetooth" : trimmed);
        return false;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }

  setError("Bluetooth did not report powered on");
  return false;
}

bool BTControl::powerOff() {
  std::string out;

  for (int attempt = 0; attempt < 8; ++attempt) {
    out.clear();

    if (runCommand("bluetoothctl power off", &out)) {
      runCommand("rfkill block bluetooth", nullptr);

      for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (!isPoweredOn()) {
          setStatus("Bluetooth off");
          return true;
        }
      }
    } else {
      const std::string trimmed = trim(out);

      if (trimmed.find("org.bluez.Error.Busy") != std::string::npos) {
        LOG_WARN() << "BT power off busy, retrying... attempt=" << attempt + 1;

        for (int i = 0; i < 6; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(250));
          if (!isPoweredOn()) {
            setStatus("Bluetooth off");
            return true;
          }
        }
      } else {
        setError(trimmed.empty() ? "Failed to power off bluetooth" : trimmed);
        return false;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }

  if (!isPoweredOn()) {
    setStatus("Bluetooth off");
    return true;
  }

  setError("Bluetooth did not report powered off");
  return false;
}

bool BTControl::isPoweredOn() const {
  std::string out;
  if (!runCommand("bluetoothctl show", &out)) {
    LOG_WARN() << "BT show failed while checking power state";
    return false;
  }

  LOG_INFO() << "BT show raw output:\n" << out;

  std::istringstream iss(out);
  std::string line;

  while (std::getline(iss, line)) {
    line = trim(line);
    if (line.rfind("Powered:", 0) == 0) {
      const bool powered = (trim(line.substr(8)) == "yes");
      LOG_INFO() << "BT Powered state parsed: " << powered;
      return powered;
    }
  }

  LOG_WARN() << "BT Powered state not found in bluetoothctl show output";
  return false;
}

bool BTControl::enableAgent(const std::string &capability) {
  std::string out;
  return runCommand("bluetoothctl agent " + capability, &out);
}

bool BTControl::setDefaultAgent() {
  std::string out;
  return runCommand("bluetoothctl default-agent", &out);
}

bool BTControl::setPairable(bool enabled) {
  std::string out;
  return runCommand(
      std::string("bluetoothctl pairable ") + (enabled ? "on" : "off"), &out);
}

bool BTControl::setDiscoverable(bool enabled) {
  std::string out;
  return runCommand(std::string("bluetoothctl discoverable ") +
                        (enabled ? "on" : "off"),
                    &out);
}

bool BTControl::startScan() {
  if (!isPoweredOn())
    return false;

  std::string out;
  return runCommand("bluetoothctl scan on", &out);
}

bool BTControl::stopScan() {
  std::string out;
  return runCommand("bluetoothctl scan off", &out);
}

bool BTControl::scanPairedDevices() {
  if (!isPoweredOn()) {
    LOG_WARN() << "scanPairedDevices called while bluetooth is powered off";
    return false;
  }

  std::string out;
  if (!runCommand("bluetoothctl devices Paired", &out))
    return false;

  std::vector<BTDevice> devices;
  parseBluetoothctlDevices(out, devices);

  LOG_INFO() << "BT paired devices found: " << devices.size();

  for (auto &device : devices) {
    std::string infoOut;
    if (runCommand("bluetoothctl info " + device.macAddress, &infoOut)) {
      BTDevice full;
      parseBluetoothctlInfo(device.macAddress, infoOut, full);
      if (full.name.empty())
        full.name = device.name;
      full.discovered = true;

      if (full.paired || full.trusted || full.connected) {
        upsertDevice(full);
      }

    } else {
      if (device.paired || device.trusted || device.connected) {
        upsertDevice(device);
      }
    }
  }

  return true;
}

bool BTControl::scanAvailableDevices() {
  if (!isPoweredOn()) {
    LOG_WARN() << "scanAvailableDevices called while bluetooth is powered off";
    return false;
  }

  std::string out;
  if (!runCommand("bluetoothctl devices", &out))
    return false;

  std::vector<BTDevice> devices;
  parseBluetoothctlDevices(out, devices);

  LOG_INFO() << "BT available devices found: " << devices.size();

  for (auto &device : devices) {
    std::string infoOut;
    if (runCommand("bluetoothctl info " + device.macAddress, &infoOut)) {
      BTDevice full;
      parseBluetoothctlInfo(device.macAddress, infoOut, full);
      if (full.name.empty())
        full.name = device.name;
      full.discovered = true;

      if (full.paired || full.trusted || full.connected) {
        upsertDevice(full);
      }
    } else {
      if (device.paired || device.trusted || device.connected) {
        upsertDevice(device);
      }
    }
  }

  return true;
}

bool BTControl::setSystemAlias(const std::string &name) {
  std::string out;

  if (!runCommand("bluetoothctl system-alias \"" + name + "\"", &out)) {
    setError("Failed to set bluetooth name");
    return false;
  }

  return true;
}

bool BTControl::refreshDeviceInfo(const std::string &macAddress) {
  if (!isValidMacAddress(macAddress)) {
    LOG_ERROR() << "refreshDeviceInfo invalid MAC: " << macAddress;
    return false;
  }

  std::string out;
  if (!runCommand("bluetoothctl info " + macAddress, &out))
    return false;

  BTDevice device;
  parseBluetoothctlInfo(macAddress, out, device);
  device.discovered = true;
  return upsertDevice(device);
}

bool BTControl::refreshAllKnownDevices() {
  auto devices = getDevicesRankedByLastConnected();
  bool ok = true;

  for (const auto &device : devices) {
    if (!refreshDeviceInfo(device.macAddress))
      ok = false;
  }

  return ok;
}

std::vector<BTDevice> BTControl::getDevicesRankedByLastConnected() const {
  std::vector<BTDevice> devices;

  SqliteDB db(m_dbPath);
  if (!db.valid())
    return devices;

  const char *sql = R"sql(
        SELECT id, mac_address, name, device_type, trusted, paired, blocked,
               connected, discovered, last_seen_utc, last_connected_utc, connect_count
        FROM bluetooth_devices
        ORDER BY
            CASE WHEN last_connected_utc = '' THEN 1 ELSE 0 END,
            last_connected_utc DESC,
            connect_count DESC,
            name ASC;
    )sql";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return devices;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    BTDevice d;
    d.id = sqlite3_column_int(stmt, 0);
    d.macAddress = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    d.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    d.deviceType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    d.trusted = sqlite3_column_int(stmt, 4) != 0;
    d.paired = sqlite3_column_int(stmt, 5) != 0;
    d.blocked = sqlite3_column_int(stmt, 6) != 0;
    d.connected = sqlite3_column_int(stmt, 7) != 0;
    d.discovered = sqlite3_column_int(stmt, 8) != 0;
    d.lastSeenUtc =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));
    d.lastConnectedUtc =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10));
    d.connectCount = sqlite3_column_int(stmt, 11);
    devices.push_back(d);
  }

  sqlite3_finalize(stmt);
  return devices;
}

std::vector<BTDevice> BTControl::getDiscoveredDevices() const {
  std::vector<BTDevice> devices;

  SqliteDB db(m_dbPath);
  if (!db.valid())
    return devices;

  const char *sql = R"sql(
        SELECT id, mac_address, name, device_type, trusted, paired, blocked,
               connected, discovered, last_seen_utc, last_connected_utc, connect_count
        FROM bluetooth_devices
        WHERE discovered = 1
        ORDER BY name ASC, mac_address ASC;
    )sql";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return devices;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    BTDevice d;
    d.id = sqlite3_column_int(stmt, 0);
    d.macAddress = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    d.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    d.deviceType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    d.trusted = sqlite3_column_int(stmt, 4) != 0;
    d.paired = sqlite3_column_int(stmt, 5) != 0;
    d.blocked = sqlite3_column_int(stmt, 6) != 0;
    d.connected = sqlite3_column_int(stmt, 7) != 0;
    d.discovered = sqlite3_column_int(stmt, 8) != 0;
    d.lastSeenUtc =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));
    d.lastConnectedUtc =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10));
    d.connectCount = sqlite3_column_int(stmt, 11);
    devices.push_back(d);
  }

  sqlite3_finalize(stmt);
  return devices;
}

std::optional<BTDevice>
BTControl::getDeviceByMac(const std::string &macAddress) const {
  if (!isValidMacAddress(macAddress))
    return std::nullopt;

  SqliteDB db(m_dbPath);
  if (!db.valid())
    return std::nullopt;

  const char *sql = R"sql(
        SELECT id, mac_address, name, device_type, trusted, paired, blocked,
               connected, discovered, last_seen_utc, last_connected_utc, connect_count
        FROM bluetooth_devices
        WHERE mac_address = ?;
    )sql";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return std::nullopt;

  sqlite3_bind_text(stmt, 1, macAddress.c_str(), -1, SQLITE_TRANSIENT);

  BTDevice d;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    d.id = sqlite3_column_int(stmt, 0);
    d.macAddress = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    d.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    d.deviceType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    d.trusted = sqlite3_column_int(stmt, 4) != 0;
    d.paired = sqlite3_column_int(stmt, 5) != 0;
    d.blocked = sqlite3_column_int(stmt, 6) != 0;
    d.connected = sqlite3_column_int(stmt, 7) != 0;
    d.discovered = sqlite3_column_int(stmt, 8) != 0;
    d.lastSeenUtc =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));
    d.lastConnectedUtc =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 10));
    d.connectCount = sqlite3_column_int(stmt, 11);

    sqlite3_finalize(stmt);
    return d;
  }

  sqlite3_finalize(stmt);
  return std::nullopt;
}

std::optional<BTDevice> BTControl::getBestDevice() const {
  auto devices = getDevicesRankedByLastConnected();
  if (devices.empty())
    return std::nullopt;
  return devices.front();
}

bool BTControl::pairDevice(const std::string &macAddress) {
  if (!isValidMacAddress(macAddress)) {
    setError("Invalid bluetooth address");
    return false;
  }

  if (!isPoweredOn()) {
    setError("Bluetooth is off");
    return false;
  }

  std::string out;
  if (!runCommand("bluetoothctl pair " + macAddress, &out)) {
    setError("Pair failed. Phone may need approval.");
    return false;
  }

  if (!refreshDeviceInfo(macAddress)) {
    setError("Paired, but failed to refresh device info");
    return false;
  }

  auto device = getDeviceByMac(macAddress);
  if (!device || !device->paired) {
    setError("Phone did not finish pairing");
    return false;
  }

  if (!device->trusted)
    trustDevice(macAddress);

  setStatus("Paired: " + (device->name.empty() ? macAddress : device->name));
  return true;
}

bool BTControl::trustDevice(const std::string &macAddress) {
  if (!isValidMacAddress(macAddress)) {
    setError("Invalid bluetooth address");
    return false;
  }

  std::string out;
  if (!runCommand("bluetoothctl trust " + macAddress, &out)) {
    setError("Failed to trust device");
    return false;
  }

  if (!refreshDeviceInfo(macAddress)) {
    setError("Trusted, but failed to refresh device info");
    return false;
  }

  auto device = getDeviceByMac(macAddress);
  if (!device || !device->trusted) {
    setError("Device did not report trusted");
    return false;
  }

  setStatus("Trusted: " + (device->name.empty() ? macAddress : device->name));
  return true;
}

bool BTControl::connectDevice(const std::string &macAddress) {
  if (!isValidMacAddress(macAddress)) {
    setError("Invalid bluetooth address");
    return false;
  }

  if (!isPoweredOn()) {
    setError("Bluetooth is off");
    return false;
  }

  auto known = getDeviceByMac(macAddress);
  if (known && !known->trusted) {
    trustDevice(macAddress);
  }

  std::string out;
  if (!runCommand("bluetoothctl connect " + macAddress, &out)) {
    setError("Connect failed");
    return false;
  }

  if (!refreshDeviceInfo(macAddress)) {
    setError("Connected, but failed to refresh device info");
    return false;
  }

  auto device = getDeviceByMac(macAddress);
  if (!device || !device->connected) {
    setError("Device did not report connected");
    return false;
  }

  if (!markConnectedNow(macAddress)) {
    setError("Connected, but failed to update database");
    return false;
  }

  setStatus("Connected: " + (device->name.empty() ? macAddress : device->name));
  return true;
}
bool BTControl::disconnectDevice(const std::string &macAddress) {
  if (!isValidMacAddress(macAddress)) {
    LOG_ERROR() << "disconnectDevice invalid MAC: " << macAddress;
    return false;
  }

  std::string out;
  if (!runCommand("bluetoothctl disconnect " + macAddress, &out))
    return false;

  refreshDeviceInfo(macAddress);
  setConnectedState(macAddress, false);

  auto device = getDeviceByMac(macAddress);
  return !device || !device->connected;
}

bool BTControl::disconnectAllDevices() {
  bool ok = true;

  if (!isPoweredOn()) {
    LOG_INFO() << "disconnectAllDevices skipped because bluetooth is off";
    return true;
  }

  auto devices = getDevicesRankedByLastConnected();

  for (const auto &device : devices) {
    if (!device.connected)
      continue;

    LOG_INFO() << "Disconnecting BT device: " << device.name << " ["
               << device.macAddress << "]";

    if (!disconnectDevice(device.macAddress))
      ok = false;
  }

  return ok;
}

bool BTControl::removeDevice(const std::string &macAddress) {
  if (!isValidMacAddress(macAddress)) {
    LOG_ERROR() << "removeDevice invalid MAC: " << macAddress;
    return false;
  }

  std::string out;
  if (!runCommand("bluetoothctl remove " + macAddress, &out))
    return false;

  SqliteDB db(m_dbPath);
  if (!db.valid())
    return false;

  const char *sql = "DELETE FROM bluetooth_devices WHERE mac_address = ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, macAddress.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);

  if (ok)
    LOG_INFO() << "BT device removed from system and DB: " << macAddress;

  return ok;
}

bool BTControl::autoReconnectBestDevice() {
  auto devices = getDevicesRankedByLastConnected();

  for (const auto &device : devices) {
    if (device.blocked)
      continue;
    if (!device.paired)
      continue;

    if (!device.trusted)
      trustDevice(device.macAddress);

    if (connectDevice(device.macAddress))
      return true;
  }

  setError("No paired device could be connected");
  return false;
}

bool BTControl::trustAllPairedDevices() {
  if (!isPoweredOn()) {
    LOG_WARN() << "trustAllPairedDevices called while bluetooth is powered off";
    return false;
  }

  if (!scanPairedDevices()) {
    LOG_WARN() << "trustAllPairedDevices failed to refresh paired devices";
    return false;
  }

  bool ok = true;
  auto devices = getDevicesRankedByLastConnected();

  for (const auto &device : devices) {
    if (!device.paired)
      continue;

    if (device.trusted)
      continue;

    LOG_INFO() << "Auto-trusting paired BT device: " << device.name << " ["
               << device.macAddress << "]";

    if (!trustDevice(device.macAddress))
      ok = false;
  }

  return ok;
}
void BTControl::setStatus(const std::string &msg) const {
  m_lastStatus = msg;
  m_lastError.clear();
  LOG_INFO() << "BT status: " << msg;
}

void BTControl::setError(const std::string &msg) const {
  m_lastError = msg;
  LOG_ERROR() << "BT error: " << msg;
}

const std::string &BTControl::lastStatus() const { return m_lastStatus; }
const std::string &BTControl::lastError() const { return m_lastError; }
