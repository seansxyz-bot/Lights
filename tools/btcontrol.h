#pragma once

#include <optional>
#include <string>
#include <vector>

struct BTDevice {
  int id = 0;
  std::string macAddress;
  std::string name;
  std::string deviceType;
  bool trusted = false;
  bool paired = false;
  bool blocked = false;
  bool connected = false;
  bool discovered = false;
  std::string lastSeenUtc;
  std::string lastConnectedUtc;
  int connectCount = 0;
};

class BTControl {
public:
  explicit BTControl(const std::string &dbPath);
  ~BTControl() = default;

  bool init();

  // adapter / power
  bool powerOn();
  bool powerOff();
  bool isPoweredOn() const;

  // pairing / visibility
  bool enableAgent(const std::string &capability = "NoInputNoOutput");
  bool setDefaultAgent();
  bool setPairable(bool enabled);
  bool setDiscoverable(bool enabled);

  // discovery / refresh
  bool startScan();
  bool stopScan();
  bool scanPairedDevices();
  bool scanAvailableDevices();
  bool refreshDeviceInfo(const std::string &macAddress);
  bool refreshAllKnownDevices();

  // database reads
  std::vector<BTDevice> getDevicesRankedByLastConnected() const;
  std::vector<BTDevice> getDiscoveredDevices() const;
  std::optional<BTDevice> getBestDevice() const;
  std::optional<BTDevice> getDeviceByMac(const std::string &macAddress) const;

  // actions
  bool pairDevice(const std::string &macAddress);
  bool trustDevice(const std::string &macAddress);
  bool connectDevice(const std::string &macAddress);
  bool disconnectDevice(const std::string &macAddress);
  bool disconnectAllDevices();
  bool removeDevice(const std::string &macAddress);

  // higher-level behavior
  bool trustAllPairedDevices();
  bool autoReconnectBestDevice();

  const std::string &lastStatus() const;
  const std::string &lastError() const;

private:
  std::string m_dbPath;

  mutable std::string m_lastStatus;
  mutable std::string m_lastError;

  void setStatus(const std::string &msg) const;
  void setError(const std::string &msg) const;

  bool createTables() const;
  bool upsertDevice(const BTDevice &device) const;
  bool markConnectedNow(const std::string &macAddress) const;
  bool markSeenNow(const std::string &macAddress) const;
  bool setConnectedState(const std::string &macAddress, bool connected) const;
  bool setSystemAlias(const std::string &name);

  static std::string nowUtc();
  static std::string trim(const std::string &s);
  static bool isValidMacAddress(const std::string &macAddress);

  static bool runCommand(const std::string &cmd, std::string *output = nullptr);
  static bool parseBluetoothctlDevices(const std::string &text,
                                       std::vector<BTDevice> &devices);
  static bool parseBluetoothctlInfo(const std::string &macAddress,
                                    const std::string &text, BTDevice &device);
};
