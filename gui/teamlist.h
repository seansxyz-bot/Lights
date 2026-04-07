#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#include "../tools/readerwriter.h"

class ImageButton;

class TeamList : public Gtk::Box {
public:
  TeamList(const std::string &iconPath, const std::string &teamsDbPath);
  virtual ~TeamList() = default;

  void reload();

  sigc::signal<void, TeamRecord> &signal_edit_team_requested();
  sigc::signal<void> &signal_add_team_requested();
  sigc::signal<void> &signal_cancel();

private:
  void rebuild_rows();

private:
  std::string m_iconPath;
  std::string m_teamsDbPath;
  std::vector<TeamRecord> m_teams;

  Gtk::ScrolledWindow m_scroll;
  Gtk::Box m_mainBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_rowsBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_bottomBox{Gtk::ORIENTATION_HORIZONTAL};

  ImageButton *m_cancelBtn = nullptr;

  sigc::signal<void, TeamRecord> m_signalEditTeamRequested;
  sigc::signal<void> m_signalAddTeamRequested;
  sigc::signal<void> m_signalCancel;
};
