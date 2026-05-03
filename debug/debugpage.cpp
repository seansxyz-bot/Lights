#include "debugpage.h"

DebugPage::DebugPage() : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_CENTER);
  set_spacing(18);

  m_lightShowButton.set_size_request(360, 72);
  m_sportsButton.set_size_request(360, 72);
  m_backButton.set_size_request(360, 72);

  pack_start(m_lightShowButton, Gtk::PACK_SHRINK);
  pack_start(m_sportsButton, Gtk::PACK_SHRINK);
  pack_start(m_backButton, Gtk::PACK_SHRINK);

  m_lightShowButton.signal_clicked().connect(
      [this]() { m_signalLightShowRequested.emit(); });
  m_sportsButton.signal_clicked().connect(
      [this]() { m_signalSportsRequested.emit(); });
  m_backButton.signal_clicked().connect(
      [this]() { m_signalBackRequested.emit(); });

  show_all_children();
}

sigc::signal<void> &DebugPage::signal_lightshow_requested() {
  return m_signalLightShowRequested;
}

sigc::signal<void> &DebugPage::signal_sports_requested() {
  return m_signalSportsRequested;
}

sigc::signal<void> &DebugPage::signal_back_requested() {
  return m_signalBackRequested;
}
