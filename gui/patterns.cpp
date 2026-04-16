#include "patterns.h"

#include "../utils/logger.h"
#include "imgbutton.h"

Patterns::Patterns(const std::string &iconPath, int currentPattern)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  LOG_INFO() << "Patterns ctor";

  auto boxA = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto boxB = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  auto boxC = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));

  boxA->set_spacing(10);
  boxB->set_spacing(10);
  boxC->set_spacing(10);

  boxA->set_halign(Gtk::ALIGN_CENTER);
  boxB->set_halign(Gtk::ALIGN_CENTER);
  boxC->set_halign(Gtk::ALIGN_CENTER);

  auto combination =
      Gtk::manage(new ImageButton(iconPath + "/combination.png", 160));
  auto chase = Gtk::manage(new ImageButton(iconPath + "/chase.png", 160));
  auto comet = Gtk::manage(new ImageButton(iconPath + "/comet.png", 160));
  auto waves = Gtk::manage(new ImageButton(iconPath + "/waves.png", 160));
  auto sloglo = Gtk::manage(new ImageButton(iconPath + "/sloglo.png", 160));
  auto twinkle = Gtk::manage(new ImageButton(iconPath + "/twinkle.png", 160));
  auto fade = Gtk::manage(new ImageButton(iconPath + "/fade.png", 160));
  auto alternate =
      Gtk::manage(new ImageButton(iconPath + "/alternate.png", 160));
  auto off = Gtk::manage(new ImageButton(iconPath + "/off.png", 160));
  auto ok = Gtk::manage(new ImageButton(iconPath + "/ok.png", 96));

  boxA->pack_start(*combination, Gtk::PACK_SHRINK);
  boxA->pack_start(*chase, Gtk::PACK_SHRINK);
  boxA->pack_start(*comet, Gtk::PACK_SHRINK);
  boxA->pack_start(*waves, Gtk::PACK_SHRINK);

  boxB->pack_start(*sloglo, Gtk::PACK_SHRINK);
  boxB->pack_start(*twinkle, Gtk::PACK_SHRINK);
  boxB->pack_start(*fade, Gtk::PACK_SHRINK);
  boxB->pack_start(*alternate, Gtk::PACK_SHRINK);

  boxC->pack_start(*off, Gtk::PACK_SHRINK);

  ok->set_halign(Gtk::ALIGN_CENTER);
  ok->set_valign(Gtk::ALIGN_END);
  ok->set_margin_bottom(20);

  set_spacing(20);
  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_CENTER);

  pack_start(*boxA, Gtk::PACK_SHRINK);
  pack_start(*boxB, Gtk::PACK_SHRINK);
  pack_start(*boxC, Gtk::PACK_SHRINK);
  pack_end(*ok, Gtk::PACK_SHRINK);

  m_buttons = {off,    combination, chase, comet,    waves,
               sloglo, twinkle,     fade,  alternate};

  for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
    m_buttons[i]->signal_clicked().connect([this, i]() {
      set_selected(i);
      m_signalPatternSelected.emit(i);
    });
  }

  ok->signal_clicked().connect([this]() { m_signalDone.emit(); });

  set_selected(currentPattern);
  show_all_children();
}

void Patterns::set_selected(int index) {
  for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
    m_buttons[i]->set_sensitive(i != index);
  }
}

sigc::signal<void, int> &Patterns::signal_pattern_selected() {
  return m_signalPatternSelected;
}

sigc::signal<void> &Patterns::signal_done() { return m_signalDone; }
