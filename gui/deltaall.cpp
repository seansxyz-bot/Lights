#include "deltaall.h"

#include "../utils/logger.h"
#include "colorwheelpicker.h"
#include "imgbutton.h"

DeltaAll::DeltaAll(const std::string &iconPath, int startR, int startG,
                   int startB, int pickerSize, int barSize, int keypadPixelSize)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {
  LOG_INFO() << "DeltaAll ctor";

  set_halign(Gtk::ALIGN_CENTER);
  set_valign(Gtk::ALIGN_FILL);
  set_spacing(12);

  m_picker = Gtk::manage(
      new ColorWheelPicker(iconPath, "Change All LEDs", startR, startG, startB,
                           pickerSize, barSize, keypadPixelSize));

  m_picker->set_halign(Gtk::ALIGN_CENTER);
  m_picker->set_valign(Gtk::ALIGN_FILL);

  m_picker->signal_color_changed().connect(
      [this](int r, int g, int b) { m_signalColorChanged.emit(r, g, b); });

  m_okBtn = Gtk::manage(new ImageButton(
      Gdk::Pixbuf::create_from_file(iconPath + "/ok.png"), keypadPixelSize));

  m_okBtn->set_halign(Gtk::ALIGN_CENTER);
  m_okBtn->signal_clicked().connect([this]() {
    if (m_picker)
      m_picker->commit_pending(); // make sure throttled value is applied
    m_signalDone.emit();
  });

  pack_start(*m_picker, Gtk::PACK_EXPAND_WIDGET);
  pack_start(*m_okBtn, Gtk::PACK_SHRINK);

  show_all_children();
}

sigc::signal<void, int, int, int> &DeltaAll::signal_color_changed() {
  return m_signalColorChanged;
}

sigc::signal<void> &DeltaAll::signal_done() { return m_signalDone; }
