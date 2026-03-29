#include "editthemepage.h"

#include "../tools/logger.h"
#include "imgbutton.h"

EditThemePage::EditThemePage(const std::string &iconPath, const Theme &theme,
                             int pickerSize, int barSize, int keypadPixelSize)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath),
      m_theme(theme) {
  LOG_INFO() << "EditThemePage ctor for theme " << theme.name;

  set_spacing(12);

  if (m_theme.colors.empty()) {
    m_theme.colors.push_back({255, 255, 255});
  }

  const auto &c = m_theme.colors[0];
  m_picker = Gtk::manage(
      new ColorWheelPicker(iconPath, "Change " + theme.name + " Theme", c.r,
                           c.g, c.b, pickerSize, barSize, keypadPixelSize));
  m_picker->set_halign(Gtk::ALIGN_CENTER);
  m_picker->signal_color_changed().connect(
      [this](int r, int g, int b) { update_selected_color(r, g, b); });

  m_scroll.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
  m_scroll.set_hexpand(true);
  m_scroll.set_vexpand(false);
  m_scroll.set_size_request(-1, 450);
  m_rowsBox.set_spacing(12);
  m_rowsBox.set_margin_top(18);
  m_rowsBox.set_margin_bottom(8);

  m_rightCol.set_spacing(10);
  m_rightCol.set_size_request(320, -1);

  m_addBtn = Gtk::manage(new Gtk::Button("Add Color"));
  m_addBtn->set_size_request(160, 48);
  m_addBtn->signal_clicked().connect([this]() {
    m_theme.colors.push_back({255, 255, 255});
    rebuild_rows();
    select_color(static_cast<int>(m_theme.colors.size()) - 1);
  });

  m_okBtn = Gtk::manage(new ImageButton(iconPath + "/ok.png", 96));
  m_okBtn->signal_clicked().connect(
      [this]() { m_signalSaveRequested.emit(m_theme); });

  m_cancelBtn = Gtk::manage(new ImageButton(iconPath + "/cancel.png", 96));
  m_cancelBtn->signal_clicked().connect(
      [this]() { m_signalCancelRequested.emit(); });

  auto topBtnRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  topBtnRow->set_spacing(10);
  topBtnRow->set_halign(Gtk::ALIGN_CENTER);
  topBtnRow->pack_start(*m_addBtn, Gtk::PACK_SHRINK);

  auto bottomBtnRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  bottomBtnRow->set_spacing(10);
  bottomBtnRow->set_halign(Gtk::ALIGN_CENTER);
  bottomBtnRow->pack_start(*m_cancelBtn, Gtk::PACK_SHRINK);
  bottomBtnRow->pack_start(*m_okBtn, Gtk::PACK_SHRINK);

  m_scroll.add(m_rowsBox);

  m_rightCol.set_spacing(10);
  m_rightCol.set_size_request(320, -1);

  m_rightCol.pack_start(*topBtnRow, Gtk::PACK_SHRINK);
  m_rightCol.pack_start(m_scroll, Gtk::PACK_SHRINK);
  m_rightCol.pack_end(*bottomBtnRow, Gtk::PACK_SHRINK);

  m_contentRow.set_spacing(16);
  m_contentRow.set_halign(Gtk::ALIGN_CENTER);
  auto vCenterCol = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

  auto topSpacer = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
  auto bottomSpacer = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

  topSpacer->set_vexpand(true);
  bottomSpacer->set_vexpand(true);

  vCenterCol->pack_start(*topSpacer, Gtk::PACK_EXPAND_WIDGET);
  vCenterCol->pack_start(m_rightCol, Gtk::PACK_SHRINK);
  vCenterCol->pack_start(*bottomSpacer, Gtk::PACK_EXPAND_WIDGET);

  m_contentRow.pack_start(*m_picker, Gtk::PACK_SHRINK);
  m_contentRow.pack_start(*vCenterCol, Gtk::PACK_SHRINK);

  pack_start(m_contentRow, Gtk::PACK_EXPAND_WIDGET);

  rebuild_rows();
  select_color(0);

  show_all_children();
}

