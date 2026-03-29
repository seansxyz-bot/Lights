#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

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
  ColorWheelPicker *m_picker = nullptr;
  ImageButton *m_okBtn = nullptr;

  sigc::signal<void, int, int, int> m_signalColorChanged;
  sigc::signal<void> m_signalDone;
};
