#pragma once

#include "../tools/readerwriter.h"
#include <gtkmm.h>
#include <sigc++/sigc++.h>

class ImageButton;

class Home : public Gtk::Box {
public:
  Home();
  virtual ~Home() = default;

  sigc::signal<void> &signal_delta_all_requested();
  sigc::signal<void> &signal_delta_group_requested();
  sigc::signal<void> &signal_themes_requested();
  sigc::signal<void> &signal_patterns_requested();
  sigc::signal<void> &signal_settings_requested();

private:
  void build_ui();
  void connect_signals();

private:
  Gtk::Box m_topRow{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_bottomRow{Gtk::ORIENTATION_HORIZONTAL};

  ImageButton *m_deltaAllBtn = nullptr;
  ImageButton *m_deltaGroupBtn = nullptr;
  ImageButton *m_themesBtn = nullptr;
  ImageButton *m_patternsBtn = nullptr;
  ImageButton *m_settingsBtn = nullptr;

  sigc::signal<void> m_signalDeltaAllRequested;
  sigc::signal<void> m_signalDeltaGroupRequested;
  sigc::signal<void> m_signalThemesRequested;
  sigc::signal<void> m_signalPatternsRequested;
  sigc::signal<void> m_signalSettingsRequested;
};
