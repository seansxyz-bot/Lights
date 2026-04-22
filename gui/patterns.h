#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#if (SCREEN == 1)
#define PATTERNS_TOP_MARGIN 20
#define PATTERNS_OUTER_SPACING 20
#define PATTERNS_ROW_SPACING 10
#define PATTERNS_COL_SPACING 10
#define PATTERNS_BUTTON_SIZE 160
#define PATTERNS_OK_SIZE 96
#define PATTERNS_OK_BOTTOM_MARGIN 20
#else
#define PATTERNS_TOP_MARGIN 8
#define PATTERNS_OUTER_SPACING 10
#define PATTERNS_ROW_SPACING 8
#define PATTERNS_COL_SPACING 8
#define PATTERNS_BUTTON_SIZE 120
#define PATTERNS_OK_SIZE 72
#define PATTERNS_OK_BOTTOM_MARGIN 8
#endif

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
  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_rowA{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_rowB{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_rowC{Gtk::ORIENTATION_HORIZONTAL};

  std::vector<ImageButton *> m_buttons;

  sigc::signal<void, int> m_signalPatternSelected;
  sigc::signal<void> m_signalDone;
};
