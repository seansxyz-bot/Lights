#include "teamlist.h"

#include "../utils/logger.h"
#include "imgbutton.h"

TeamList::TeamList(const std::string &iconPath, const std::string &teamsDbPath)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath),
      m_teamsDbPath(teamsDbPath) {
  LOG_INFO() << "TeamList ctor";

  set_spacing(20);
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_CENTER);

  m_mainBox.set_spacing(12);
  m_mainBox.set_halign(Gtk::ALIGN_CENTER);

  m_rowsBox.set_spacing(10);
  m_rowsBox.set_halign(Gtk::ALIGN_CENTER);

  m_scroll.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
  m_scroll.add(m_rowsBox);
  m_scroll.set_min_content_height(500);
  m_scroll.set_min_content_width(900);

  auto addBtn = Gtk::manage(new Gtk::Button("Add Team"));
  m_cancelBtn = Gtk::manage(new ImageButton(m_iconPath + "/cancel.png", 96));

  addBtn->set_halign(Gtk::ALIGN_CENTER);
  addBtn->set_can_focus(false);

  m_bottomBox.set_spacing(20);
  m_bottomBox.set_halign(Gtk::ALIGN_CENTER);
  m_bottomBox.pack_start(*m_cancelBtn, Gtk::PACK_SHRINK);

  m_mainBox.pack_start(*addBtn, Gtk::PACK_SHRINK);
  m_mainBox.pack_start(m_scroll, Gtk::PACK_EXPAND_WIDGET);
  m_mainBox.pack_start(m_bottomBox, Gtk::PACK_SHRINK);

  pack_start(m_mainBox, Gtk::PACK_EXPAND_WIDGET);

  addBtn->signal_clicked().connect(
      [this]() { m_signalAddTeamRequested.emit(); });

  m_cancelBtn->signal_clicked().connect([this]() { m_signalCancel.emit(); });

  reload();
  show_all_children();
}

void TeamList::reload() {
  // m_teams = readTeams(m_teamsDbPath);
  rebuild_rows();
  show_all_children();
}

void TeamList::rebuild_rows() {
  auto children = m_rowsBox.get_children();
  for (auto *child : children)
    m_rowsBox.remove(*child);

  for (const auto &team : m_teams) {
    auto row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    row->set_spacing(6);
    row->set_halign(Gtk::ALIGN_CENTER);

    auto label = Gtk::manage(new Gtk::Label(team.name));
    label->set_xalign(0.0f);

    auto editBtn =
        Gtk::manage(new ImageButton(m_iconPath + "/settings.png", 96));

    editBtn->signal_clicked().connect(
        [this, team]() { m_signalEditTeamRequested.emit(team); });

    row->pack_start(*label, Gtk::PACK_SHRINK);
    row->pack_start(*editBtn, Gtk::PACK_SHRINK);

    m_rowsBox.pack_start(*row, Gtk::PACK_SHRINK);
  }

  m_rowsBox.show_all_children();
}

sigc::signal<void, TeamRecord> &TeamList::signal_edit_team_requested() {
  return m_signalEditTeamRequested;
}

sigc::signal<void> &TeamList::signal_add_team_requested() {
  return m_signalAddTeamRequested;
}

sigc::signal<void> &TeamList::signal_cancel() { return m_signalCancel; }
