#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>

class HardwareConfigPage : public Gtk::Box {
public:
  HardwareConfigPage(int numLeds, int numShiftRegs);
  ~HardwareConfigPage() override = default;

  void set_busy(bool busy);

  sigc::signal<void, int, int> &signal_apply_requested();
  sigc::signal<void> &signal_cancel_requested();

private:
  void onApplyClicked();

  Gtk::Box m_center{Gtk::ORIENTATION_VERTICAL};
  Gtk::Grid m_grid;
  Gtk::Label m_title{"Hardware Config"};
  Gtk::Label m_ledLabel{"Number of RGB LEDs"};
  Gtk::Label m_shiftRegLabel{"Number of shift registers"};
  Gtk::Label m_warningLabel;
  Glib::RefPtr<Gtk::Adjustment> m_ledAdjustment;
  Glib::RefPtr<Gtk::Adjustment> m_shiftRegAdjustment;
  Gtk::SpinButton m_ledSpin;
  Gtk::SpinButton m_shiftRegSpin;
  Gtk::Box m_buttonRow{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Button m_cancelButton{"Cancel"};
  Gtk::Button m_okButton{"OK"};

  sigc::signal<void, int, int> m_signalApplyRequested;
  sigc::signal<void> m_signalCancelRequested;
};
