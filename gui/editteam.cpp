#include "editteam.h"

#include "../tools/logger.h"
#include "imgbutton.h"

namespace {
Gtk::Label *make_label(const std::string &text) {
  auto *lbl = Gtk::manage(new Gtk::Label(text));
  lbl->set_halign(Gtk::ALIGN_END);
  lbl->set_valign(Gtk::ALIGN_CENTER);
  return lbl;
}
} // namespace

EditTeam::EditTeam(const std::string &iconPath, const std::string &teamsDbPath,
                   const TeamRecord &team)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath),
      m_teamsDbPath(teamsDbPath), m_team(team) {
  LOG_INFO() << "EditTeam ctor for team " << team.name;

  set_spacing(20);
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_CENTER);

  m_grid.set_row_spacing(10);
  m_grid.set_column_spacing(12);
  m_grid.set_halign(Gtk::ALIGN_CENTER);

  m_nameEntry.set_text(m_team.name);
  m_leagueEntry.set_text(m_team.league);
  m_teamCodeEntry.set_text(m_team.teamCode);
  m_nextGameUrlEntry.set_text(m_team.nextGameUrlTemplate);
  m_nextGameParserEntry.set_text(m_team.nextGameParser);
  m_liveGameUrlEntry.set_text(m_team.liveGameUrlTemplate);
  m_liveGameParserEntry.set_text(m_team.liveGameParser);
  m_apiTeamIdEntry.set_text(m_team.apiTeamId);
  m_displayOrderEntry.set_text(std::to_string(m_team.displayOrder));
  m_themeNameEntry.set_text(m_team.themeName);
  m_nextGameUtcEntry.set_text(m_team.nextGameUtc);
  m_enabledCheck.set_active(m_team.enabled != 0);

  int row = 0;
  m_grid.attach(*make_label("Name"), 0, row, 1, 1);
  m_grid.attach(m_nameEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("League"), 0, row, 1, 1);
  m_grid.attach(m_leagueEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("Team Code"), 0, row, 1, 1);
  m_grid.attach(m_teamCodeEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("Next Game URL"), 0, row, 1, 1);
  m_grid.attach(m_nextGameUrlEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("Next Game Parser"), 0, row, 1, 1);
  m_grid.attach(m_nextGameParserEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("Live Game URL"), 0, row, 1, 1);
  m_grid.attach(m_liveGameUrlEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("Live Game Parser"), 0, row, 1, 1);
  m_grid.attach(m_liveGameParserEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("API Team ID"), 0, row, 1, 1);
  m_grid.attach(m_apiTeamIdEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("Display Order"), 0, row, 1, 1);
  m_grid.attach(m_displayOrderEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("Theme Name"), 0, row, 1, 1);
  m_grid.attach(m_themeNameEntry, 1, row++, 1, 1);

  m_grid.attach(*make_label("Next Game UTC"), 0, row, 1, 1);
  m_grid.attach(m_nextGameUtcEntry, 1, row++, 1, 1);

  m_grid.attach(m_enabledCheck, 1, row++, 1, 1);

  m_okBtn = Gtk::manage(new ImageButton(m_iconPath + "/ok.png", 96));
  m_cancelBtn = Gtk::manage(new ImageButton(m_iconPath + "/cancel.png", 96));

  m_buttonBox.set_spacing(20);
  m_buttonBox.set_halign(Gtk::ALIGN_CENTER);
  m_buttonBox.pack_start(*m_okBtn, Gtk::PACK_SHRINK);
  m_buttonBox.pack_start(*m_cancelBtn, Gtk::PACK_SHRINK);

  pack_start(m_grid, Gtk::PACK_SHRINK);
  pack_start(m_buttonBox, Gtk::PACK_SHRINK);

  m_okBtn->signal_clicked().connect([this]() { on_save(); });
  m_cancelBtn->signal_clicked().connect([this]() { m_signalCancel.emit(); });

  show_all_children();
}

void EditTeam::on_save() {
  m_team.name = m_nameEntry.get_text();
  m_team.league = m_leagueEntry.get_text();
  m_team.teamCode = m_teamCodeEntry.get_text();
  m_team.nextGameUrlTemplate = m_nextGameUrlEntry.get_text();
  m_team.nextGameParser = m_nextGameParserEntry.get_text();
  m_team.liveGameUrlTemplate = m_liveGameUrlEntry.get_text();
  m_team.liveGameParser = m_liveGameParserEntry.get_text();
  m_team.apiTeamId = m_apiTeamIdEntry.get_text();
  m_team.displayOrder = std::stoi(m_displayOrderEntry.get_text().empty()
                                      ? "0"
                                      : m_displayOrderEntry.get_text());
  m_team.themeName = m_themeNameEntry.get_text();
  m_team.nextGameUtc = m_nextGameUtcEntry.get_text();
  m_team.enabled = m_enabledCheck.get_active() ? 1 : 0;

  if (writeTeam(m_teamsDbPath, m_team)) {
    m_signalSaved.emit();
  }
}

sigc::signal<void> &EditTeam::signal_saved() { return m_signalSaved; }

sigc::signal<void> &EditTeam::signal_cancel() { return m_signalCancel; }
