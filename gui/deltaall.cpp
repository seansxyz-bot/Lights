#include "deltaall.h"

#include "../utils/logger.h"
#include "colorwheelpicker.h"
#include "imgbutton.h"

DeltaAll::DeltaAll(const std::string &iconPath, int startR, int startG,
                   int startB, int pickerSize, int barSize, int keypadPixelSize)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath) {
  LOG_INFO() << "DeltaAll ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(DELTAALL_TOP_MARGIN);
  set_margin_bottom(DELTAALL_BOTTOM_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(DELTAALL_WIDGET_SPACING);

  m_picker = Gtk::manage(
      new ColorWheelPicker(iconPath, "Change All LEDs", startR, startG, startB,
                           pickerSize, barSize, keypadPixelSize));

  m_picker->set_halign(Gtk::ALIGN_CENTER);
  m_picker->set_valign(Gtk::ALIGN_START);

  m_picker->signal_color_changed().connect(
      [this](int r, int g, int b) { m_signalColorChanged.emit(r, g, b); });

  m_picker->signal_keypad_visibility_changed().connect([this](bool shown) {
    m_keypadVisible = shown;
    set_done_button_cancel(shown);
  });

  m_okBtn = Gtk::manage(new ImageButton(
      Gdk::Pixbuf::create_from_file(iconPath + "/ok.png"), keypadPixelSize));

  m_okBtn->set_halign(Gtk::ALIGN_CENTER);
  m_okBtn->signal_clicked().connect([this]() {
    if (m_keypadVisible) {
      if (m_picker)
        m_picker->dismiss_keypad();
      return;
    }

    if (m_picker)
      m_picker->commit_pending();

    set_done_button_cancel(false);
    m_signalDone.emit();
  });

  m_centBox.pack_start(*m_picker, Gtk::PACK_SHRINK);
  m_centBox.pack_start(*m_okBtn, Gtk::PACK_SHRINK);

  pack_start(m_centBox, Gtk::PACK_SHRINK);

  show_all_children();
}

void DeltaAll::set_done_button_cancel(bool cancelMode) {
  if (!m_okBtn)
    return;

  m_okBtn->set_image_path(m_iconPath +
                          std::string(cancelMode ? "/cancel.png" : "/ok.png"));
}

sigc::signal<void, int, int, int> &DeltaAll::signal_color_changed() {
  return m_signalColorChanged;
}

sigc::signal<void> &DeltaAll::signal_done() { return m_signalDone; }
