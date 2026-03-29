#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

class ImageButton;

class Themes : public Gtk::Box {
public:
  Themes(const std::string &iconPath, int currentTheme,
         bool schedulerMode = false);
  virtual ~Themes() = default;

  sigc::signal<void, int> &signal_theme_selected();
  sigc::signal<void, int> &signal_schedule_requested();
  sigc::signal<void> &signal_done();

private:
  void set_selected(int index);

private:
  bool m_schedulerMode = false;
  std::vector<ImageButton *> m_buttons;

  sigc::signal<void, int> m_signalThemeSelected;
  sigc::signal<void, int> m_signalScheduleRequested;
  sigc::signal<void> m_signalDone;
};
