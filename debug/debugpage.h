#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>

class DebugPage : public Gtk::Box {
public:
  DebugPage();
  ~DebugPage() override = default;

  sigc::signal<void> &signal_sports_requested();
  sigc::signal<void> &signal_lightshow_requested();
  sigc::signal<void> &signal_back_requested();

private:
  Gtk::Button m_lightShowButton{"LightShow"};
  Gtk::Button m_sportsButton{"Sports"};
  Gtk::Button m_backButton{"Back"};
  sigc::signal<void> m_signalLightShowRequested;
  sigc::signal<void> m_signalSportsRequested;
  sigc::signal<void> m_signalBackRequested;
};
