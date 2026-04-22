#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#include "../drivers/i2c/teensyclient.h"
#include "../storage/read.h"
#include "../storage/write.h"
#include "colorwheelpicker.h"

#if (SCREEN == 1)
#define EDITTHEMEPAGE_TOP_MARGIN 12
#define EDITTHEMEPAGE_MAIN_SPACING 16
#define EDITTHEMEPAGE_RIGHT_SPACING 10
#define EDITTHEMEPAGE_SCROLL_HEIGHT 450
#define EDITTHEMEPAGE_RIGHT_COL_WIDTH 320
#define EDITTHEMEPAGE_ADD_BTN_WIDTH 160
#define EDITTHEMEPAGE_ADD_BTN_HEIGHT 48
#define EDITTHEMEPAGE_OK_SIZE 96
#define EDITTHEMEPAGE_CANCEL_SIZE 96
#define EDITTHEMEPAGE_ROW_SPACING 12
#define EDITTHEMEPAGE_ROW_LABEL_WIDTH 140
#define EDITTHEMEPAGE_DELETE_WIDTH 100
#define EDITTHEMEPAGE_DELETE_HEIGHT 40
#else
#define EDITTHEMEPAGE_TOP_MARGIN 8
#define EDITTHEMEPAGE_MAIN_SPACING 10
#define EDITTHEMEPAGE_RIGHT_SPACING 8
#define EDITTHEMEPAGE_SCROLL_HEIGHT 320
#define EDITTHEMEPAGE_RIGHT_COL_WIDTH 240
#define EDITTHEMEPAGE_ADD_BTN_WIDTH 120
#define EDITTHEMEPAGE_ADD_BTN_HEIGHT 40
#define EDITTHEMEPAGE_OK_SIZE 72
#define EDITTHEMEPAGE_CANCEL_SIZE 72
#define EDITTHEMEPAGE_ROW_SPACING 8
#define EDITTHEMEPAGE_ROW_LABEL_WIDTH 110
#define EDITTHEMEPAGE_DELETE_WIDTH 80
#define EDITTHEMEPAGE_DELETE_HEIGHT 34
#endif

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
    Gtk::Button *deleteBtn = nullptr;
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

  Gtk::Button *m_addBtn = nullptr;
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
