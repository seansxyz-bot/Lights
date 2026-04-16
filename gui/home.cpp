#include "home.h"

#include "../utils/logger.h"
#include "imgbutton.h"

#if (UBUNTU == 1)
#define BUTTON_WIDTH 384
#define BUTTON_MARGIN 32
#else
#define BUTTON_WIDTH 239
#define BUTTON_MARGIN 20
#endif

// auto pixbuf =
//     ButtonImageMaker::create(SETTINGS_PATH, "Dad's Birthday", BUTTON_WIDTH);

// m_deltaAllBtn =
//     Gtk::manage(new ImageButton(pixbuf, BUTTON_WIDTH, BUTTON_MARGIN, 0));
// std::string(ICON_PATH) + "/da.png", BUTTON_WIDTH, BUTTON_MARGIN));

Home::Home() : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  LOG_INFO() << "Home ctor";

  build_ui();
  connect_signals();
}

void Home::build_ui() {
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_CENTER);

  m_topRow.set_halign(Gtk::ALIGN_CENTER);
  m_bottomRow.set_halign(Gtk::ALIGN_CENTER);

  m_deltaAllBtn = Gtk::manage(new ImageButton(
      std::string(ICON_PATH) + "/da.png", BUTTON_WIDTH, BUTTON_MARGIN));

  m_deltaGroupBtn = Gtk::manage(new ImageButton(
      std::string(ICON_PATH) + "/dg.png", BUTTON_WIDTH, BUTTON_MARGIN));

  m_themesBtn = Gtk::manage(new ImageButton(
      std::string(ICON_PATH) + "/themes.png", BUTTON_WIDTH, BUTTON_MARGIN));

  m_patternsBtn = Gtk::manage(new ImageButton(
      std::string(ICON_PATH) + "/pattern.png", BUTTON_WIDTH, BUTTON_MARGIN));

  m_settingsBtn = Gtk::manage(new ImageButton(
      std::string(ICON_PATH) + "/settings.png", BUTTON_WIDTH, BUTTON_MARGIN));

  m_topRow.pack_start(*m_deltaAllBtn, Gtk::PACK_SHRINK);
  m_topRow.pack_start(*m_deltaGroupBtn, Gtk::PACK_SHRINK);

  m_bottomRow.pack_start(*m_themesBtn, Gtk::PACK_SHRINK);
  m_bottomRow.pack_start(*m_patternsBtn, Gtk::PACK_SHRINK);
  m_bottomRow.pack_start(*m_settingsBtn, Gtk::PACK_SHRINK);

  pack_start(m_topRow, Gtk::PACK_SHRINK);
  pack_start(m_bottomRow, Gtk::PACK_SHRINK);

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
