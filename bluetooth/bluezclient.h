#pragma once

#include <gio/gio.h>

#include <optional>
#include <string>
#include <vector>

struct BluezDeviceInfo {
  std::string objectPath;
  std::string macAddress;
  std::string name;
  std::string alias;
  std::string icon;
  bool trusted = false;
  bool paired = false;
  bool blocked = false;
  bool connected = false;
  bool discovered = false;
};

class BluezClient {
public:
  BluezClient();
  ~BluezClient();

  bool init();
  bool isReady() const;

  const std::string &lastError() const;

  bool isPoweredOn(bool *poweredOut = nullptr) const;
  bool setPowered(bool enabled);
  bool setPairable(bool enabled);
  bool setDiscoverable(bool enabled);
  bool setAlias(const std::string &name);

  bool startDiscovery();
  bool stopDiscovery();
  bool setTrusted(const std::string &macAddress, bool trusted = true);

  std::vector<BluezDeviceInfo> getDevices() const;
  std::optional<BluezDeviceInfo>
  getDeviceByAddress(const std::string &macAddress) const;

  bool pairDevice(const std::string &macAddress);
  bool connectDevice(const std::string &macAddress);
  bool disconnectDevice(const std::string &macAddress);
  bool removeDevice(const std::string &macAddress);

private:
  static constexpr const char *kBluezService = "org.bluez";
  static constexpr const char *kObjectManagerInterface =
      "org.freedesktop.DBus.ObjectManager";
  static constexpr const char *kPropertiesInterface =
      "org.freedesktop.DBus.Properties";
  static constexpr const char *kAdapterInterface = "org.bluez.Adapter1";
  static constexpr const char *kDeviceInterface = "org.bluez.Device1";
  static constexpr int kDefaultTimeoutMs = 1500;
  static constexpr int kMethodTimeoutMs = 3000;

  GDBusConnection *m_connection = nullptr;
  std::string m_adapterPath;
  mutable std::string m_lastError;

  void clearError() const;
  void setError(const std::string &msg) const;

  bool ensureAdapterPath();
  GVariant *callMethodSync(const char *objectPath, const char *interfaceName,
                           const char *methodName, GVariant *parameters,
                           int timeoutMs) const;

  GVariant *getProperty(const char *objectPath, const char *interfaceName,
                        const char *propertyName) const;
  bool setProperty(const char *objectPath, const char *interfaceName,
                   const char *propertyName, GVariant *value);

  static std::string normalizeAddress(const std::string &macAddress);
  static std::string variantToString(GVariant *v);
  static bool variantToBool(GVariant *v, bool fallback = false);
};
