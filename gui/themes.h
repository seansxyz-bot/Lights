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
#define THEMES_PER_ROW 5
#define THEMES_OK_BOTTOM_MARGIN 20
#else
#define THEMES_TOP_MARGIN 8
#define THEMES_OUTER_SPACING 8
#define THEMES_ROW_SPACING 6
#define THEMES_COL_SPACING 6
#define THEMES_BUTTON_SIZE 100
#define THEMES_OK_SIZE 72
#define THEMES_PER_ROW 4
#define THEMES_OK_BOTTOM_MARGIN 8
#endif

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

private:
  struct ThemeButtonEntry {
    int themeId = 0;
    ImageButton *button = nullptr;
  };

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::ScrolledWindow m_scroller;
  Gtk::Box m_rowsBox{Gtk::ORIENTATION_VERTICAL};

  std::vector<ThemeButtonEntry> m_buttons;

  sigc::signal<void, int> m_signalThemeSelected;
  sigc::signal<void, int> m_signalScheduleRequested;
  sigc::signal<void> m_signalDone;
};
