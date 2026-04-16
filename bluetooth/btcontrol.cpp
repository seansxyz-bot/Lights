#include "btcontrol.h"

#include "../utils/logger.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
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

BTControl::BTControl(const std::string &dbPath)
    : m_dbPath(dbPath + "lights.db") {
  system(
      std::string("sudo cp " + dbPath + "/main.conf /etc/bluetooth/main.conf")
          .c_str());
}

bool BTControl::init() {
  LOG_INFO() << "BTControl init dbPath=" << m_dbPath;
  if (!createTables())
    return false;

  if (!m_bluez.init()) {
    LOG_WARN() << "BluezClient init failed: " << m_bluez.lastError();
    // Keep DB usable even if bluetooth stack is down.
  }

  return true;
}

BTDevice BTControl::fromBluezDevice(const BluezDeviceInfo &d) {
  BTDevice out;
  out.macAddress = d.macAddress;
  out.name = !d.name.empty() ? d.name : d.alias;
  out.deviceType = d.icon;
  out.trusted = d.trusted;
  out.paired = d.paired;
  out.blocked = d.blocked;
  out.connected = d.connected;
  out.discovered = d.discovered;
  out.lastSeenUtc = nowUtc();
  return out;
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
  bool powered = false;

  if (!m_bluez.isPoweredOn(&powered)) {
    setError(m_bluez.lastError().empty() ? "Failed to query bluetooth power"
                                         : m_bluez.lastError());
    return false;
  }

  if (!powered) {
    LOG_INFO() << "BT powerOn: setting Powered=true";
    if (!m_bluez.setPowered(true)) {
      setError(m_bluez.lastError().empty() ? "Failed to power on bluetooth"
                                           : m_bluez.lastError());
      return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (!m_bluez.isPoweredOn(&powered) || !powered) {
      setError("Bluetooth did not report powered on");
      return false;
    }
  }

  if (!setSystemAlias("Light Controller")) {
    LOG_WARN() << "Bluetooth powered on but failed to set alias";
  }

  if (!enableAgent("NoInputNoOutput")) {
    LOG_WARN() << "Failed to enable BT agent: " << lastError();
  }

  if (!setPairable(true)) {
    LOG_WARN() << "Failed to set pairable";
  }

  if (!scanPairedDevices()) {
    LOG_WARN() << "Failed to scan paired devices after power on";
  }

  if (!trustAllPairedDevices()) {
    LOG_WARN() << "Failed to trust paired devices after power on";
  }

  if (!autoReconnectBestDevice()) {
    LOG_WARN() << "No paired BT device auto-reconnected: " << lastError();
  }

  setStatus("Bluetooth on");
  m_signalPowerChanged.emit(true);
  return true;
}

bool BTControl::powerOff() {
  bool powered = true;
  if (m_bluez.isPoweredOn(&powered) && !powered) {
    setStatus("Bluetooth off");
    m_signalPowerChanged.emit(false);
    return true;
  }

  if (!m_bluez.setPowered(false)) {
    setError(m_bluez.lastError().empty() ? "Failed to power off bluetooth"
                                         : m_bluez.lastError());
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  if (!m_bluez.isPoweredOn(&powered) || powered) {
    setError("Bluetooth did not report powered off");
    return false;
  }

  setStatus("Bluetooth off");
  m_signalPowerChanged.emit(false);
  return true;
}

bool BTControl::isPoweredOn() const {
  bool powered = false;
  if (!m_bluez.isPoweredOn(&powered)) {
    LOG_WARN() << "BT isPoweredOn failed: " << m_bluez.lastError();
    return false;
  }
  return powered;
}

bool BTControl::enableAgent(const std::string &capability) {
  if (m_agent.isActive())
    return true;

  if (!m_agent.start(capability)) {
    setError(m_agent.lastError().empty() ? "Failed to start BT agent"
                                         : m_agent.lastError());
    return false;
  }

  return true;
}

bool BTControl::setDefaultAgent() {
  if (!m_agent.isActive()) {
    setError("BT agent is not active");
    return false;
  }
  return true;
}

bool BTControl::setPairable(bool enabled) {
  if (!m_bluez.setPairable(enabled)) {
    setError(m_bluez.lastError().empty() ? "Failed to set pairable"
                                         : m_bluez.lastError());
    return false;
  }
  return true;
}

bool BTControl::setDiscoverable(bool enabled) {
  if (!m_bluez.setDiscoverable(enabled)) {
    setError(m_bluez.lastError().empty() ? "Failed to set discoverable"
                                         : m_bluez.lastError());
    return false;
  }
  return true;
}

bool BTControl::startScan() {
  if (!isPoweredOn())
    return false;

  if (!m_bluez.startDiscovery()) {
    setError(m_bluez.lastError().empty() ? "Failed to start scan"
                                         : m_bluez.lastError());
    return false;
  }

  return true;
}

bool BTControl::stopScan() {
  if (!m_bluez.stopDiscovery()) {
    setError(m_bluez.lastError().empty() ? "Failed to stop scan"
                                         : m_bluez.lastError());
    return false;
  }

  return true;
}

bool BTControl::scanPairedDevices() {
  if (!isPoweredOn()) {
    LOG_WARN() << "scanPairedDevices called while bluetooth is powered off";
    return false;
  }

  auto devices = m_bluez.getDevices();
  for (const auto &d : devices) {
    if (!d.paired)
      continue;

    auto old = getDeviceByMac(d.macAddress);

    BTDevice full = fromBluezDevice(d);
    full.discovered = true;
    upsertDevice(full);

    const std::string displayName =
        full.name.empty() ? full.macAddress : full.name;

    if (!old.has_value()) {
      if (full.connected) {
        setStatus("Connected: " + displayName);
      }
      continue;
    }

    if (!old->connected && full.connected) {
      setStatus("Connected: " + displayName);
    }
  }

  return true;
}

bool BTControl::scanAvailableDevices() {
  if (!isPoweredOn()) {
    LOG_WARN() << "scanAvailableDevices called while bluetooth is powered off";
    return false;
  }

  auto devices = m_bluez.getDevices();
  for (const auto &d : devices) {
    if (!d.paired)
      continue;

    BTDevice full = fromBluezDevice(d);
    full.discovered = true;
    upsertDevice(full);
  }

  return true;
}

bool BTControl::setSystemAlias(const std::string &name) {
  if (!m_bluez.setAlias(name)) {
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

  auto device = m_bluez.getDeviceByAddress(macAddress);
  if (!device)
    return false;

  BTDevice d = fromBluezDevice(*device);
  d.discovered = true;
  return upsertDevice(d);
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

  if (!m_bluez.pairDevice(macAddress)) {
    setError(m_bluez.lastError().empty() ? "Pair failed" : m_bluez.lastError());
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

  if (!m_bluez.setTrusted(macAddress, true)) {
    setError(m_bluez.lastError().empty() ? "Failed to trust device"
                                         : m_bluez.lastError());
    return false;
  }

  if (!refreshDeviceInfo(macAddress)) {
    setError("Trusted device, but failed to refresh device info");
    return false;
  }
  auto device = getDeviceByMac(macAddress);
  setStatus("Connected: " +
            ((device && !device->name.empty()) ? device->name : macAddress));
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

  std::string ignore;
  (void)ignore;

  if (!m_bluez.connectDevice(macAddress)) {
    setError(m_bluez.lastError().empty() ? "Connect failed"
                                         : m_bluez.lastError());
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

  if (!m_bluez.disconnectDevice(macAddress)) {
    setError(m_bluez.lastError().empty() ? "Disconnect failed"
                                         : m_bluez.lastError());
    return false;
  }

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

  if (!m_bluez.removeDevice(macAddress)) {
    setError(m_bluez.lastError().empty() ? "Failed to remove device"
                                         : m_bluez.lastError());
    return false;
  }

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
  if (!isPoweredOn()) {
    setError("Bluetooth is off");
    return false;
  }

  scanPairedDevices();

  auto devices = getDevicesRankedByLastConnected();

  for (const auto &device : devices) {
    if (device.blocked)
      continue;
    if (!device.paired)
      continue;

    if (device.connected)
      return true;

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
