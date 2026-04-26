#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#include "../storage/read.h"
#include "../storage/write.h"

#if (SCREEN == 1)
#define TEAMLIST_TOP_MARGIN 20
#define TEAMLIST_OUTER_SPACING 20
#define TEAMLIST_MAIN_SPACING 12
#define TEAMLIST_ROWS_SPACING 10
#define TEAMLIST_BOTTOM_SPACING 20
#define TEAMLIST_SCROLL_MIN_HEIGHT 500
#define TEAMLIST_SCROLL_MIN_WIDTH 900
#define TEAMLIST_CANCEL_SIZE 96
#define TEAMLIST_EDIT_SIZE 96
#define TEAMLIST_LABEL_WIDTH 520
#else
#define TEAMLIST_TOP_MARGIN 8
#define TEAMLIST_OUTER_SPACING 10
#define TEAMLIST_MAIN_SPACING 8
#define TEAMLIST_ROWS_SPACING 6
#define TEAMLIST_BOTTOM_SPACING 10
#define TEAMLIST_SCROLL_MIN_HEIGHT 360
#define TEAMLIST_SCROLL_MIN_WIDTH 700
#define TEAMLIST_CANCEL_SIZE 72
#define TEAMLIST_EDIT_SIZE 72
#define TEAMLIST_LABEL_WIDTH 420
#endif

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

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::ScrolledWindow m_scroll;
  Gtk::Box m_mainBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_rowsBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_bottomBox{Gtk::ORIENTATION_HORIZONTAL};

  ImageButton *m_cancelBtn = nullptr;

  sigc::signal<void, TeamRecord> m_signalEditTeamRequested;
  sigc::signal<void> m_signalAddTeamRequested;
  sigc::signal<void> m_signalCancel;
};
