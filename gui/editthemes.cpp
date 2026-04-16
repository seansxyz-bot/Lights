#include "editthemes.h"

#include "../utils//buttonimagemaker.h"
#include "../utils/logger.h"
#include "imgbutton.h"

EditThemes::EditThemes(const std::string &settingsPath,
                       const std::vector<Theme> &themes)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  LOG_INFO() << "EditThemes ctor themes=" << themes.size();

  set_spacing(16);
  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_FILL);
  set_hexpand(true);
  set_vexpand(true);

  // ----- Title -----
  auto titleFont = Pango::FontDescription();
  titleFont.set_weight(Pango::WEIGHT_BOLD);
  titleFont.set_size(22 * Pango::SCALE);

  m_title.override_font(titleFont);
  m_title.set_halign(Gtk::ALIGN_CENTER);
  m_title.set_margin_top(12);
  m_title.set_margin_bottom(12);

  // ----- Scroll area -----
  m_scroll.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
  m_scroll.set_hexpand(true);
  m_scroll.set_vexpand(true);

  // ----- Grid -----
  m_grid.set_row_spacing(10);
  m_grid.set_column_spacing(10);
  m_grid.set_column_homogeneous(true);
  m_grid.set_halign(Gtk::ALIGN_CENTER);
  m_grid.set_valign(Gtk::ALIGN_START);
  m_grid.set_margin_top(10);
  m_grid.set_margin_bottom(10);
  m_grid.set_margin_start(10);
  m_grid.set_margin_end(10);

  const int cols = 5;
  const int buttonSize = 128;

  for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
    const auto &theme = themes[i];
    const int themeId = theme.id;

    auto pixbuf =
        ButtonImageMaker::create(settingsPath, theme.name, buttonSize);
    if (!pixbuf) {
      LOG_INFO() << "Failed to create pixbuf for theme: " << theme.name;
      continue;
    }

    auto btn = Gtk::manage(new ImageButton(pixbuf, buttonSize));
    btn->set_halign(Gtk::ALIGN_CENTER);
    btn->set_valign(Gtk::ALIGN_CENTER);
    btn->set_hexpand(false);
    btn->set_vexpand(false);

    btn->signal_clicked().connect(
        [this, themeId]() { m_signalThemeEditRequested.emit(themeId); });

    const int row = i / cols;
    const int col = i % cols;
    m_grid.attach(*btn, col, row, 1, 1);
  }

  m_scroll.add(m_grid);

  // ----- OK button -----
  m_okBtn = Gtk::manage(new ImageButton(settingsPath + "/icons/ok.png", 96));
  m_okBtn->set_halign(Gtk::ALIGN_CENTER);
  m_okBtn->set_margin_top(8);
  m_okBtn->set_margin_bottom(20);
  m_okBtn->signal_clicked().connect([this]() { m_signalDone.emit(); });

  // ----- Layout -----
  pack_start(m_title, Gtk::PACK_SHRINK);
  pack_start(m_scroll, Gtk::PACK_EXPAND_WIDGET);
  pack_end(*m_okBtn, Gtk::PACK_SHRINK);

  show_all_children();
}

sigc::signal<void, int> &EditThemes::signal_theme_edit_requested() {
  return m_signalThemeEditRequested;
}

sigc::signal<void> &EditThemes::signal_done() { return m_signalDone; }
