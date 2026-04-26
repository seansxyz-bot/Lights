#include "settings.h"

#include "../storage/write.h"
#include "../utils/buttonimagemaker.h"
#include "../utils/logger.h"
#include "imgbutton.h"

namespace {
std::string settingsPathFromIconPath(const std::string &iconPath) {
  const auto pos = iconPath.rfind("/icons");
  if (pos == std::string::npos)
    return iconPath;
  return iconPath.substr(0, pos);
}
} // namespace

Settings::Settings(const std::string &iconPath, Options &opt, bool bluetoothOn,
                   bool lightShowOn)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_autoSensorOn(opt.sensor),
      m_lightsOn(opt.on), m_bluetoothOn(bluetoothOn),
      m_lightShowOn(lightShowOn) {

  LOG_INFO() << "Settings ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(SETTINGS_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(SETTINGS_OUTER_SPACING);

  m_rowA.set_halign(Gtk::ALIGN_CENTER);
  m_rowB.set_halign(Gtk::ALIGN_CENTER);
  m_rowC.set_halign(Gtk::ALIGN_CENTER);

  m_rowA.set_spacing(SETTINGS_ROW_SPACING);
  m_rowB.set_spacing(SETTINGS_ROW_SPACING);
  m_rowC.set_spacing(SETTINGS_ROW_SPACING);

  m_autoSensorBtn = Gtk::manage(new ImageButton(
      iconPath, "/auto_off", "/auto_on", m_autoSensorOn, SETTINGS_TOGGLE_SIZE));

  m_lightSwitchBtn = Gtk::manage(new ImageButton(
      iconPath, "/lights_off", "/lights_on", m_lightsOn, SETTINGS_TOGGLE_SIZE));

  m_bluetoothBtn =
      Gtk::manage(new ImageButton(iconPath, "/bluetooth_off", "/bluetooth_on",
                                  m_bluetoothOn, SETTINGS_TOGGLE_SIZE));

  m_editThemeBtn =
      Gtk::manage(new ImageButton(iconPath + "/dt.png", SETTINGS_EDIT_SIZE));
  m_editPatternBtn =
      Gtk::manage(new ImageButton(iconPath + "/dp.png", SETTINGS_EDIT_SIZE));
  m_editTeamsBtn = Gtk::manage(
      new ImageButton(iconPath + "/editteams.png", SETTINGS_EDIT_SIZE));
  m_lightShowBtn = Gtk::manage(new ImageButton(
      ButtonImageMaker::create(settingsPathFromIconPath(iconPath), "LightShow",
                               SETTINGS_EDIT_SIZE),
      SETTINGS_EDIT_SIZE));
  m_bluetoothControlsBtn = Gtk::manage(
      new ImageButton(iconPath + "/blue_ctrl_mbc.png", SETTINGS_EDIT_SIZE));
  m_restartBtn = Gtk::manage(
      new ImageButton(iconPath + "/restart.png", SETTINGS_RESTART_SIZE));
  m_okBtn =
      Gtk::manage(new ImageButton(iconPath + "/ok.png", SETTINGS_OK_SIZE));

  m_okBtn->set_halign(Gtk::ALIGN_CENTER);
  m_okBtn->set_margin_bottom(SETTINGS_OK_BOTTOM_MARGIN);

  m_rowA.pack_start(*m_autoSensorBtn, Gtk::PACK_SHRINK);
  m_rowA.pack_start(*m_lightSwitchBtn, Gtk::PACK_SHRINK);
  m_rowA.pack_start(*m_bluetoothBtn, Gtk::PACK_SHRINK);

  m_rowB.pack_start(*m_editThemeBtn, Gtk::PACK_SHRINK);
  m_rowB.pack_start(*m_editPatternBtn, Gtk::PACK_SHRINK);
  m_rowB.pack_start(*m_editTeamsBtn, Gtk::PACK_SHRINK);

  m_lightShowBtn->set_sensitive(m_lightShowOn);
  m_rowC.pack_start(*m_lightShowBtn, Gtk::PACK_SHRINK);
  m_rowC.pack_start(*m_bluetoothControlsBtn, Gtk::PACK_SHRINK);
  m_rowC.pack_start(*m_restartBtn, Gtk::PACK_SHRINK);

  m_centBox.pack_start(m_rowA, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_rowB, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_rowC, Gtk::PACK_SHRINK);
  m_centBox.pack_start(*m_okBtn, Gtk::PACK_SHRINK);

  pack_start(m_centBox, Gtk::PACK_SHRINK);

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

  m_editThemeBtn->signal_clicked().connect(
      [this]() { m_signalEditThemeRequested.emit(); });

  m_editPatternBtn->signal_clicked().connect(
      [this]() { m_signalEditPatternRequested.emit(); });

  m_editTeamsBtn->signal_clicked().connect(
      [this]() { m_signalEditTeamsRequested.emit(); });

  m_lightShowBtn->signal_clicked().connect(
      [this]() { m_signalLightShowRequested.emit(); });

  m_bluetoothControlsBtn->signal_clicked().connect(
      [this]() { m_signalBluetoothControlsRequested.emit(); });

  m_restartBtn->signal_clicked().connect(
      [this]() { m_signalRestartRequested.emit(); });

  m_okBtn->signal_clicked().connect([this]() { m_signalDone.emit(); });

  show_all_children();
}

void Settings::set_restart_enabled(bool enabled) {
  if (m_restartBtn)
    m_restartBtn->set_sensitive(enabled);
}

void Settings::set_bluetooth_enabled(bool enabled) {
  if (m_bluetoothBtn)
    m_bluetoothBtn->set_sensitive(enabled);
}

void Settings::set_lightshow_enabled(bool enabled) {
  m_lightShowOn = enabled;
  if (m_lightShowBtn)
    m_lightShowBtn->set_sensitive(enabled);
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

sigc::signal<void> &Settings::signal_edit_theme_requested() {
  return m_signalEditThemeRequested;
}

sigc::signal<void> &Settings::signal_edit_pattern_requested() {
  return m_signalEditPatternRequested;
}

sigc::signal<void> &Settings::signal_edit_teams_requested() {
  return m_signalEditTeamsRequested;
}

sigc::signal<void> &Settings::signal_lightshow_requested() {
  return m_signalLightShowRequested;
}

sigc::signal<void> &Settings::signal_bluetooth_controls_requested() {
  return m_signalBluetoothControlsRequested;
}

sigc::signal<void> &Settings::signal_restart_requested() {
  return m_signalRestartRequested;
}

sigc::signal<void> &Settings::signal_done() { return m_signalDone; }
