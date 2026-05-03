#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

class DebugSportsPage : public Gtk::Box {
public:
  DebugSportsPage();
  ~DebugSportsPage() override = default;

  sigc::signal<void> &signal_update_all_requested();
  sigc::signal<void, std::string> &signal_update_team_requested();
  sigc::signal<void, std::string> &signal_game_day_requested();
  sigc::signal<void> &signal_score_placeholder_requested();
  sigc::signal<void> &signal_back_requested();

private:
  Gtk::Button m_updateAllButton{"Update Schedules (All Teams)"};
  Gtk::Button m_updateKrakenButton{"Update Schedule - Kraken"};
  Gtk::Button m_updateSeahawksButton{"Update Schedule - Seahawks"};
  Gtk::Button m_updateMarinersButton{"Update Schedule - Mariners"};
  Gtk::Button m_gameDayMlbButton{"Run MLB Game Day Animation"};
  Gtk::Button m_gameDayNflButton{"Run NFL Game Day Animation"};
  Gtk::Button m_gameDayNhlButton{"Run NHL Game Day Animation"};
  Gtk::Button m_scoreButton{"Run Score Animation (Placeholder)"};
  Gtk::Button m_backButton{"Back"};

  sigc::signal<void> m_signalUpdateAllRequested;
  sigc::signal<void, std::string> m_signalUpdateTeamRequested;
  sigc::signal<void, std::string> m_signalGameDayRequested;
  sigc::signal<void> m_signalScorePlaceholderRequested;
  sigc::signal<void> m_signalBackRequested;
};
