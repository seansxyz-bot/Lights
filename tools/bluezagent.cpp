#include "bluezagent.h"

#include "logger.h"

#include <sstream>

namespace {

const char *kAgentIntrospectionXml = R"XML(
<node>
  <interface name="org.bluez.Agent1">
    <method name="Release"/>
    <method name="RequestPinCode">
      <arg name="device" type="o" direction="in"/>
      <arg name="pincode" type="s" direction="out"/>
    </method>
    <method name="DisplayPinCode">
      <arg name="device" type="o" direction="in"/>
      <arg name="pincode" type="s" direction="in"/>
    </method>
    <method name="RequestPasskey">
      <arg name="device" type="o" direction="in"/>
      <arg name="passkey" type="u" direction="out"/>
    </method>
    <method name="DisplayPasskey">
      <arg name="device" type="o" direction="in"/>
      <arg name="passkey" type="u" direction="in"/>
      <arg name="entered" type="q" direction="in"/>
    </method>
    <method name="RequestConfirmation">
      <arg name="device" type="o" direction="in"/>
      <arg name="passkey" type="u" direction="in"/>
    </method>
    <method name="RequestAuthorization">
      <arg name="device" type="o" direction="in"/>
    </method>
    <method name="AuthorizeService">
      <arg name="device" type="o" direction="in"/>
      <arg name="uuid" type="s" direction="in"/>
    </method>
    <method name="Cancel"/>
  </interface>
</node>
)XML";

const GDBusInterfaceVTable kAgentVTable = {
    &BluezAgent::handleMethodCall,
    nullptr,
    nullptr,
};

} // namespace

BluezAgent::BluezAgent() = default;

BluezAgent::~BluezAgent() { stop(); }

