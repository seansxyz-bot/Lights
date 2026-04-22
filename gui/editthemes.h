#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#include "../storage/read.h"
#include "../storage/write.h"

#if (SCREEN == 1)
#define EDITTHEMES_TOP_MARGIN 16
#define EDITTHEMES_TITLE_FONT_PX 22
#define EDITTHEMES_OUTER_MARGIN 10
#define EDITTHEMES_ROW_SPACING 10
#define EDITTHEMES_COL_SPACING 10
#define EDITTHEMES_COLS 5
#define EDITTHEMES_BUTTON_SIZE 128
#define EDITTHEMES_OK_SIZE 96
#define EDITTHEMES_OK_BOTTOM_MARGIN 20
#else
#define EDITTHEMES_TOP_MARGIN 8
#define EDITTHEMES_TITLE_FONT_PX 16
#define EDITTHEMES_OUTER_MARGIN 6
#define EDITTHEMES_ROW_SPACING 8
#define EDITTHEMES_COL_SPACING 8
#define EDITTHEMES_COLS 4
#define EDITTHEMES_BUTTON_SIZE 96
#define EDITTHEMES_OK_SIZE 72
#define EDITTHEMES_OK_BOTTOM_MARGIN 8
#endif

class ImageButton;

class EditThemes : public Gtk::Box {
public:
  EditThemes(const std::string &settingsPath, const std::vector<Theme> &themes);
  ~EditThemes() override = default;

  sigc::signal<void, int> &signal_theme_edit_requested();
  sigc::signal<void> &signal_done();

private:
  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Label m_title{"Edit Themes"};
  Gtk::ScrolledWindow m_scroll;
  Gtk::Grid m_grid;
  ImageButton *m_okBtn = nullptr;

  sigc::signal<void, int> m_signalThemeEditRequested;
  sigc::signal<void> m_signalDone;
};