void EditThemePage::rebuild_rows() {
  for (auto *child : m_rowsBox.get_children()) {
    m_rowsBox.remove(*child);
  }
  m_rowWidgets.clear();

  if (m_theme.colors.empty()) {
    m_theme.colors.push_back({255, 255, 255});
  }

  if (m_selectedIndex < 0)
    m_selectedIndex = 0;
  if (m_selectedIndex >= static_cast<int>(m_theme.colors.size()))
    m_selectedIndex = static_cast<int>(m_theme.colors.size()) - 1;

  for (int i = 0; i < static_cast<int>(m_theme.colors.size()); ++i) {
    auto row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
    row->set_spacing(10);
    row->set_halign(Gtk::ALIGN_START);

    auto radio = Gtk::manage(new Gtk::RadioButton());
    if (i == 0)
      m_radioGroup = radio->get_group();
    else
      radio->set_group(m_radioGroup);

    auto lbl = Gtk::manage(new Gtk::Label(rgb_text(m_theme.colors[i])));
    lbl->set_size_request(140, -1);
    lbl->set_halign(Gtk::ALIGN_START);

    auto del = Gtk::manage(new Gtk::Button("Delete"));
    del->set_size_request(100, 40);
    del->set_sensitive(m_theme.colors.size() > 1);

    radio->signal_toggled().connect([this, i, radio]() {
      if (radio->get_active())
        select_color(i);
    });

    del->signal_clicked().connect([this, i]() {
      if (m_theme.colors.size() <= 1)
        return;

      m_theme.colors.erase(m_theme.colors.begin() + i);

      if (m_selectedIndex >= static_cast<int>(m_theme.colors.size()))
        m_selectedIndex = static_cast<int>(m_theme.colors.size()) - 1;

      rebuild_rows();
      select_color(m_selectedIndex);
    });

    row->pack_start(*radio, Gtk::PACK_SHRINK);
    row->pack_start(*lbl, Gtk::PACK_SHRINK);
    row->pack_start(*del, Gtk::PACK_SHRINK);

    m_rowsBox.pack_start(*row, Gtk::PACK_SHRINK);
    m_rowWidgets.push_back({radio, lbl, del, row});
  }

  m_rowsBox.show_all_children();
}

void EditThemePage::select_color(int index) {
  if (index < 0 || index >= static_cast<int>(m_theme.colors.size()))
    return;

  m_selectedIndex = index;

  for (int i = 0; i < static_cast<int>(m_rowWidgets.size()); ++i) {
    if (m_rowWidgets[i].radio)
      m_rowWidgets[i].radio->set_active(i == index);
  }

  const auto &c = m_theme.colors[index];
  if (m_picker)
    m_picker->set_rgb(c.r, c.g, c.b);
}

void EditThemePage::update_selected_color(uint8_t r, uint8_t g, uint8_t b) {
  if (m_selectedIndex < 0 ||
      m_selectedIndex >= static_cast<int>(m_theme.colors.size()))
    return;

  m_theme.colors[m_selectedIndex] = {r, g, b};

  if (m_selectedIndex < static_cast<int>(m_rowWidgets.size()) &&
      m_rowWidgets[m_selectedIndex].rgbLabel) {
    m_rowWidgets[m_selectedIndex].rgbLabel->set_text(
        rgb_text(m_theme.colors[m_selectedIndex]));
  }
}

std::string EditThemePage::rgb_text(const RGB_Color &c) {
  return std::to_string(c.r) + ", " + std::to_string(c.g) + ", " +
         std::to_string(c.b);
}

sigc::signal<void, Theme> &EditThemePage::signal_save_requested() {
  return m_signalSaveRequested;
}

sigc::signal<void> &EditThemePage::signal_cancel_requested() {
  return m_signalCancelRequested;
}
