#pragma once

#include <array>
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

#if (SCREEN == 1)
#define DELTAGROUP_TOP_MARGIN 20
#define DELTAGROUP_MAIN_SPACING 20
#define DELTAGROUP_LEFT_SPACING 12
#define DELTAGROUP_ROW_SPACING 16
#define DELTAGROUP_OK_TOP_MARGIN 8
#define DELTAGROUP_OK_BOTTOM_MARGIN 12
#define DELTAGROUP_GROUP_BTN_SIZE 190
#else
#define DELTAGROUP_TOP_MARGIN 8
#define DELTAGROUP_MAIN_SPACING 10
#define DELTAGROUP_LEFT_SPACING 8
#define DELTAGROUP_ROW_SPACING 10
#define DELTAGROUP_OK_TOP_MARGIN 4
#define DELTAGROUP_OK_BOTTOM_MARGIN 6
#define DELTAGROUP_GROUP_BTN_SIZE 150
#endif

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

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
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
