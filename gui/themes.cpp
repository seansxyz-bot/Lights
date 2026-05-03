#include "themes.h"

#include "../utils/logger.h"
#include "imgbutton.h"

#include <algorithm>

Themes::Themes(const std::string &iconPath, const std::vector<Theme> &themes,
               int currentTheme)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath),
      m_selectedTheme(currentTheme) {
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

  m_gridShellBox.set_halign(Gtk::ALIGN_CENTER);
  m_gridShellBox.set_valign(Gtk::ALIGN_START);
  m_gridShellBox.set_spacing(THEMES_COL_SPACING);
  m_prevPageButton.set_size_request(THEMES_PAGE_BUTTON_WIDTH,
                                    THEMES_PAGE_BUTTON_HEIGHT);
  m_nextPageButton.set_size_request(THEMES_PAGE_BUTTON_WIDTH,
                                    THEMES_PAGE_BUTTON_HEIGHT);

  // Always add "none" first as theme id 0
  m_entries.push_back({0, iconPath + "/none.png"});

  for (const auto &theme : themes) {
    const std::string imagePath =
        iconPath + "/" + (theme.fileName.empty() ? "none.png" : theme.fileName);
    m_entries.push_back({theme.id, imagePath});
  }

  const auto selectedIt =
      std::find_if(m_entries.begin(), m_entries.end(),
                   [currentTheme](const ThemeEntry &entry) {
                     return entry.themeId == currentTheme;
                   });
  if (selectedIt != m_entries.end())
    m_currentPage =
        static_cast<size_t>(selectedIt - m_entries.begin()) / THEMES_PAGE_SIZE;

  m_prevPageButton.signal_clicked().connect([this]() {
    if (m_currentPage == 0)
      return;
    --m_currentPage;
    rebuild_page();
  });
  m_nextPageButton.signal_clicked().connect([this]() {
    const size_t totalPages =
        std::max<size_t>(1, (m_entries.size() + THEMES_PAGE_SIZE - 1) /
                                THEMES_PAGE_SIZE);
    if (m_currentPage + 1 >= totalPages)
      return;
    ++m_currentPage;
    rebuild_page();
  });

  auto *ok = Gtk::manage(new ImageButton(iconPath + "/ok.png", THEMES_OK_SIZE));
  ok->set_halign(Gtk::ALIGN_CENTER);
  ok->set_margin_bottom(THEMES_OK_BOTTOM_MARGIN);
  ok->signal_clicked().connect([this]() { m_signalDone.emit(); });

  m_gridShellBox.pack_start(m_prevPageButton, Gtk::PACK_SHRINK);
  m_gridShellBox.pack_start(m_rowsBox, Gtk::PACK_SHRINK);
  m_gridShellBox.pack_start(m_nextPageButton, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_gridShellBox, Gtk::PACK_SHRINK);
  m_centBox.pack_start(*ok, Gtk::PACK_SHRINK);

  pack_start(m_centBox, Gtk::PACK_SHRINK);

  rebuild_page();

  show_all_children();
}

void Themes::set_selected(int themeId) {
  m_selectedTheme = themeId;
  for (auto &entry : m_buttons) {
    entry.button->set_sensitive(entry.themeId != themeId);
  }
}

void Themes::clear_rows() {
  auto children = m_rowsBox.get_children();
  for (auto *child : children)
    m_rowsBox.remove(*child);
  m_buttons.clear();
}

void Themes::rebuild_page() {
  clear_rows();

  const size_t totalPages =
      std::max<size_t>(1, (m_entries.size() + THEMES_PAGE_SIZE - 1) /
                              THEMES_PAGE_SIZE);
  if (m_currentPage >= totalPages)
    m_currentPage = totalPages - 1;

  const size_t start = m_currentPage * THEMES_PAGE_SIZE;
  const size_t end = std::min(start + THEMES_PAGE_SIZE, m_entries.size());
  Gtk::Box *rows[THEMES_ROWS_PER_PAGE] = {};

  for (int row = 0; row < THEMES_ROWS_PER_PAGE; ++row) {
    rows[row] = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    rows[row]->set_halign(Gtk::ALIGN_CENTER);
    rows[row]->set_spacing(THEMES_COL_SPACING);
    m_rowsBox.pack_start(*rows[row], Gtk::PACK_SHRINK);
  }

  for (size_t i = start; i < end; ++i) {
    const size_t localIndex = i - start;
    const int row = static_cast<int>(localIndex / THEMES_PER_ROW);
    const auto &entry = m_entries[i];
    auto *btn = Gtk::manage(new ImageButton(entry.imagePath, THEMES_BUTTON_SIZE));

    rows[row]->pack_start(*btn, Gtk::PACK_SHRINK);
    m_buttons.push_back({entry.themeId, btn});

    btn->signal_clicked().connect([this, themeId = entry.themeId]() {
      set_selected(themeId);
      m_signalThemeSelected.emit(themeId);
    });
  }

  m_prevPageButton.set_visible(true);
  m_nextPageButton.set_visible(true);
  m_prevPageButton.set_opacity(totalPages > 1 && m_currentPage > 0 ? 1.0 : 0.0);
  m_nextPageButton.set_opacity(totalPages > 1 && m_currentPage + 1 < totalPages
                                  ? 1.0
                                  : 0.0);
  m_prevPageButton.set_sensitive(m_currentPage > 0);
  m_nextPageButton.set_sensitive(m_currentPage + 1 < totalPages);

  set_selected(m_selectedTheme);
  show_all_children();
}

sigc::signal<void, int> &Themes::signal_theme_selected() {
  return m_signalThemeSelected;
}

sigc::signal<void, int> &Themes::signal_schedule_requested() {
  return m_signalScheduleRequested;
}

sigc::signal<void> &Themes::signal_done() { return m_signalDone; }
