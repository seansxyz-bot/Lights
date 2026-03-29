#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

class ImageButton;

class Patterns : public Gtk::Box {
public:
  Patterns(const std::string &iconPath, int currentPattern);
  virtual ~Patterns() = default;

  sigc::signal<void, int> &signal_pattern_selected();
  sigc::signal<void> &signal_done();

private:
  void set_selected(int index);

private:
  std::vector<ImageButton *> m_buttons;

  sigc::signal<void, int> m_signalPatternSelected;
  sigc::signal<void> m_signalDone;
};
