#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

class ImageButton;

class Settings : public Gtk::Box {
public:
  Settings(const std::string &iconPath, bool autoSensorOn, bool lightsOn,
           bool bluetoothOn);
  virtual ~Settings() = default;

  void set_restart_enabled(bool enabled);
  void set_bluetooth_enabled(bool enabled);

  sigc::signal<void, bool> &signal_auto_sensor_toggled();
  sigc::signal<void, bool> &signal_lights_toggled();
  sigc::signal<void, bool> &signal_bluetooth_toggled();
  sigc::signal<void> &signal_edit_theme_requested();
  sigc::signal<void> &signal_edit_teams_requested(); // NEW
  sigc::signal<void> &signal_restart_requested();
  sigc::signal<void> &signal_done();

private:
  bool m_autoSensorOn = false;
  bool m_lightsOn = false;
  bool m_bluetoothOn = false;

  ImageButton *m_autoSensorBtn = nullptr;
  ImageButton *m_lightSwitchBtn = nullptr;
  ImageButton *m_bluetoothBtn = nullptr;
  ImageButton *m_editThemeBtn = nullptr;
  ImageButton *m_editTeamsBtn = nullptr; // NEW
  ImageButton *m_restartBtn = nullptr;
  ImageButton *m_okBtn = nullptr;

  sigc::signal<void, bool> m_signalAutoSensorToggled;
  sigc::signal<void, bool> m_signalLightsToggled;
  sigc::signal<void, bool> m_signalBluetoothToggled;
  sigc::signal<void> m_signalEditThemeRequested;
  sigc::signal<void> m_signalEditTeamsRequested; // NEW
  sigc::signal<void> m_signalRestartRequested;
  sigc::signal<void> m_signalDone;
};
