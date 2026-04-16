#pragma once

#include <gio/gio.h>
#include <string>

class BluezAgent {
public:
  BluezAgent();
  ~BluezAgent();

  bool start(const std::string &capability = "NoInputNoOutput");
  void stop();

  bool isActive() const;
  const std::string &lastError() const;

  static void handleMethodCall(GDBusConnection *connection, const gchar *sender,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *method_name, GVariant *parameters,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data);

private:
  static constexpr const char *kObjectPath = "/com/seansxyz/LightsAgent";
  static constexpr const char *kBluezService = "org.bluez";
  static constexpr const char *kBluezAgentManagerPath = "/org/bluez";
  static constexpr const char *kBluezAgentManagerInterface =
      "org.bluez.AgentManager1";
  static constexpr const char *kAgentInterface = "org.bluez.Agent1";

  GDBusConnection *m_connection = nullptr;
  GDBusNodeInfo *m_nodeInfo = nullptr;
  guint m_registrationId = 0;
  std::string m_capability;
  std::string m_lastError;

  bool registerObject();
  bool registerWithBluez();
  void unregisterFromBluez();
  void clearError();
  void setError(const std::string &msg);
};
