// gui/editpattern.cpp
#include "editpattern.h"

#include "../utils/logger.h"
#include "imgbutton.h"

#include <algorithm>

namespace {
struct PatternLabel {
  const char *label;
  int id;
};

constexpr PatternLabel COL_A[] = {
    {"chase", 2},
    {"comet", 3},
    {"waves", 4},
};

constexpr PatternLabel COL_B[] = {
    {"sloglo", 5},
    {"twinkle", 6},
};

constexpr PatternLabel COL_C[] = {
    {"fade", 7},
    {"alternate", 8},
};
} // namespace

EditPattern::EditPattern(const std::string &iconPath,
                         const std::vector<Pattern> &patterns)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath),
      m_patterns(patterns) {
  LOG_INFO() << "EditPattern ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(EDIT_PATTERN_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(EDIT_PATTERN_OUTER_SPACING);

  m_gridBox.set_halign(Gtk::ALIGN_CENTER);
  m_gridBox.set_spacing(EDIT_PATTERN_COL_SPACING);

  m_colA.set_spacing(EDIT_PATTERN_ROW_SPACING);
  m_colB.set_spacing(EDIT_PATTERN_ROW_SPACING);
  m_colC.set_spacing(EDIT_PATTERN_ROW_SPACING);

  for (const auto &p : COL_A)
    m_colA.pack_start(*makeCell(p.label, p.id), Gtk::PACK_SHRINK);

  for (const auto &p : COL_B)
    m_colB.pack_start(*makeCell(p.label, p.id), Gtk::PACK_SHRINK);

  for (const auto &p : COL_C)
    m_colC.pack_start(*makeCell(p.label, p.id), Gtk::PACK_SHRINK);

  m_gridBox.pack_start(m_colA, Gtk::PACK_SHRINK);
  m_gridBox.pack_start(m_colB, Gtk::PACK_SHRINK);
  m_gridBox.pack_start(m_colC, Gtk::PACK_SHRINK);

  auto *cancel = Gtk::manage(
      new ImageButton(m_iconPath + "/cancel.png", EDIT_PATTERN_CANCEL_SIZE));
  auto *ok = Gtk::manage(
      new ImageButton(m_iconPath + "/ok.png", EDIT_PATTERN_OK_SIZE));

  m_buttonRow.set_halign(Gtk::ALIGN_CENTER);
  m_buttonRow.set_spacing(EDIT_PATTERN_COL_SPACING);
  m_buttonRow.pack_start(*cancel, Gtk::PACK_SHRINK);
  m_buttonRow.pack_start(*ok, Gtk::PACK_SHRINK);

  cancel->signal_clicked().connect([this]() { m_signalCancel.emit(); });

  ok->signal_clicked().connect([this]() { m_signalSave.emit(m_patterns); });

  m_centBox.pack_start(m_gridBox, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_buttonRow, Gtk::PACK_SHRINK);

  pack_start(m_centBox, Gtk::PACK_SHRINK);
  show_all_children();
}

Gtk::Box *EditPattern::makeCell(const std::string &label, int patternId) {
  auto *cell = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  cell->set_spacing(EDIT_PATTERN_ROW_SPACING);
  cell->set_halign(Gtk::ALIGN_START);

  auto *name = Gtk::manage(new Gtk::Label(label));
  name->set_halign(Gtk::ALIGN_START);
  name->set_size_request(EDIT_PATTERN_LABEL_WIDTH, -1);

  Pango::FontDescription fd;
  fd.set_family("Sans");
  fd.set_weight(Pango::WEIGHT_BOLD);
  fd.set_size(EDIT_PATTERN_FONT_SIZE * PANGO_SCALE);
  name->override_font(fd);

  auto *entry = Gtk::manage(new Gtk::Entry());
  entry->set_text(std::to_string(speedFor(patternId)));
  entry->set_width_chars(3);
  entry->set_max_length(3);
  entry->set_size_request(EDIT_PATTERN_ENTRY_WIDTH, EDIT_PATTERN_ENTRY_HEIGHT);
  entry->set_alignment(0.5f);
  entry->set_editable(false);
  entry->set_can_focus(false);
  entry->override_font(fd);

  entry->signal_button_press_event().connect(
      [this, patternId](GdkEventButton *) {
        beginEdit(patternId);
        return true;
      },
      false);

  cell->pack_start(*name, Gtk::PACK_SHRINK);
  cell->pack_start(*entry, Gtk::PACK_SHRINK);

  m_entries.push_back({patternId, entry});
  return cell;
}

void EditPattern::beginEdit(int patternId) {
  m_editingPatternId = patternId;

  // Remove normal edit-pattern UI
  remove(m_centBox);

  // Clean up any existing keypad
  if (m_keypad) {
    m_keypadBox.remove(*m_keypad);
    m_keypad = nullptr;
  }

  m_keypadBox.set_halign(Gtk::ALIGN_CENTER);
  m_keypadBox.set_valign(Gtk::ALIGN_START);
  m_keypadBox.set_margin_top(EDIT_PATTERN_TOP_MARGIN);

  m_keypad = Gtk::manage(new KeyPad(m_iconPath, speedFor(patternId),
                                    EDIT_PATTERN_KEYPAD_SIZE, 100));

  m_keypad->signal_ok_pressed().connect([this](int speed) {
    speed = clampSpeed(speed);

    setSpeed(m_editingPatternId, speed);
    m_signalPatternSpeedPreview.emit(m_editingPatternId, speed);

    if (m_keypad) {
      m_keypadBox.remove(*m_keypad);
      m_keypad = nullptr;
    }

    remove(m_keypadBox);
    pack_start(m_centBox, Gtk::PACK_SHRINK);

    m_editingPatternId = -1;
    show_all_children();
  });

  pack_start(m_keypadBox, Gtk::PACK_SHRINK);
  m_keypadBox.pack_start(*m_keypad, Gtk::PACK_SHRINK);

  show_all_children();
}

void EditPattern::setSpeed(int patternId, int speed) {
  speed = clampSpeed(speed);

  bool found = false;
  for (auto &p : m_patterns) {
    if (p.id == patternId) {
      p.speed = speed;
      found = true;
      break;
    }
  }

  if (!found)
    m_patterns.push_back({patternId, speed});

  if (auto *entry = entryFor(patternId))
    entry->set_text(std::to_string(speed));
}

int EditPattern::speedFor(int patternId) const {
  for (const auto &p : m_patterns) {
    if (p.id == patternId)
      return clampSpeed(p.speed);
  }

  return 50;
}

Gtk::Entry *EditPattern::entryFor(int patternId) const {
  for (const auto &ref : m_entries) {
    if (ref.id == patternId)
      return ref.entry;
  }

  return nullptr;
}

int EditPattern::clampSpeed(int speed) {
  return std::max(0, std::min(100, speed));
}

sigc::signal<void, int, int> &EditPattern::signal_pattern_speed_preview() {
  return m_signalPatternSpeedPreview;
}

sigc::signal<void, std::vector<Pattern>> &EditPattern::signal_save() {
  return m_signalSave;
}

sigc::signal<void> &EditPattern::signal_cancel() { return m_signalCancel; }
