#include "deltagroup.h"

#include "../utils/logger.h"
#include "colorwheelpicker.h"
#include "imgbutton.h"

DeltaGroup::DeltaGroup(const std::string &iconPath, int startGroup,
                       const std::array<GroupColor, 3> &groupColors,
                       int pickerSize, int barSize, int keypadPixelSize)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_groupSelection(startGroup),
      m_groupColors(groupColors) {
  LOG_INFO() << "DeltaGroup ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(DELTAGROUP_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(DELTAGROUP_ROW_SPACING);

  m_leftPane.set_spacing(DELTAGROUP_LEFT_SPACING);

  m_capBtn = Gtk::manage(
      new ImageButton(iconPath + "/cap.png", DELTAGROUP_GROUP_BTN_SIZE));
  m_frontBtn = Gtk::manage(
      new ImageButton(iconPath + "/front.png", DELTAGROUP_GROUP_BTN_SIZE));
  m_backBtn = Gtk::manage(
      new ImageButton(iconPath + "/back.png", DELTAGROUP_GROUP_BTN_SIZE));

  const auto &c = m_groupColors[m_groupSelection];
  m_picker = Gtk::manage(new ColorWheelPicker(iconPath, "Change Cap LEDs", c.r,
                                              c.g, c.b, pickerSize, barSize,
                                              keypadPixelSize));

  m_picker->set_halign(Gtk::ALIGN_CENTER);
  m_picker->set_valign(Gtk::ALIGN_START);

  m_capBtn->signal_clicked().connect([this]() { set_active_group(0); });
  m_frontBtn->signal_clicked().connect([this]() { set_active_group(1); });
  m_backBtn->signal_clicked().connect([this]() { set_active_group(2); });

  m_picker->signal_color_changed().connect([this](int r, int g, int b) {
    m_groupColors[m_groupSelection] = {r, g, b};
    m_signalGroupColorChanged.emit(m_groupSelection, r, g, b);
  });

  m_okBtn = Gtk::manage(new ImageButton(iconPath + "/ok.png", keypadPixelSize));
  m_okBtn->set_halign(Gtk::ALIGN_CENTER);
  m_okBtn->set_margin_top(DELTAGROUP_OK_TOP_MARGIN);
  m_okBtn->set_margin_bottom(DELTAGROUP_OK_BOTTOM_MARGIN);
  m_okBtn->signal_clicked().connect([this]() {
    if (m_picker)
      m_picker->commit_pending();
    m_signalDone.emit();
  });

  m_leftPane.pack_start(*m_capBtn, Gtk::PACK_SHRINK);
  m_leftPane.pack_start(*m_frontBtn, Gtk::PACK_SHRINK);
  m_leftPane.pack_start(*m_backBtn, Gtk::PACK_SHRINK);

  m_mainRow.set_halign(Gtk::ALIGN_CENTER);
  m_mainRow.set_valign(Gtk::ALIGN_START);
  m_mainRow.set_spacing(DELTAGROUP_MAIN_SPACING);
  m_mainRow.pack_start(m_leftPane, Gtk::PACK_SHRINK);
  m_mainRow.pack_start(*m_picker, Gtk::PACK_SHRINK);

  m_centBox.pack_start(m_mainRow, Gtk::PACK_SHRINK);
  m_centBox.pack_start(*m_okBtn, Gtk::PACK_SHRINK);

  pack_start(m_centBox, Gtk::PACK_SHRINK);

  set_active_group(m_groupSelection);
  show_all_children();
}

void DeltaGroup::set_active_group(int group) {
  if (group < 0 || group > 2)
    return;

  // Make sure any throttled picker value is applied before switching groups
  if (m_picker)
    m_picker->commit_pending();

  m_groupSelection = group;

  switch (m_groupSelection) {
  case 0:
    m_picker->set_header_text("Change Cap LEDs");
    break;
  case 1:
    m_picker->set_header_text("Change Front Steps LEDs");
    break;
  case 2:
    m_picker->set_header_text("Change Back Steps LEDs");
    break;
  }

  const auto &c = m_groupColors[m_groupSelection];
  m_picker->set_rgb(c.r, c.g, c.b);

  m_capBtn->set_sensitive(m_groupSelection != 0);
  m_frontBtn->set_sensitive(m_groupSelection != 1);
  m_backBtn->set_sensitive(m_groupSelection != 2);

  LOG_INFO() << "DeltaGroup active group set to " << m_groupSelection;
}

sigc::signal<void, int, int, int, int> &
DeltaGroup::signal_group_color_changed() {
  return m_signalGroupColorChanged;
}

sigc::signal<void> &DeltaGroup::signal_done() { return m_signalDone; }
