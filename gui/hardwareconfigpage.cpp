#include "hardwareconfigpage.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace {
constexpr int kDefaultNumLeds = 19;
constexpr int kDefaultNumShiftRegs = 8;
constexpr int kMaxNumLeds = 32;
constexpr int kMaxNumShiftRegs = 16;

int clampValue(int value, int minValue, int maxValue) {
  return std::max(minValue, std::min(maxValue, value));
}
} // namespace

HardwareConfigPage::HardwareConfigPage(int numLeds, int numShiftRegs)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL),
      m_ledAdjustment(Gtk::Adjustment::create(
          clampValue(numLeds, 1, kMaxNumLeds), 1, kMaxNumLeds, 1, 5, 0)),
      m_shiftRegAdjustment(Gtk::Adjustment::create(
          clampValue(numShiftRegs, 1, kMaxNumShiftRegs), 1, kMaxNumShiftRegs,
          1, 2, 0)) {
  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_FILL);
  set_margin_top(24);
  set_margin_bottom(16);

  m_center.set_halign(Gtk::ALIGN_CENTER);
  m_center.set_valign(Gtk::ALIGN_CENTER);
  m_center.set_spacing(18);

  m_title.set_name("hardware-config-title");
  m_title.set_margin_bottom(8);

  m_grid.set_row_spacing(14);
  m_grid.set_column_spacing(24);
  m_grid.set_halign(Gtk::ALIGN_CENTER);

  m_ledSpin.set_adjustment(m_ledAdjustment);
  m_shiftRegSpin.set_adjustment(m_shiftRegAdjustment);
  m_ledSpin.set_numeric(true);
  m_shiftRegSpin.set_numeric(true);
  m_ledSpin.set_digits(0);
  m_shiftRegSpin.set_digits(0);
  m_ledSpin.set_size_request(120, 48);
  m_shiftRegSpin.set_size_request(120, 48);

  m_grid.attach(m_ledLabel, 0, 0, 1, 1);
  m_grid.attach(m_ledSpin, 1, 0, 1, 1);
  m_grid.attach(m_shiftRegLabel, 0, 1, 1, 1);
  m_grid.attach(m_shiftRegSpin, 1, 1, 1, 1);

  m_warningLabel.set_halign(Gtk::ALIGN_CENTER);
  m_warningLabel.set_line_wrap(true);
  m_warningLabel.set_max_width_chars(42);

  m_buttonRow.set_halign(Gtk::ALIGN_CENTER);
  m_buttonRow.set_spacing(24);
  m_cancelButton.set_size_request(140, 56);
  m_okButton.set_size_request(140, 56);
  m_buttonRow.pack_start(m_cancelButton, Gtk::PACK_SHRINK);
  m_buttonRow.pack_start(m_okButton, Gtk::PACK_SHRINK);

  m_center.pack_start(m_title, Gtk::PACK_SHRINK);
  m_center.pack_start(m_grid, Gtk::PACK_SHRINK);
  m_center.pack_start(m_warningLabel, Gtk::PACK_SHRINK);
  m_center.pack_start(m_buttonRow, Gtk::PACK_SHRINK);
  pack_start(m_center, Gtk::PACK_EXPAND_WIDGET);

  m_cancelButton.signal_clicked().connect(
      [this]() { m_signalCancelRequested.emit(); });
  m_okButton.signal_clicked().connect(
      sigc::mem_fun(*this, &HardwareConfigPage::onApplyClicked));

  show_all_children();
}

void HardwareConfigPage::set_busy(bool busy) {
  m_ledSpin.set_sensitive(!busy);
  m_shiftRegSpin.set_sensitive(!busy);
  m_cancelButton.set_sensitive(!busy);
  m_okButton.set_sensitive(!busy);
}

void HardwareConfigPage::onApplyClicked() {
  const int numLeds = m_ledSpin.get_value_as_int();
  const int numShiftRegs = m_shiftRegSpin.get_value_as_int();

  if (numLeds < 1 || numLeds > kMaxNumLeds || numShiftRegs < 1 ||
      numShiftRegs > kMaxNumShiftRegs) {
    m_warningLabel.set_text("Values must be LEDs 1-32 and shift registers 1-16.");
    return;
  }

  const int requiredShiftRegs =
      static_cast<int>(std::ceil((numLeds * 3) / 8.0));
  if (numShiftRegs < requiredShiftRegs) {
    std::ostringstream msg;
    msg << numLeds << " RGB LEDs need at least " << requiredShiftRegs
        << " shift registers.";
    m_warningLabel.set_text(msg.str());
    return;
  }

  m_warningLabel.set_text("");
  m_signalApplyRequested.emit(numLeds, numShiftRegs);
}

sigc::signal<void, int, int> &HardwareConfigPage::signal_apply_requested() {
  return m_signalApplyRequested;
}

sigc::signal<void> &HardwareConfigPage::signal_cancel_requested() {
  return m_signalCancelRequested;
}