bool BluezAgent::start(const std::string &capability) {
  clearError();

  const std::string wanted =
      capability.empty() ? "NoInputNoOutput" : capability;

  if (isActive()) {
    if (m_capability == wanted)
      return true;

    stop();
  }

  m_capability = wanted;

  GError *error = nullptr;
  m_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (!m_connection) {
    setError(std::string("Failed to connect to system bus: ") +
             (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
    return false;
  }

  if (!registerObject()) {
    stop();
    return false;
  }

  if (!registerWithBluez()) {
    stop();
    return false;
  }

  return true;
}

void BluezAgent::stop() {
  unregisterFromBluez();

  if (m_connection && m_registrationId != 0) {
    g_dbus_connection_unregister_object(m_connection, m_registrationId);
    m_registrationId = 0;
  }

  if (m_nodeInfo) {
    g_dbus_node_info_unref(m_nodeInfo);
    m_nodeInfo = nullptr;
  }

  if (m_connection) {
    g_object_unref(m_connection);
    m_connection = nullptr;
  }

  m_capability.clear();
}

bool BluezAgent::isActive() const {
  return m_connection != nullptr && m_registrationId != 0;
}

const std::string &BluezAgent::lastError() const { return m_lastError; }

bool BluezAgent::registerObject() {
  GError *error = nullptr;
  m_nodeInfo = g_dbus_node_info_new_for_xml(kAgentIntrospectionXml, &error);
  if (!m_nodeInfo) {
    setError(std::string("Failed to parse agent introspection XML: ") +
             (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
    return false;
  }

  GDBusInterfaceInfo *iface =
      g_dbus_node_info_lookup_interface(m_nodeInfo, kAgentInterface);
  if (!iface) {
    setError("Failed to find org.bluez.Agent1 in introspection data");
    return false;
  }

  m_registrationId = g_dbus_connection_register_object(
      m_connection, kObjectPath, iface, &kAgentVTable, this, nullptr, &error);

  if (m_registrationId == 0) {
    setError(std::string("Failed to register D-Bus agent object: ") +
             (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
    return false;
  }

  return true;
}

bool BluezAgent::registerWithBluez() {
  GError *error = nullptr;

  GVariant *result = g_dbus_connection_call_sync(
      m_connection, kBluezService, kBluezAgentManagerPath,
      kBluezAgentManagerInterface, "RegisterAgent",
      g_variant_new("(os)", kObjectPath, m_capability.c_str()), nullptr,
      G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

  if (!result) {
    setError(std::string("RegisterAgent failed: ") +
             (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
    return false;
  }
  g_variant_unref(result);
  LOG_INFO() << "BluezAgent RegisterAgent ok path=" << kObjectPath
             << " capability=" << m_capability;

  error = nullptr;
  result = g_dbus_connection_call_sync(
      m_connection, kBluezService, kBluezAgentManagerPath,
      kBluezAgentManagerInterface, "RequestDefaultAgent",
      g_variant_new("(o)", kObjectPath), nullptr, G_DBUS_CALL_FLAGS_NONE, -1,
      nullptr, &error);

  if (!result) {
    setError(std::string("RequestDefaultAgent failed: ") +
             (error ? error->message : "unknown error"));
    if (error)
      g_error_free(error);
    return false;
  }
  g_variant_unref(result);
  LOG_INFO() << "BluezAgent RequestDefaultAgent ok path=" << kObjectPath;

  return true;
}

void BluezAgent::unregisterFromBluez() {
  if (!m_connection)
    return;

  GError *error = nullptr;
  GVariant *result = g_dbus_connection_call_sync(
      m_connection, kBluezService, kBluezAgentManagerPath,
      kBluezAgentManagerInterface, "UnregisterAgent",
      g_variant_new("(o)", kObjectPath), nullptr, G_DBUS_CALL_FLAGS_NONE, 1000,
      nullptr, &error);

  if (result)
    g_variant_unref(result);

  if (error) {
    LOG_WARN() << "UnregisterAgent warning: " << error->message;
    g_error_free(error);
  }
}

void BluezAgent::clearError() { m_lastError.clear(); }

void BluezAgent::setError(const std::string &msg) {
  m_lastError = msg;
  LOG_ERROR() << "BluezAgent: " << msg;
}

void BluezAgent::handleMethodCall(
    GDBusConnection *connection, const gchar *sender, const gchar *object_path,
    const gchar *interface_name, const gchar *method_name, GVariant *parameters,
    GDBusMethodInvocation *invocation, gpointer user_data) {
  (void)connection;
  (void)sender;
  (void)object_path;
  (void)interface_name;

  auto *self = static_cast<BluezAgent *>(user_data);
  if (!self) {
    g_dbus_method_invocation_return_dbus_error(
        invocation, "org.bluez.Error.Failed", "Agent instance missing");
    return;
  }

  const std::string method = method_name ? method_name : "";
  LOG_WARN() << "BluezAgent::handleMethodCall method=" << method;

  if (method == "Release") {
    LOG_WARN() << "BluezAgent::Release";
    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (method == "Cancel") {
    LOG_WARN() << "BluezAgent::Cancel";
    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (method == "RequestConfirmation") {
    const gchar *device = nullptr;
    guint32 passkey = 0;
    g_variant_get(parameters, "(&ou)", &device, &passkey);

    LOG_WARN() << "BluezAgent::RequestConfirmation device="
               << (device ? device : "") << " passkey=" << passkey;

    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (method == "RequestAuthorization") {
    const gchar *device = nullptr;
    g_variant_get(parameters, "(&o)", &device);

    LOG_WARN() << "BluezAgent::RequestAuthorization device="
               << (device ? device : "");

    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (method == "AuthorizeService") {
    const gchar *device = nullptr;
    const gchar *uuid = nullptr;
    g_variant_get(parameters, "(&o&s)", &device, &uuid);

    LOG_WARN() << "BluezAgent::AuthorizeService device="
               << (device ? device : "") << " uuid=" << (uuid ? uuid : "");

    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (method == "DisplayPinCode") {
    const gchar *device = nullptr;
    const gchar *pincode = nullptr;
    g_variant_get(parameters, "(&o&s)", &device, &pincode);

    LOG_WARN() << "BluezAgent::DisplayPinCode device=" << (device ? device : "")
               << " pincode=" << (pincode ? pincode : "");

    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (method == "DisplayPasskey") {
    const gchar *device = nullptr;
    guint32 passkey = 0;
    guint16 entered = 0;
    g_variant_get(parameters, "(&ouq)", &device, &passkey, &entered);

    LOG_WARN() << "BluezAgent::DisplayPasskey device=" << (device ? device : "")
               << " passkey=" << passkey << " entered=" << entered;

    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (method == "RequestPinCode") {
    const gchar *device = nullptr;
    g_variant_get(parameters, "(&o)", &device);

    LOG_WARN() << "BluezAgent::RequestPinCode rejected device="
               << (device ? device : "");

    g_dbus_method_invocation_return_dbus_error(
        invocation, "org.bluez.Error.Rejected", "No input available");
    return;
  }

  if (method == "RequestPasskey") {
    const gchar *device = nullptr;
    g_variant_get(parameters, "(&o)", &device);

    LOG_WARN() << "BluezAgent::RequestPasskey rejected device="
               << (device ? device : "");

    g_dbus_method_invocation_return_dbus_error(
        invocation, "org.bluez.Error.Rejected", "No input available");
    return;
  }

  LOG_WARN() << "BluezAgent::UnknownMethod method=" << method;
  g_dbus_method_invocation_return_dbus_error(
      invocation, "org.freedesktop.DBus.Error.UnknownMethod",
      "Unknown agent method");
}
