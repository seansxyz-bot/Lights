#include "bluezclient.h"

#include "../utils/logger.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

static bool isIgnorableNotReady(const std::string &err) {
  return err.find("org.bluez.Error.NotReady") != std::string::npos;
}

static bool isIgnorableAlready(const std::string &err) {
  return err.find("org.bluez.Error.InProgress") != std::string::npos ||
         err.find("org.bluez.Error.AlreadyExists") != std::string::npos;
}

} // namespace

BluezClient::BluezClient() = default;

BluezClient::~BluezClient() {
  if (m_connection) {
    g_object_unref(m_connection);
    m_connection = nullptr;
  }
}

bool BluezClient::init() {
  clearError();

  if (m_connection)
    return ensureAdapterPath();

  GError *error = nullptr;
  m_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (!m_connection) {
    setError(std::string("Failed to connect to system bus: ") +
             (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
    return false;
  }

  return ensureAdapterPath();
}

bool BluezClient::isReady() const {
  return m_connection != nullptr && !m_adapterPath.empty();
}

const std::string &BluezClient::lastError() const { return m_lastError; }

void BluezClient::clearError() const { m_lastError.clear(); }

void BluezClient::setError(const std::string &msg) const {
  m_lastError = msg;
  LOG_ERROR() << "BluezClient: " << msg;
}

std::string BluezClient::normalizeAddress(const std::string &macAddress) {
  std::string out = macAddress;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return out;
}

std::string BluezClient::variantToString(GVariant *v) {
  if (!v)
    return "";

  std::string out;
  if (g_variant_is_of_type(v, G_VARIANT_TYPE_STRING)) {
    out = g_variant_get_string(v, nullptr);
  }

  g_variant_unref(v);
  return out;
}

bool BluezClient::variantToBool(GVariant *v, bool fallback) {
  if (!v)
    return fallback;

  bool out = fallback;
  if (g_variant_is_of_type(v, G_VARIANT_TYPE_BOOLEAN)) {
    out = g_variant_get_boolean(v);
  }

  g_variant_unref(v);
  return out;
}

GVariant *BluezClient::callMethodSync(const char *objectPath,
                                      const char *interfaceName,
                                      const char *methodName,
                                      GVariant *parameters,
                                      int timeoutMs) const {
  if (!m_connection) {
    setError("D-Bus connection not initialized");
    return nullptr;
  }

  GError *error = nullptr;
  GVariant *result = g_dbus_connection_call_sync(
      m_connection, kBluezService, objectPath, interfaceName, methodName,
      parameters, nullptr, G_DBUS_CALL_FLAGS_NONE, timeoutMs, nullptr, &error);

  if (!result) {
    setError(std::string(interfaceName) + "." + methodName +
             " failed: " + (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
  }

  return result;
}

GVariant *BluezClient::getProperty(const char *objectPath,
                                   const char *interfaceName,
                                   const char *propertyName) const {
  GVariant *result = callMethodSync(
      objectPath, kPropertiesInterface, "Get",
      g_variant_new("(ss)", interfaceName, propertyName), kDefaultTimeoutMs);

  if (!result)
    return nullptr;

  GVariant *value = nullptr;
  g_variant_get(result, "(v)", &value);
  g_variant_unref(result);
  return value;
}

bool BluezClient::setProperty(const char *objectPath, const char *interfaceName,
                              const char *propertyName, GVariant *value) {
  GVariant *result =
      callMethodSync(objectPath, kPropertiesInterface, "Set",
                     g_variant_new("(ssv)", interfaceName, propertyName, value),
                     kDefaultTimeoutMs);

  if (!result)
    return false;

  g_variant_unref(result);
  clearError();
  return true;
}

bool BluezClient::ensureAdapterPath() {
  clearError();

  if (!m_connection) {
    setError("D-Bus connection not initialized");
    return false;
  }

  GError *error = nullptr;
  GVariant *result = g_dbus_connection_call_sync(
      m_connection, kBluezService, "/", kObjectManagerInterface,
      "GetManagedObjects", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE,
      kMethodTimeoutMs, nullptr, &error);

  if (!result) {
    setError(std::string("GetManagedObjects failed: ") +
             (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
    return false;
  }

  GVariantIter *objects = nullptr;
  g_variant_get(result, "(a{oa{sa{sv}}})", &objects);

  gchar *objectPath = nullptr;
  GVariantIter *ifaces = nullptr;

  while (g_variant_iter_next(objects, "{oa{sa{sv}}}", &objectPath, &ifaces)) {
    gchar *ifaceName = nullptr;
    GVariantIter *props = nullptr;

    while (g_variant_iter_next(ifaces, "{sa{sv}}", &ifaceName, &props)) {
      if (std::string(ifaceName) == kAdapterInterface) {
        m_adapterPath = objectPath;
        g_variant_iter_free(props);
        g_free(ifaceName);

        g_variant_iter_free(ifaces);
        g_free(objectPath);

        g_variant_iter_free(objects);
        g_variant_unref(result);
        clearError();
        return true;
      }

      g_variant_iter_free(props);
      g_free(ifaceName);
    }

    g_variant_iter_free(ifaces);
    g_free(objectPath);
  }

  g_variant_iter_free(objects);
  g_variant_unref(result);

  setError("No BlueZ adapter found");
  return false;
}

bool BluezClient::isPoweredOn(bool *poweredOut) const {
  GVariant *value =
      getProperty(m_adapterPath.c_str(), kAdapterInterface, "Powered");
  if (!value)
    return false;

  const bool powered = variantToBool(value, false);
  if (poweredOut)
    *poweredOut = powered;
  clearError();
  return true;
}

bool BluezClient::setPowered(bool enabled) {
  if (!ensureAdapterPath())
    return false;
  return setProperty(m_adapterPath.c_str(), kAdapterInterface, "Powered",
                     g_variant_new_boolean(enabled));
}

bool BluezClient::setPairable(bool enabled) {
  if (!ensureAdapterPath())
    return false;
  return setProperty(m_adapterPath.c_str(), kAdapterInterface, "Pairable",
                     g_variant_new_boolean(enabled));
}

bool BluezClient::setDiscoverable(bool enabled) {
  if (!ensureAdapterPath())
    return false;
  return setProperty(m_adapterPath.c_str(), kAdapterInterface, "Discoverable",
                     g_variant_new_boolean(enabled));
}

bool BluezClient::setAlias(const std::string &name) {
  if (!ensureAdapterPath())
    return false;
  return setProperty(m_adapterPath.c_str(), kAdapterInterface, "Alias",
                     g_variant_new_string(name.c_str()));
}

bool BluezClient::startDiscovery() {
  if (!ensureAdapterPath())
    return false;

  GVariant *result =
      callMethodSync(m_adapterPath.c_str(), kAdapterInterface, "StartDiscovery",
                     nullptr, kMethodTimeoutMs);

  if (!result) {
    const std::string errMsg = m_lastError;

    if (isIgnorableAlready(errMsg)) {
      LOG_INFO() << "StartDiscovery ignored (already running)";
      clearError();
      return true;
    }

    return false;
  }

  g_variant_unref(result);
  clearError();
  return true;
}

bool BluezClient::stopDiscovery() {
  if (!ensureAdapterPath())
    return false;

  GVariant *result = callMethodSync(m_adapterPath.c_str(), kAdapterInterface,
                                    "StopDiscovery", nullptr, kMethodTimeoutMs);

  if (!result) {
    const std::string errMsg = m_lastError;

    if (isIgnorableNotReady(errMsg)) {
      LOG_INFO() << "StopDiscovery ignored (already stopped / not ready)";
      clearError();
      return true;
    }

    return false;
  }

  g_variant_unref(result);
  clearError();
  return true;
}

std::vector<BluezDeviceInfo> BluezClient::getDevices() const {
  std::vector<BluezDeviceInfo> devices;

  if (!m_connection)
    return devices;

  GError *error = nullptr;
  GVariant *result = g_dbus_connection_call_sync(
      m_connection, kBluezService, "/", kObjectManagerInterface,
      "GetManagedObjects", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE,
      kMethodTimeoutMs, nullptr, &error);

  if (!result) {
    setError(std::string("GetManagedObjects failed: ") +
             (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
    return devices;
  }

  GVariantIter *objects = nullptr;
  g_variant_get(result, "(a{oa{sa{sv}}})", &objects);

  gchar *objectPath = nullptr;
  GVariantIter *ifaces = nullptr;

  while (g_variant_iter_next(objects, "{oa{sa{sv}}}", &objectPath, &ifaces)) {
    gchar *ifaceName = nullptr;
    GVariantIter *props = nullptr;

    while (g_variant_iter_next(ifaces, "{sa{sv}}", &ifaceName, &props)) {
      if (std::string(ifaceName) == kDeviceInterface) {
        BluezDeviceInfo d;
        d.objectPath = objectPath;

        gchar *key = nullptr;
        GVariant *value = nullptr;

        while (g_variant_iter_next(props, "{sv}", &key, &value)) {
          const std::string k = key ? key : "";

          if (k == "Address")
            d.macAddress = g_variant_get_string(value, nullptr);
          else if (k == "Name")
            d.name = g_variant_get_string(value, nullptr);
          else if (k == "Alias")
            d.alias = g_variant_get_string(value, nullptr);
          else if (k == "Icon")
            d.icon = g_variant_get_string(value, nullptr);
          else if (k == "Trusted")
            d.trusted = g_variant_get_boolean(value);
          else if (k == "Paired")
            d.paired = g_variant_get_boolean(value);
          else if (k == "Blocked")
            d.blocked = g_variant_get_boolean(value);
          else if (k == "Connected")
            d.connected = g_variant_get_boolean(value);
          else if (k == "RSSI")
            d.discovered = true;

          g_variant_unref(value);
          g_free(key);
        }

        if (!d.macAddress.empty())
          devices.push_back(d);
      }

      g_variant_iter_free(props);
      g_free(ifaceName);
    }

    g_variant_iter_free(ifaces);
    g_free(objectPath);
  }

  g_variant_iter_free(objects);
  g_variant_unref(result);
  clearError();
  return devices;
}

std::optional<BluezDeviceInfo>
BluezClient::getDeviceByAddress(const std::string &macAddress) const {
  const std::string wanted = normalizeAddress(macAddress);

  auto devices = getDevices();
  for (const auto &d : devices) {
    if (normalizeAddress(d.macAddress) == wanted)
      return d;
  }

  return std::nullopt;
}

bool BluezClient::pairDevice(const std::string &macAddress) {
  auto dev = getDeviceByAddress(macAddress);
  if (!dev) {
    setError("Device not found for pair");
    return false;
  }

  GVariant *result = callMethodSync(dev->objectPath.c_str(), kDeviceInterface,
                                    "Pair", nullptr, kMethodTimeoutMs);
  if (!result)
    return false;

  g_variant_unref(result);
  clearError();
  return true;
}

bool BluezClient::connectDevice(const std::string &macAddress) {
  auto dev = getDeviceByAddress(macAddress);
  if (!dev) {
    setError("Device not found for connect");
    return false;
  }

  GVariant *result = callMethodSync(dev->objectPath.c_str(), kDeviceInterface,
                                    "Connect", nullptr, kMethodTimeoutMs);
  if (!result)
    return false;

  g_variant_unref(result);
  clearError();
  return true;
}

bool BluezClient::disconnectDevice(const std::string &macAddress) {
  auto dev = getDeviceByAddress(macAddress);
  if (!dev) {
    setError("Device not found for disconnect");
    return false;
  }

  GVariant *result = callMethodSync(dev->objectPath.c_str(), kDeviceInterface,
                                    "Disconnect", nullptr, kMethodTimeoutMs);
  if (!result)
    return false;

  g_variant_unref(result);
  clearError();
  return true;
}

bool BluezClient::removeDevice(const std::string &macAddress) {
  if (!ensureAdapterPath())
    return false;

  auto dev = getDeviceByAddress(macAddress);
  if (!dev) {
    setError("Device not found for remove");
    return false;
  }

  GVariant *result = callMethodSync(
      m_adapterPath.c_str(), kAdapterInterface, "RemoveDevice",
      g_variant_new("(o)", dev->objectPath.c_str()), kMethodTimeoutMs);

  if (!result)
    return false;

  g_variant_unref(result);
  clearError();
  return true;
}
bool BluezClient::setTrusted(const std::string &macAddress, bool trusted) {
  auto dev = getDeviceByAddress(macAddress);
  if (!dev) {
    setError("Device not found for trust");
    return false;
  }

  return setProperty(dev->objectPath.c_str(), kDeviceInterface, "Trusted",
                     g_variant_new_boolean(trusted));
}
