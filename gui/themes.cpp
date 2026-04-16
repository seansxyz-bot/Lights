#include "themes.h"

#include "../utils/logger.h"
#include "imgbutton.h"

Themes::Themes(const std::string &iconPath, int currentTheme)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {

  auto boxA = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto boxB = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto boxC = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto boxD = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));

  boxA->set_spacing(10);
  boxB->set_spacing(10);
  boxC->set_spacing(10);
  boxD->set_spacing(10);

  boxA->set_halign(Gtk::ALIGN_CENTER);
  boxB->set_halign(Gtk::ALIGN_CENTER);
  boxC->set_halign(Gtk::ALIGN_CENTER);
  boxD->set_halign(Gtk::ALIGN_CENTER);

  // index 0 = none, rest are theme IDs as in your original selection page
  auto none = Gtk::manage(new ImageButton(iconPath + "/none.png", 160));
  auto newyear = Gtk::manage(new ImageButton(iconPath + "/newyear.png", 160));
  auto valentine =
      Gtk::manage(new ImageButton(iconPath + "/valentine.png", 160));
  auto dad = Gtk::manage(new ImageButton(iconPath + "/dad.png", 160));
  auto spd = Gtk::manage(new ImageButton(iconPath + "/spd.png", 160));
  auto sean = Gtk::manage(new ImageButton(iconPath + "/sean.png", 160));
  auto easter = Gtk::manage(new ImageButton(iconPath + "/easter.png", 160));
  auto memorial = Gtk::manage(new ImageButton(iconPath + "/memorial.png", 160));
  auto july4 = Gtk::manage(new ImageButton(iconPath + "/july4.png", 160));
  auto anniversary =
      Gtk::manage(new ImageButton(iconPath + "/anniversary.png", 160));
  auto mom = Gtk::manage(new ImageButton(iconPath + "/mom.png", 160));
  auto labor = Gtk::manage(new ImageButton(iconPath + "/labor.png", 160));
  auto halloween =
      Gtk::manage(new ImageButton(iconPath + "/halloween.png", 160));
  auto thanksgiving =
      Gtk::manage(new ImageButton(iconPath + "/thanksgiving.png", 160));
  auto xmas = Gtk::manage(new ImageButton(iconPath + "/christmas.png", 160));
  auto cops = Gtk::manage(new ImageButton(iconPath + "/cops.png", 160));
  auto seahawks = Gtk::manage(new ImageButton(iconPath + "/seahawks.png", 160));
  auto kraken = Gtk::manage(new ImageButton(iconPath + "/kraken.png", 160));
  auto mariners = Gtk::manage(new ImageButton(iconPath + "/mariners.png", 160));
  auto ok = Gtk::manage(new ImageButton(iconPath + "/ok.png", 96));

  boxA->pack_start(*none, Gtk::PACK_SHRINK);

  boxA->pack_start(*newyear, Gtk::PACK_SHRINK);
  boxA->pack_start(*valentine, Gtk::PACK_SHRINK);
  boxA->pack_start(*dad, Gtk::PACK_SHRINK);
  boxA->pack_start(*spd, Gtk::PACK_SHRINK);

  boxB->pack_start(*sean, Gtk::PACK_SHRINK);
  boxB->pack_start(*easter, Gtk::PACK_SHRINK);
  boxB->pack_start(*memorial, Gtk::PACK_SHRINK);
  boxB->pack_start(*july4, Gtk::PACK_SHRINK);
  boxB->pack_start(*anniversary, Gtk::PACK_SHRINK);

  boxC->pack_start(*mom, Gtk::PACK_SHRINK);
  boxC->pack_start(*labor, Gtk::PACK_SHRINK);
  boxC->pack_start(*halloween, Gtk::PACK_SHRINK);
  boxC->pack_start(*thanksgiving, Gtk::PACK_SHRINK);
  boxC->pack_start(*xmas, Gtk::PACK_SHRINK);

  boxD->pack_start(*cops, Gtk::PACK_SHRINK);

  boxD->pack_start(*seahawks, Gtk::PACK_SHRINK);
  boxD->pack_start(*kraken, Gtk::PACK_SHRINK);
  boxD->pack_start(*mariners, Gtk::PACK_SHRINK);

  ok->set_halign(Gtk::ALIGN_CENTER);
  ok->set_valign(Gtk::ALIGN_END);
  ok->set_margin_bottom(20);

  set_spacing(20);
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_CENTER);

  pack_start(*boxA, Gtk::PACK_SHRINK);
  pack_start(*boxB, Gtk::PACK_SHRINK);
  pack_start(*boxC, Gtk::PACK_SHRINK);
  pack_start(*boxD, Gtk::PACK_SHRINK);
  pack_end(*ok, Gtk::PACK_SHRINK);

  m_buttons = {none, newyear,  valentine, dad,          spd,
               sean, easter,   memorial,  july4,        anniversary,
               mom,  labor,    halloween, thanksgiving, xmas,
               cops, seahawks, kraken,    mariners};

  for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
    m_buttons[i]->signal_clicked().connect([this, i]() {
      set_selected(i);
      m_signalThemeSelected.emit(i);
    });
  }

  ok->signal_clicked().connect([this]() { m_signalDone.emit(); });

  set_selected(currentTheme);

  show_all_children();
}

void Themes::set_selected(int index) {
  for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
    m_buttons[i]->set_sensitive(i != index);
  }
}

sigc::signal<void, int> &Themes::signal_theme_selected() {
  return m_signalThemeSelected;
}

sigc::signal<void, int> &Themes::signal_schedule_requested() {
  return m_signalScheduleRequested;
}

sigc::signal<void> &Themes::signal_done() { return m_signalDone; }
