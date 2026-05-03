#pragma once

#include "../storage/read.h"
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#if (SCREEN == 1)
#define THEMES_TOP_MARGIN 20
#define THEMES_OUTER_SPACING 16
#define THEMES_ROW_SPACING 10
#define THEMES_COL_SPACING 10
#define THEMES_BUTTON_SIZE 160
#define THEMES_OK_SIZE 96
#define THEMES_PER_ROW 6
#define THEMES_OK_BOTTOM_MARGIN 20
#define THEMES_PAGE_BUTTON_WIDTH 58
#define THEMES_PAGE_BUTTON_HEIGHT 420
#else
#define THEMES_TOP_MARGIN 8
#define THEMES_OUTER_SPACING 8
#define THEMES_ROW_SPACING 6
#define THEMES_COL_SPACING 6
#define THEMES_BUTTON_SIZE 94
#define THEMES_OK_SIZE 72
#define THEMES_PER_ROW 6
#define THEMES_OK_BOTTOM_MARGIN 8
#define THEMES_PAGE_BUTTON_WIDTH 42
#define THEMES_PAGE_BUTTON_HEIGHT 405
#endif

#define THEMES_ROWS_PER_PAGE 3
#define THEMES_PAGE_SIZE (THEMES_PER_ROW * THEMES_ROWS_PER_PAGE)

class ImageButton;

class Themes : public Gtk::Box {
public:
  Themes(const std::string &iconPath, const std::vector<Theme> &themes,
         int currentTheme);
  virtual ~Themes() = default;

  sigc::signal<void, int> &signal_theme_selected();
  sigc::signal<void, int> &signal_schedule_requested();
  sigc::signal<void> &signal_done();

private:
  void set_selected(int themeId);
  void rebuild_page();
  void clear_rows();

private:
  struct ThemeEntry {
    int themeId = 0;
    std::string imagePath;
  };

  struct ThemeButtonEntry {
    int themeId = 0;
    ImageButton *button = nullptr;
  };

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_gridShellBox{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_rowsBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Button m_prevPageButton{"B\nA\nC\nK"};
  Gtk::Button m_nextPageButton{"N\nE\nX\nT"};

  std::string m_iconPath;
  std::vector<ThemeEntry> m_entries;
  std::vector<ThemeButtonEntry> m_buttons;
  int m_selectedTheme = 0;
  size_t m_currentPage = 0;

  sigc::signal<void, int> m_signalThemeSelected;
  sigc::signal<void, int> m_signalScheduleRequested;
  sigc::signal<void> m_signalDone;
};
