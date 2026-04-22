#include "patterns.h"

#include "../utils/logger.h"
#include "imgbutton.h"

Patterns::Patterns(const std::string &iconPath, int currentPattern)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  LOG_INFO() << "Patterns ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(PATTERNS_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(PATTERNS_OUTER_SPACING);

  m_rowA.set_halign(Gtk::ALIGN_CENTER);
  m_rowB.set_halign(Gtk::ALIGN_CENTER);
  m_rowC.set_halign(Gtk::ALIGN_CENTER);

  m_rowA.set_spacing(PATTERNS_COL_SPACING);
  m_rowB.set_spacing(PATTERNS_COL_SPACING);
  m_rowC.set_spacing(PATTERNS_COL_SPACING);

  auto combination = Gtk::manage(
      new ImageButton(iconPath + "/combination.png", PATTERNS_BUTTON_SIZE));
  auto chase = Gtk::manage(
      new ImageButton(iconPath + "/chase.png", PATTERNS_BUTTON_SIZE));
  auto comet = Gtk::manage(
      new ImageButton(iconPath + "/comet.png", PATTERNS_BUTTON_SIZE));
  auto waves = Gtk::manage(
      new ImageButton(iconPath + "/waves.png", PATTERNS_BUTTON_SIZE));
  auto sloglo = Gtk::manage(
      new ImageButton(iconPath + "/sloglo.png", PATTERNS_BUTTON_SIZE));
  auto twinkle = Gtk::manage(
      new ImageButton(iconPath + "/twinkle.png", PATTERNS_BUTTON_SIZE));
  auto fade = Gtk::manage(
      new ImageButton(iconPath + "/fade.png", PATTERNS_BUTTON_SIZE));
  auto alternate = Gtk::manage(
      new ImageButton(iconPath + "/alternate.png", PATTERNS_BUTTON_SIZE));
  auto off =
      Gtk::manage(new ImageButton(iconPath + "/off.png", PATTERNS_BUTTON_SIZE));
  auto ok =
      Gtk::manage(new ImageButton(iconPath + "/ok.png", PATTERNS_OK_SIZE));

  m_rowA.pack_start(*combination, Gtk::PACK_SHRINK);
  m_rowA.pack_start(*chase, Gtk::PACK_SHRINK);
  m_rowA.pack_start(*comet, Gtk::PACK_SHRINK);
  m_rowA.pack_start(*waves, Gtk::PACK_SHRINK);

  m_rowB.pack_start(*sloglo, Gtk::PACK_SHRINK);
  m_rowB.pack_start(*twinkle, Gtk::PACK_SHRINK);
  m_rowB.pack_start(*fade, Gtk::PACK_SHRINK);
  m_rowB.pack_start(*alternate, Gtk::PACK_SHRINK);

  m_rowC.pack_start(*off, Gtk::PACK_SHRINK);

  ok->set_halign(Gtk::ALIGN_CENTER);
  ok->set_margin_bottom(PATTERNS_OK_BOTTOM_MARGIN);

  m_centBox.pack_start(m_rowA, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_rowB, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_rowC, Gtk::PACK_SHRINK);
  m_centBox.pack_start(*ok, Gtk::PACK_SHRINK);

  pack_start(m_centBox, Gtk::PACK_SHRINK);

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
