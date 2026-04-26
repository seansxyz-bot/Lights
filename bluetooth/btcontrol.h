#pragma once

#include "bluezclient.h"

#include "bluezagent.h"
#include <optional>
#include <sigc++/sigc++.h>
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
  int displayOrder = 0;
  std::string lastSeenUtc;
  std::string lastConnectedUtc;
  int connectCount = 0;
};

class BTControl {
public:
  explicit BTControl(const std::string &dbPath);
  ~BTControl() = default;

  bool init();

  bool powerOn();
  bool powerOnForManualControl();
  bool powerOff();
  bool isPoweredOn() const;

  bool enableAgent(const std::string &capability = "NoInputNoOutput");
  bool setDefaultAgent();
  bool setPairable(bool enabled);
  bool setDiscoverable(bool enabled);

  bool startScan();
  bool stopScan();
  bool scanPairedDevices();
  bool scanAvailableDevices();
  bool refreshDeviceInfo(const std::string &macAddress);
  bool refreshAllKnownDevices();

  std::vector<BTDevice> getDevicesRankedByLastConnected() const;
  std::vector<BTDevice> getSavedDevicesInDisplayOrder() const;
  std::vector<BTDevice> getDevicesByAutoConnectPriority() const;
  std::vector<BTDevice> getDiscoveredDevices() const;
  std::optional<BTDevice> getBestDevice() const;
  std::optional<BTDevice> getConnectedDevice() const;
  std::optional<BTDevice> getDeviceByMac(const std::string &macAddress) const;

  bool pairDevice(const std::string &macAddress);
  bool trustDevice(const std::string &macAddress);
  bool connectDevice(const std::string &macAddress);
  bool connectSavedDevice(const std::string &macAddress);
  bool disconnectDevice(const std::string &macAddress);
  bool disconnectAllDevices();
  bool removeDevice(const std::string &macAddress);
  bool deleteSavedDevice(const std::string &macAddress);

  bool trustAllPairedDevices();
  bool autoReconnectBestDevice();
  bool connectNextSavedDevice();

  const std::string &lastStatus() const;
  const std::string &lastError() const;

  sigc::signal<void, bool> &signal_power_changed() {
    return m_signalPowerChanged;
  }

private:
  std::string m_dbPath;
  mutable std::string m_lastStatus;
  mutable std::string m_lastError;

  mutable BluezClient m_bluez;
  BluezAgent m_agent;

  sigc::signal<void, bool> m_signalPowerChanged;

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
  static BTDevice fromBluezDevice(const BluezDeviceInfo &d);
};
