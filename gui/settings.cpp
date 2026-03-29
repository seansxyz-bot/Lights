#include "settings.h"

#include "../tools/logger.h"
#include "imgbutton.h"

Settings::Settings(const std::string &iconPath, bool autoSensorOn,
                   bool lightsOn, bool bluetoothOn)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_autoSensorOn(autoSensorOn),
      m_lightsOn(lightsOn), m_bluetoothOn(bluetoothOn) {
  LOG_INFO() << "Settings ctor";

  auto boxA = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto boxB = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto boxC = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));

  boxA->set_spacing(10);
  boxB->set_spacing(10);
  boxC->set_spacing(10);

  boxA->set_halign(Gtk::ALIGN_CENTER);
  boxB->set_halign(Gtk::ALIGN_CENTER);
  boxC->set_halign(Gtk::ALIGN_CENTER);

  m_autoSensorBtn = Gtk::manage(
      new ImageButton(iconPath, "/auto_off", "/auto_on", m_autoSensorOn, 256));

  m_lightSwitchBtn = Gtk::manage(
      new ImageButton(iconPath, "/lights_off", "/lights_on", m_lightsOn, 256));

  m_bluetoothBtn = Gtk::manage(new ImageButton(
      iconPath, "/bluetooth_off", "/bluetooth_on", m_bluetoothOn, 256));

  m_themeControlBtn =
      Gtk::manage(new ImageButton(iconPath + "/theme_control.png", 196));

  m_editThemeBtn = Gtk::manage(new ImageButton(iconPath + "/dt.png", 196));

  m_editTeamsBtn =
      Gtk::manage(new ImageButton(iconPath + "/editteams.png", 196));

  m_restartBtn = Gtk::manage(new ImageButton(iconPath + "/restart.png", 160));
  m_okBtn = Gtk::manage(new ImageButton(iconPath + "/ok.png", 96));

  m_okBtn->set_halign(Gtk::ALIGN_CENTER);
  m_okBtn->set_valign(Gtk::ALIGN_END);
  m_okBtn->set_margin_bottom(20);

  boxA->pack_start(*m_autoSensorBtn, Gtk::PACK_SHRINK);
  boxA->pack_start(*m_lightSwitchBtn, Gtk::PACK_SHRINK);
  boxA->pack_start(*m_bluetoothBtn, Gtk::PACK_SHRINK);

  boxB->pack_start(*m_themeControlBtn, Gtk::PACK_SHRINK);
  boxB->pack_start(*m_editThemeBtn, Gtk::PACK_SHRINK);
  boxB->pack_start(*m_editTeamsBtn, Gtk::PACK_SHRINK);

  boxC->pack_start(*m_restartBtn, Gtk::PACK_SHRINK);

  set_spacing(20);
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_CENTER);

  pack_start(*boxA, Gtk::PACK_SHRINK);
  pack_start(*boxB, Gtk::PACK_SHRINK);
  pack_start(*boxC, Gtk::PACK_SHRINK);
  pack_end(*m_okBtn, Gtk::PACK_SHRINK);

  m_autoSensorBtn->signal_clicked().connect([this]() {
    m_autoSensorOn = !m_autoSensorOn;
    m_signalAutoSensorToggled.emit(m_autoSensorOn);
  });

  m_lightSwitchBtn->signal_clicked().connect([this]() {
    m_lightsOn = !m_lightsOn;
    m_signalLightsToggled.emit(m_lightsOn);
  });

  m_bluetoothBtn->signal_clicked().connect([this]() {
    m_bluetoothOn = !m_bluetoothOn;
    m_signalBluetoothToggled.emit(m_bluetoothOn);
  });

  m_themeControlBtn->signal_clicked().connect(
      [this]() { m_signalThemeControlRequested.emit(); });

  m_editThemeBtn->signal_clicked().connect(
      [this]() { m_signalEditThemeRequested.emit(); });

  m_editTeamsBtn->signal_clicked().connect(
      [this]() { m_signalEditTeamsRequested.emit(); });

  m_restartBtn->signal_clicked().connect(
      [this]() { m_signalRestartRequested.emit(); });

  m_okBtn->signal_clicked().connect([this]() { m_signalDone.emit(); });

  show_all_children();
}

void Settings::set_restart_enabled(bool enabled) {
  if (m_restartBtn)
    m_restartBtn->set_sensitive(enabled);
}

sigc::signal<void, bool> &Settings::signal_auto_sensor_toggled() {
  return m_signalAutoSensorToggled;
}

sigc::signal<void, bool> &Settings::signal_lights_toggled() {
  return m_signalLightsToggled;
}

sigc::signal<void, bool> &Settings::signal_bluetooth_toggled() {
  return m_signalBluetoothToggled;
}

sigc::signal<void> &Settings::signal_theme_control_requested() {
  return m_signalThemeControlRequested;
}

sigc::signal<void> &Settings::signal_edit_theme_requested() {
  return m_signalEditThemeRequested;
}

sigc::signal<void> &Settings::signal_edit_teams_requested() {
  return m_signalEditTeamsRequested;
}

sigc::signal<void> &Settings::signal_restart_requested() {
  return m_signalRestartRequested;
}

sigc::signal<void> &Settings::signal_done() { return m_signalDone; }
