// gui/editpattern.h
#pragma once

#include "../models/types.h"
#include "keypad.h"

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#if (SCREEN == 1)
#define EDIT_PATTERN_TOP_MARGIN 20
#define EDIT_PATTERN_OUTER_SPACING 18
#define EDIT_PATTERN_COL_SPACING 36
#define EDIT_PATTERN_ROW_SPACING 14
#define EDIT_PATTERN_LABEL_WIDTH 130
#define EDIT_PATTERN_ENTRY_WIDTH 90
#define EDIT_PATTERN_ENTRY_HEIGHT 54
#define EDIT_PATTERN_FONT_SIZE 28
#define EDIT_PATTERN_OK_SIZE 96
#define EDIT_PATTERN_CANCEL_SIZE 96
#define EDIT_PATTERN_KEYPAD_SIZE 96
#else
#define EDIT_PATTERN_TOP_MARGIN 4
#define EDIT_PATTERN_OUTER_SPACING 8
#define EDIT_PATTERN_COL_SPACING 20
#define EDIT_PATTERN_ROW_SPACING 8
#define EDIT_PATTERN_LABEL_WIDTH 84
#define EDIT_PATTERN_ENTRY_WIDTH 64
#define EDIT_PATTERN_ENTRY_HEIGHT 40
#define EDIT_PATTERN_FONT_SIZE 18
#define EDIT_PATTERN_OK_SIZE 60
#define EDIT_PATTERN_CANCEL_SIZE 60
#define EDIT_PATTERN_KEYPAD_SIZE 72
#endif

class ImageButton;

class EditPattern : public Gtk::Box {
public:
  EditPattern(const std::string &iconPath,
              const std::vector<Pattern> &patterns);

  sigc::signal<void, int, int> &signal_pattern_speed_preview();
  sigc::signal<void, std::vector<Pattern>> &signal_save();
  sigc::signal<void> &signal_cancel();

private:
  struct EntryRef {
    int id;
    Gtk::Entry *entry;
  };

  Gtk::Box *makeCell(const std::string &label, int patternId);
  void beginEdit(int patternId);
  void setSpeed(int patternId, int speed);
  int speedFor(int patternId) const;
  Gtk::Entry *entryFor(int patternId) const;
  static int clampSpeed(int speed);

  std::string m_iconPath;
  std::vector<Pattern> m_patterns;
  std::vector<EntryRef> m_entries;

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_gridBox{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_colA{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_colB{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_colC{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_buttonRow{Gtk::ORIENTATION_HORIZONTAL};

  KeyPad *m_keypad = nullptr;
  int m_editingPatternId = -1;

  sigc::signal<void, int, int> m_signalPatternSpeedPreview;
  sigc::signal<void, std::vector<Pattern>> m_signalSave;
  sigc::signal<void> m_signalCancel;
};
