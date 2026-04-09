#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#include "../tools/readerwriter.h"
#include "../tools/teensyclient.h"
#include "colorwheelpicker.h"

class ImageButton;

class EditThemePage : public Gtk::Box {
public:
  EditThemePage(const std::string &iconPath, const Theme &theme, int pickerSize,
                int barSize, int keypadPixelSize);
  ~EditThemePage() override = default;

  const Theme &theme() const { return m_theme; }

  sigc::signal<void, Theme> &signal_save_requested();
  sigc::signal<void> &signal_cancel_requested();

private:
  struct RowWidgets {
    Gtk::RadioButton *radio = nullptr;
    Gtk::Label *rgbLabel = nullptr;
    Gtk::Button *deleteBtn;
    Gtk::Box *row = nullptr;
  };

  std::string m_iconPath;
  Theme m_theme;
  int m_selectedIndex = 0;

  ColorWheelPicker *m_picker = nullptr;

  Gtk::Box m_contentRow{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_rightCol{Gtk::ORIENTATION_VERTICAL};
  Gtk::ScrolledWindow m_scroll;
  Gtk::Box m_rowsBox{Gtk::ORIENTATION_VERTICAL};

  Gtk::Button *m_addBtn = nullptr; // changed from ImageButton
  ImageButton *m_okBtn = nullptr;
  ImageButton *m_cancelBtn = nullptr;

  Gtk::RadioButtonGroup m_radioGroup;
  std::vector<RowWidgets> m_rowWidgets;

  sigc::signal<void, Theme> m_signalSaveRequested;
  sigc::signal<void> m_signalCancelRequested;

private:
  void rebuild_rows();
  void select_color(int index);
  void update_selected_color(uint8_t r, uint8_t g, uint8_t b);
  static std::string rgb_text(const RGB_Color &c);
};
