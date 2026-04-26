#include "bluetoothcontrols.h"

#include "../utils/logger.h"
#include "imgbutton.h"

#include <algorithm>

BluetoothControls::BluetoothControls(const std::string &iconPath,
                                     const std::vector<BTDevice> &devices)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath),
      m_devices(devices) {
  LOG_INFO() << "BluetoothControls ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(BTCTRL_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(BTCTRL_OUTER_SPACING);

  m_title.set_text("Bluetooth Controls");
  m_title.set_halign(Gtk::ALIGN_CENTER);

  m_grid.set_halign(Gtk::ALIGN_CENTER);
  m_grid.set_valign(Gtk::ALIGN_START);
  m_grid.set_row_spacing(BTCTRL_GRID_SPACING);
  m_grid.set_column_spacing(BTCTRL_GRID_SPACING);

  const size_t limit = std::min<size_t>(m_devices.size(), 12);
  for (size_t i = 0; i < limit; ++i) {
    const BTDevice device = m_devices[i];
    auto *btn = Gtk::manage(new Gtk::Button(deviceLabel(device)));
    btn->set_size_request(BTCTRL_DEVICE_WIDTH, BTCTRL_DEVICE_HEIGHT);
    btn->set_can_focus(false);
    btn->signal_clicked().connect(
        [this, device]() { m_signalDeviceSelected.emit(device); });
    m_grid.attach(*btn, static_cast<int>(i % 3), static_cast<int>(i / 3), 1, 1);
  }

  if (limit == 0) {
    auto *empty = Gtk::manage(new Gtk::Label("No saved paired devices"));
    empty->set_size_request(BTCTRL_DEVICE_WIDTH * 3, BTCTRL_DEVICE_HEIGHT);
    empty->set_halign(Gtk::ALIGN_CENTER);
    m_grid.attach(*empty, 0, 0, 3, 1);
  }

  m_cancelBtn =
      Gtk::manage(new ImageButton(m_iconPath + "/cancel.png", BTCTRL_CANCEL_SIZE));

  m_centBox.pack_start(m_title, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_grid, Gtk::PACK_SHRINK);
  m_centBox.pack_start(*m_cancelBtn, Gtk::PACK_SHRINK);
  pack_start(m_centBox, Gtk::PACK_SHRINK);

  m_cancelBtn->signal_clicked().connect([this]() { m_signalCancel.emit(); });

  show_all_children();
}

std::string BluetoothControls::deviceLabel(const BTDevice &device) const {
  std::string text = device.name.empty() ? device.macAddress : device.name;
  if (device.connected)
    text += "\nConnected";
  return text;
}

sigc::signal<void, BTDevice> &BluetoothControls::signal_device_selected() {
  return m_signalDeviceSelected;
}

sigc::signal<void> &BluetoothControls::signal_cancel() {
  return m_signalCancel;
}
