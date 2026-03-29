#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

#include "../tools/settingsrw.h"

class ImageButton;

class EditTeam : public Gtk::Box {
public:
  EditTeam(const std::string &iconPath, const std::string &teamsDbPath,
           const TeamRecord &team);
  virtual ~EditTeam() = default;

  sigc::signal<void> &signal_saved();
  sigc::signal<void> &signal_cancel();

private:
  void on_save();

private:
  std::string m_iconPath;
  std::string m_teamsDbPath;
  TeamRecord m_team;

  Gtk::Grid m_grid;
  Gtk::Box m_buttonBox{Gtk::ORIENTATION_HORIZONTAL};

  Gtk::Entry m_nameEntry;
  Gtk::Entry m_leagueEntry;
  Gtk::Entry m_teamCodeEntry;
  Gtk::Entry m_nextGameUrlEntry;
  Gtk::Entry m_nextGameParserEntry;
  Gtk::Entry m_liveGameUrlEntry;
  Gtk::Entry m_liveGameParserEntry;
  Gtk::Entry m_apiTeamIdEntry;
  Gtk::Entry m_displayOrderEntry;
  Gtk::Entry m_themeNameEntry;
  Gtk::Entry m_nextGameUtcEntry;

  Gtk::CheckButton m_enabledCheck{"Enabled"};

  ImageButton *m_okBtn = nullptr;
  ImageButton *m_cancelBtn = nullptr;

  sigc::signal<void> m_signalSaved;
  sigc::signal<void> m_signalCancel;
};
