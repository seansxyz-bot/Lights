#pragma once

#include "../bluetooth/btcontrol.h"
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

#if (SCREEN == 1)
#define BTCTRL_TOP_MARGIN 20
#define BTCTRL_OUTER_SPACING 18
#define BTCTRL_GRID_SPACING 10
#define BTCTRL_DEVICE_WIDTH 260
#define BTCTRL_DEVICE_HEIGHT 96
#define BTCTRL_CANCEL_SIZE 96
#else
#define BTCTRL_TOP_MARGIN 8
#define BTCTRL_OUTER_SPACING 10
#define BTCTRL_GRID_SPACING 8
#define BTCTRL_DEVICE_WIDTH 210
#define BTCTRL_DEVICE_HEIGHT 72
#define BTCTRL_CANCEL_SIZE 72
#endif

class ImageButton;

class BluetoothControls : public Gtk::Box {
public:
  BluetoothControls(const std::string &iconPath,
                    const std::vector<BTDevice> &devices);
  virtual ~BluetoothControls() = default;

  sigc::signal<void, BTDevice> &signal_device_selected();
  sigc::signal<void> &signal_cancel();

private:
  std::string deviceLabel(const BTDevice &device) const;

  std::string m_iconPath;
  std::vector<BTDevice> m_devices;

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Label m_title;
  Gtk::Grid m_grid;

  ImageButton *m_cancelBtn = nullptr;

  sigc::signal<void, BTDevice> m_signalDeviceSelected;
  sigc::signal<void> m_signalCancel;
};
