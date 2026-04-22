#include "editthemes.h"

#include "../utils//buttonimagemaker.h"
#include "../utils/logger.h"
#include "imgbutton.h"

EditThemes::EditThemes(const std::string &settingsPath,
                       const std::vector<Theme> &themes)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  LOG_INFO() << "EditThemes ctor themes=" << themes.size();

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(EDITTHEMES_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(EDITTHEMES_OUTER_MARGIN);

  auto titleFont = Pango::FontDescription();
  titleFont.set_weight(Pango::WEIGHT_BOLD);
  titleFont.set_size(EDITTHEMES_TITLE_FONT_PX * Pango::SCALE);
  m_title.override_font(titleFont);
  m_title.set_halign(Gtk::ALIGN_CENTER);

  m_scroll.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
  m_scroll.set_hexpand(true);
  m_scroll.set_vexpand(true);

  m_grid.set_row_spacing(EDITTHEMES_ROW_SPACING);
  m_grid.set_column_spacing(EDITTHEMES_COL_SPACING);
  m_grid.set_column_homogeneous(true);
  m_grid.set_halign(Gtk::ALIGN_CENTER);
  m_grid.set_valign(Gtk::ALIGN_START);
  m_grid.set_margin_top(EDITTHEMES_OUTER_MARGIN);
  m_grid.set_margin_bottom(EDITTHEMES_OUTER_MARGIN);
  m_grid.set_margin_start(EDITTHEMES_OUTER_MARGIN);
  m_grid.set_margin_end(EDITTHEMES_OUTER_MARGIN);

  for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
    const auto &theme = themes[i];
    const int themeId = theme.id;

    auto pixbuf = ButtonImageMaker::create(settingsPath, theme.name,
                                           EDITTHEMES_BUTTON_SIZE);
    if (!pixbuf) {
      LOG_INFO() << "Failed to create pixbuf for theme: " << theme.name;
      continue;
    }

    auto btn = Gtk::manage(new ImageButton(pixbuf, EDITTHEMES_BUTTON_SIZE));
    btn->set_halign(Gtk::ALIGN_CENTER);
    btn->set_valign(Gtk::ALIGN_CENTER);
    btn->set_hexpand(false);
    btn->set_vexpand(false);

    btn->signal_clicked().connect(
        [this, themeId]() { m_signalThemeEditRequested.emit(themeId); });

    const int row = i / EDITTHEMES_COLS;
    const int col = i % EDITTHEMES_COLS;
    m_grid.attach(*btn, col, row, 1, 1);
  }

  m_scroll.add(m_grid);

  m_okBtn = Gtk::manage(
      new ImageButton(settingsPath + "/icons/ok.png", EDITTHEMES_OK_SIZE));
  m_okBtn->set_halign(Gtk::ALIGN_CENTER);
  m_okBtn->set_margin_bottom(EDITTHEMES_OK_BOTTOM_MARGIN);
  m_okBtn->signal_clicked().connect([this]() { m_signalDone.emit(); });

  m_centBox.pack_start(m_title, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_scroll, Gtk::PACK_EXPAND_WIDGET);
  m_centBox.pack_start(*m_okBtn, Gtk::PACK_SHRINK);
  pack_start(m_centBox, Gtk::PACK_EXPAND_WIDGET);

  show_all_children();
}

sigc::signal<void, int> &EditThemes::signal_theme_edit_requested() {
  return m_signalThemeEditRequested;
}

sigc::signal<void> &EditThemes::signal_done() { return m_signalDone; }
