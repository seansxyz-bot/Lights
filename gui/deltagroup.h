#pragma once

#include <array>
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

class ColorWheelPicker;
class ImageButton;

class DeltaGroup : public Gtk::Box {
public:
  struct GroupColor {
    int r = 0;
    int g = 0;
    int b = 0;
  };

  DeltaGroup(const std::string &iconPath, int startGroup,
             const std::array<GroupColor, 3> &groupColors, int pickerSize,
             int barSize, int keypadPixelSize);
  virtual ~DeltaGroup() = default;

  sigc::signal<void, int, int, int, int> &signal_group_color_changed();
  sigc::signal<void> &signal_done();

private:
  void set_active_group(int group);

private:
  int m_groupSelection = 0;
  std::array<GroupColor, 3> m_groupColors{};

  Gtk::Box m_mainRow{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_leftPane{Gtk::ORIENTATION_VERTICAL};

  ImageButton *m_capBtn = nullptr;
  ImageButton *m_frontBtn = nullptr;
  ImageButton *m_backBtn = nullptr;
  ImageButton *m_okBtn = nullptr;

  ColorWheelPicker *m_picker = nullptr;

  sigc::signal<void, int, int, int, int> m_signalGroupColorChanged;
  sigc::signal<void> m_signalDone;
};
