#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

#if (SCREEN == 1)
#define DELTAALL_TOP_MARGIN 24
#define DELTAALL_WIDGET_SPACING 20
#define DELTAALL_BOTTOM_MARGIN 12
#else
#define DELTAALL_TOP_MARGIN 8
#define DELTAALL_WIDGET_SPACING 10
#define DELTAALL_BOTTOM_MARGIN 8
#endif

class ColorWheelPicker;
class ImageButton;

class DeltaAll : public Gtk::Box {
public:
  DeltaAll(const std::string &iconPath, int startR, int startG, int startB,
           int pickerSize, int barSize, int keypadPixelSize);
  virtual ~DeltaAll() = default;

  sigc::signal<void, int, int, int> &signal_color_changed();
  sigc::signal<void> &signal_done();

private:
  void set_done_button_cancel(bool cancelMode);

  std::string m_iconPath;
  bool m_keypadVisible{false};

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  ColorWheelPicker *m_picker = nullptr;
  ImageButton *m_okBtn = nullptr;

  sigc::signal<void, int, int, int> m_signalColorChanged;
  sigc::signal<void> m_signalDone;
};
