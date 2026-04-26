#include "bluetoothdevicedetail.h"

#include "../utils/logger.h"
#include "imgbutton.h"

BluetoothDeviceDetail::BluetoothDeviceDetail(const std::string &iconPath,
                                             const BTDevice &device)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath),
      m_device(device) {
  LOG_INFO() << "BluetoothDeviceDetail ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(BTDETAIL_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(BTDETAIL_OUTER_SPACING);

  m_title.set_text(device.name.empty() ? device.macAddress : device.name);
  m_title.set_halign(Gtk::ALIGN_CENTER);

  m_macLabel.set_text(device.macAddress);
  m_macLabel.set_halign(Gtk::ALIGN_CENTER);

  m_buttonRow.set_halign(Gtk::ALIGN_CENTER);
  m_buttonRow.set_spacing(BTDETAIL_OUTER_SPACING);

  m_connectBtn = Gtk::manage(
      new ImageButton(m_iconPath + "/bluetooth_on.png", BTDETAIL_BUTTON_SIZE));
  m_deleteBtn =
      Gtk::manage(new ImageButton(m_iconPath + "/trash.png", BTDETAIL_BUTTON_SIZE));
  m_cancelBtn = Gtk::manage(
      new ImageButton(m_iconPath + "/cancel.png", BTDETAIL_BUTTON_SIZE));

  m_buttonRow.pack_start(*m_connectBtn, Gtk::PACK_SHRINK);
  m_buttonRow.pack_start(*m_deleteBtn, Gtk::PACK_SHRINK);
  m_buttonRow.pack_start(*m_cancelBtn, Gtk::PACK_SHRINK);

  m_centBox.pack_start(m_title, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_macLabel, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_buttonRow, Gtk::PACK_SHRINK);
  pack_start(m_centBox, Gtk::PACK_SHRINK);

  m_connectBtn->signal_clicked().connect(
      [this]() { m_signalConnectRequested.emit(m_device); });
  m_deleteBtn->signal_clicked().connect(
      [this]() { m_signalDeleteRequested.emit(m_device); });
  m_cancelBtn->signal_clicked().connect([this]() { m_signalCancel.emit(); });

  show_all_children();
}

void BluetoothDeviceDetail::set_busy(bool busy) {
  if (m_connectBtn)
    m_connectBtn->set_sensitive(!busy);
  if (m_deleteBtn)
    m_deleteBtn->set_sensitive(!busy);
  if (m_cancelBtn)
    m_cancelBtn->set_sensitive(!busy);
}

sigc::signal<void, BTDevice> &
BluetoothDeviceDetail::signal_connect_requested() {
  return m_signalConnectRequested;
}

sigc::signal<void, BTDevice> &BluetoothDeviceDetail::signal_delete_requested() {
  return m_signalDeleteRequested;
}

sigc::signal<void> &BluetoothDeviceDetail::signal_cancel() {
  return m_signalCancel;
}
