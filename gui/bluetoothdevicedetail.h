#pragma once

#include "../bluetooth/btcontrol.h"
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

#if (SCREEN == 1)
#define BTDETAIL_TOP_MARGIN 24
#define BTDETAIL_OUTER_SPACING 24
#define BTDETAIL_BUTTON_SIZE 128
#else
#define BTDETAIL_TOP_MARGIN 8
#define BTDETAIL_OUTER_SPACING 12
#define BTDETAIL_BUTTON_SIZE 88
#endif

class ImageButton;

class BluetoothDeviceDetail : public Gtk::Box {
public:
  BluetoothDeviceDetail(const std::string &iconPath, const BTDevice &device);
  virtual ~BluetoothDeviceDetail() = default;

  void set_busy(bool busy);

  sigc::signal<void, BTDevice> &signal_connect_requested();
  sigc::signal<void, BTDevice> &signal_delete_requested();
  sigc::signal<void> &signal_cancel();

private:
  std::string m_iconPath;
  BTDevice m_device;

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Label m_title;
  Gtk::Label m_macLabel;
  Gtk::Box m_buttonRow{Gtk::ORIENTATION_HORIZONTAL};

  ImageButton *m_connectBtn = nullptr;
  ImageButton *m_deleteBtn = nullptr;
  ImageButton *m_cancelBtn = nullptr;

  sigc::signal<void, BTDevice> m_signalConnectRequested;
  sigc::signal<void, BTDevice> m_signalDeleteRequested;
  sigc::signal<void> m_signalCancel;
};
