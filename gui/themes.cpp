#include "themes.h"

#include "../utils/logger.h"
#include "imgbutton.h"

Themes::Themes(const std::string &iconPath, const std::vector<Theme> &themes,
               int currentTheme)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  LOG_INFO() << "Themes ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(THEMES_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(THEMES_OUTER_SPACING);

  m_rowsBox.set_halign(Gtk::ALIGN_CENTER);
  m_rowsBox.set_valign(Gtk::ALIGN_START);
  m_rowsBox.set_spacing(THEMES_ROW_SPACING);

  Gtk::Box *currentRow = nullptr;
  int countInRow = 0;

  auto start_new_row = [&]() {
    currentRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    currentRow->set_halign(Gtk::ALIGN_CENTER);
    currentRow->set_spacing(THEMES_COL_SPACING);
    m_rowsBox.pack_start(*currentRow, Gtk::PACK_SHRINK);
    countInRow = 0;
  };

  start_new_row();

  // Always add "none" first as theme id 0
  {
    auto *btn = Gtk::manage(
        new ImageButton(iconPath + "/none.png", THEMES_BUTTON_SIZE));

    currentRow->pack_start(*btn, Gtk::PACK_SHRINK);
    m_buttons.push_back({0, btn});

    btn->signal_clicked().connect([this]() {
      set_selected(0);
      m_signalThemeSelected.emit(0);
    });

    ++countInRow;
  }

  for (const auto &theme : themes) {
    if (countInRow >= THEMES_PER_ROW) {
      start_new_row();
    }

    const std::string imagePath =
        iconPath + "/" + (theme.fileName.empty() ? "none.png" : theme.fileName);

    std::cout << imagePath << std::endl;

    auto *btn = Gtk::manage(new ImageButton(imagePath, THEMES_BUTTON_SIZE));

    currentRow->pack_start(*btn, Gtk::PACK_SHRINK);
    m_buttons.push_back({theme.id, btn});

    btn->signal_clicked().connect([this, theme]() {
      set_selected(theme.id);
      m_signalThemeSelected.emit(theme.id);
    });

    ++countInRow;
  }

  auto *ok = Gtk::manage(new ImageButton(iconPath + "/ok.png", THEMES_OK_SIZE));
  ok->set_halign(Gtk::ALIGN_CENTER);
  ok->set_margin_bottom(THEMES_OK_BOTTOM_MARGIN);
  ok->signal_clicked().connect([this]() { m_signalDone.emit(); });

  m_centBox.pack_start(m_rowsBox, Gtk::PACK_SHRINK);
  m_centBox.pack_start(*ok, Gtk::PACK_SHRINK);

  pack_start(m_centBox, Gtk::PACK_SHRINK);

  set_selected(currentTheme);

  show_all_children();
}

void Themes::set_selected(int themeId) {
  for (auto &entry : m_buttons) {
    entry.button->set_sensitive(entry.themeId != themeId);
  }
}

sigc::signal<void, int> &Themes::signal_theme_selected() {
  return m_signalThemeSelected;
}

sigc::signal<void, int> &Themes::signal_schedule_requested() {
  return m_signalScheduleRequested;
}

sigc::signal<void> &Themes::signal_done() { return m_signalDone; }
