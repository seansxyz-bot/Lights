#include "home.h"

#include "../utils/ui_metrics.h"
#include "imgbutton.h"

Home::Home() : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  LOG_INFO() << "Home ctor";

  build_ui();
  connect_signals();
}

void Home::build_ui() {
  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_FILL);

  // m_topSpacer.set_size_request(-1, HOME_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  pack_start(m_centBox, Gtk::PACK_SHRINK);

  m_topRow.set_halign(Gtk::ALIGN_CENTER);
  m_bottomRow.set_halign(Gtk::ALIGN_CENTER);

  m_deltaAllBtn = Gtk::manage(
      new ImageButton(std::string(ICON_PATH) + "/da.png",
                      UiMetrics::button_side(), UiMetrics::button_margin()));

  m_deltaGroupBtn = Gtk::manage(
      new ImageButton(std::string(ICON_PATH) + "/dg.png",
                      UiMetrics::button_side(), UiMetrics::button_margin()));

  m_themesBtn = Gtk::manage(
      new ImageButton(std::string(ICON_PATH) + "/themes.png",
                      UiMetrics::button_side(), UiMetrics::button_margin()));

  m_patternsBtn = Gtk::manage(
      new ImageButton(std::string(ICON_PATH) + "/pattern.png",
                      UiMetrics::button_side(), UiMetrics::button_margin()));

  m_settingsBtn = Gtk::manage(
      new ImageButton(std::string(ICON_PATH) + "/settings.png",
                      UiMetrics::button_side(), UiMetrics::button_margin()));

  m_topRow.pack_start(*m_deltaAllBtn, Gtk::PACK_SHRINK);
  m_topRow.pack_start(*m_deltaGroupBtn, Gtk::PACK_SHRINK);

  m_bottomRow.pack_start(*m_themesBtn, Gtk::PACK_SHRINK);
  m_bottomRow.pack_start(*m_patternsBtn, Gtk::PACK_SHRINK);
  m_bottomRow.pack_start(*m_settingsBtn, Gtk::PACK_SHRINK);

  m_centBox.pack_start(m_topRow, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_bottomRow, Gtk::PACK_SHRINK);

  // pack_start(m_topSpacer, Gtk::PACK_SHRINK);
  pack_start(m_centBox, Gtk::PACK_EXPAND_WIDGET);

  LOG_INFO() << "Home UI built";
}

void Home::connect_signals() {
  m_deltaAllBtn->signal_clicked().connect([this]() {
    LOG_INFO() << "Home: delta all requested";
    m_signalDeltaAllRequested.emit();
  });

  m_deltaGroupBtn->signal_clicked().connect([this]() {
    LOG_INFO() << "Home: delta group requested";
    m_signalDeltaGroupRequested.emit();
  });

  m_themesBtn->signal_clicked().connect([this]() {
    LOG_INFO() << "Home: themes requested";
    m_signalThemesRequested.emit();
  });

  m_patternsBtn->signal_clicked().connect([this]() {
    LOG_INFO() << "Home: patterns requested";
    m_signalPatternsRequested.emit();
  });

  m_settingsBtn->signal_clicked().connect([this]() {
    LOG_INFO() << "Home: settings requested";
    m_signalSettingsRequested.emit();
  });

  LOG_INFO() << "Home signals connected";
}

sigc::signal<void> &Home::signal_delta_all_requested() {
  return m_signalDeltaAllRequested;
}

sigc::signal<void> &Home::signal_delta_group_requested() {
  return m_signalDeltaGroupRequested;
}

sigc::signal<void> &Home::signal_themes_requested() {
  return m_signalThemesRequested;
}

sigc::signal<void> &Home::signal_patterns_requested() {
  return m_signalPatternsRequested;
}

sigc::signal<void> &Home::signal_settings_requested() {
  return m_signalSettingsRequested;
}
