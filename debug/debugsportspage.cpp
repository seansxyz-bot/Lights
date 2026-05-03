#include "debugsportspage.h"

namespace {
void setupButton(Gtk::Button &button) { button.set_size_request(440, 50); }
} // namespace

DebugSportsPage::DebugSportsPage() : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_CENTER);
  set_spacing(8);

  setupButton(m_updateAllButton);
  setupButton(m_updateKrakenButton);
  setupButton(m_updateSeahawksButton);
  setupButton(m_updateMarinersButton);
  setupButton(m_gameDayMlbButton);
  setupButton(m_gameDayNflButton);
  setupButton(m_gameDayNhlButton);
  setupButton(m_scoreButton);
  setupButton(m_backButton);

  pack_start(m_updateAllButton, Gtk::PACK_SHRINK);
  pack_start(m_updateKrakenButton, Gtk::PACK_SHRINK);
  pack_start(m_updateSeahawksButton, Gtk::PACK_SHRINK);
  pack_start(m_updateMarinersButton, Gtk::PACK_SHRINK);
  pack_start(m_gameDayMlbButton, Gtk::PACK_SHRINK);
  pack_start(m_gameDayNflButton, Gtk::PACK_SHRINK);
  pack_start(m_gameDayNhlButton, Gtk::PACK_SHRINK);
  pack_start(m_scoreButton, Gtk::PACK_SHRINK);
  pack_start(m_backButton, Gtk::PACK_SHRINK);

  m_updateAllButton.signal_clicked().connect(
      [this]() { m_signalUpdateAllRequested.emit(); });
  m_updateKrakenButton.signal_clicked().connect(
      [this]() { m_signalUpdateTeamRequested.emit("Kraken"); });
  m_updateSeahawksButton.signal_clicked().connect(
      [this]() { m_signalUpdateTeamRequested.emit("Seahawks"); });
  m_updateMarinersButton.signal_clicked().connect(
      [this]() { m_signalUpdateTeamRequested.emit("Mariners"); });
  m_gameDayMlbButton.signal_clicked().connect(
      [this]() { m_signalGameDayRequested.emit("Mariners"); });
  m_gameDayNflButton.signal_clicked().connect(
      [this]() { m_signalGameDayRequested.emit("Seahawks"); });
  m_gameDayNhlButton.signal_clicked().connect(
      [this]() { m_signalGameDayRequested.emit("Kraken"); });
  m_scoreButton.signal_clicked().connect(
      [this]() { m_signalScorePlaceholderRequested.emit(); });
  m_backButton.signal_clicked().connect(
      [this]() { m_signalBackRequested.emit(); });

  show_all_children();
}

sigc::signal<void> &DebugSportsPage::signal_update_all_requested() {
  return m_signalUpdateAllRequested;
}

sigc::signal<void, std::string> &DebugSportsPage::signal_update_team_requested() {
  return m_signalUpdateTeamRequested;
}

sigc::signal<void, std::string> &DebugSportsPage::signal_game_day_requested() {
  return m_signalGameDayRequested;
}

sigc::signal<void> &DebugSportsPage::signal_score_placeholder_requested() {
  return m_signalScorePlaceholderRequested;
}

sigc::signal<void> &DebugSportsPage::signal_back_requested() {
  return m_signalBackRequested;
}
