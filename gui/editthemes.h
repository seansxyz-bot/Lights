#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#include "../storage/read.h"
#include "../storage/write.h"

class ImageButton;

class EditThemes : public Gtk::Box {
public:
  EditThemes(const std::string &settingsPath, const std::vector<Theme> &themes);
  ~EditThemes() override = default;

  sigc::signal<void, int> &signal_theme_edit_requested(); // theme id
  sigc::signal<void> &signal_done();

private:
  Gtk::Label m_title{"Edit Themes"};
  Gtk::ScrolledWindow m_scroll;
  Gtk::Grid m_grid;
  ImageButton *m_okBtn = nullptr;

  sigc::signal<void, int> m_signalThemeEditRequested;
  sigc::signal<void> m_signalDone;
};
