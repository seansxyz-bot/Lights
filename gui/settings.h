#pragma once

#include "../models/types.h"
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

#if (SCREEN == 1)
#define SETTINGS_TOP_MARGIN 20
#define SETTINGS_OUTER_SPACING 20
#define SETTINGS_ROW_SPACING 10
#define SETTINGS_TOGGLE_SIZE 256
#define SETTINGS_EDIT_SIZE 196
#define SETTINGS_RESTART_SIZE 160
#define SETTINGS_OK_SIZE 96
#define SETTINGS_OK_BOTTOM_MARGIN 20
#else
#define SETTINGS_TOP_MARGIN 4
#define SETTINGS_OUTER_SPACING 8
#define SETTINGS_ROW_SPACING 8
#define SETTINGS_TOGGLE_SIZE 150
#define SETTINGS_EDIT_SIZE 120
#define SETTINGS_RESTART_SIZE 100
#define SETTINGS_OK_SIZE 60
#define SETTINGS_OK_BOTTOM_MARGIN 4
#endif

class ImageButton;

class Settings : public Gtk::Box {
public:
  Settings(const std::string &iconPath, Options &opt, bool bluetoothOn);
  virtual ~Settings() = default;

  void set_restart_enabled(bool enabled);
  void set_bluetooth_enabled(bool enabled);

  sigc::signal<void, bool> &signal_auto_sensor_toggled();
  sigc::signal<void, bool> &signal_lights_toggled();
  sigc::signal<void, bool> &signal_bluetooth_toggled();
  sigc::signal<void> &signal_edit_theme_requested();
  sigc::signal<void> &signal_edit_pattern_requested();
  sigc::signal<void> &signal_edit_teams_requested();
  sigc::signal<void> &signal_restart_requested();
  sigc::signal<void> &signal_done();

private:
  bool m_autoSensorOn = false;
  bool m_lightsOn = false;
  bool m_bluetoothOn = false;

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_rowA{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_rowB{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_rowC{Gtk::ORIENTATION_HORIZONTAL};

  ImageButton *m_autoSensorBtn = nullptr;
  ImageButton *m_lightSwitchBtn = nullptr;
  ImageButton *m_bluetoothBtn = nullptr;
  ImageButton *m_editThemeBtn = nullptr;
  ImageButton *m_editPatternBtn = nullptr;
  ImageButton *m_editTeamsBtn = nullptr;
  ImageButton *m_restartBtn = nullptr;
  ImageButton *m_okBtn = nullptr;

  sigc::signal<void, bool> m_signalAutoSensorToggled;
  sigc::signal<void, bool> m_signalLightsToggled;
  sigc::signal<void, bool> m_signalBluetoothToggled;
  sigc::signal<void> m_signalEditThemeRequested;
  sigc::signal<void> m_signalEditPatternRequested;
  sigc::signal<void> m_signalEditTeamsRequested;
  sigc::signal<void> m_signalRestartRequested;
  sigc::signal<void> m_signalDone;
};
