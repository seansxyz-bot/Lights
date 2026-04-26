#include "lightshowsettings.h"

#include "../utils/logger.h"
#include "imgbutton.h"

#include <iomanip>
#include <sstream>

LightShowSettingsPage::LightShowSettingsPage(
    const std::string &iconPath, const LightShowTunables &tunables)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL), m_iconPath(iconPath) {
  LOG_INFO() << "LightShowSettingsPage ctor";

  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_START);
  set_margin_top(LIGHTSHOW_TOP_MARGIN);

  m_centBox.set_halign(Gtk::ALIGN_CENTER);
  m_centBox.set_valign(Gtk::ALIGN_START);
  m_centBox.set_spacing(LIGHTSHOW_OUTER_SPACING);

  Pango::FontDescription titleFont;
  titleFont.set_family("Sans");
  titleFont.set_weight(Pango::WEIGHT_BOLD);
  titleFont.set_size(LIGHTSHOW_TITLE_FONT_SIZE * PANGO_SCALE);

  m_title.override_font(titleFont);
  m_title.set_halign(Gtk::ALIGN_CENTER);

  m_gridBox.set_halign(Gtk::ALIGN_CENTER);
  m_gridBox.set_spacing(LIGHTSHOW_COL_SPACING);

  m_colA.set_spacing(LIGHTSHOW_ROW_SPACING);
  m_colB.set_spacing(LIGHTSHOW_ROW_SPACING);

  m_colA.pack_start(*makeControl("Sensitivity", LightShowControl::AgcTarget,
                                 tunables.agc_target.load(), 0.05, 2.00, 0.01),
                    Gtk::PACK_SHRINK);
  m_colA.pack_start(*makeControl("Intensity", LightShowControl::MasterGain,
                                 tunables.master_gain.load(), 0.00, 2.00, 0.01),
                    Gtk::PACK_SHRINK);
  m_colA.pack_start(*makeControl("Gamma", LightShowControl::Gamma,
                                 tunables.gamma.load(), 0.10, 3.00, 0.01),
                    Gtk::PACK_SHRINK);
  m_colA.pack_start(*makeControl("Contrast", LightShowControl::Contrast,
                                 tunables.color_contrast.load(), 0.50, 6.00,
                                 0.05),
                    Gtk::PACK_SHRINK);
  m_colA.pack_start(*makeControl("Saturation", LightShowControl::Saturation,
                                 tunables.saturation.load(), 0.00, 1.00, 0.01),
                    Gtk::PACK_SHRINK);

  m_colB.pack_start(*makeControl("Bass", LightShowControl::BassGain,
                                 tunables.band_gain_bass.load(), 0.00, 3.00,
                                 0.01),
                    Gtk::PACK_SHRINK);
  m_colB.pack_start(*makeControl("Mid", LightShowControl::MidGain,
                                 tunables.band_gain_mid.load(), 0.00, 3.00,
                                 0.01),
                    Gtk::PACK_SHRINK);
  m_colB.pack_start(*makeControl("High", LightShowControl::HighGain,
                                 tunables.band_gain_high.load(), 0.00, 3.00,
                                 0.01),
                    Gtk::PACK_SHRINK);
  m_colB.pack_start(*makeControl("Log Gain", LightShowControl::LogGain,
                                 tunables.log_gain.load(), 0.10, 4.00, 0.01),
                    Gtk::PACK_SHRINK);
  m_colB.pack_start(*makeControl("Drift", LightShowControl::DriftAmount,
                                 tunables.drift_amount.load(), 0.00, 1.00,
                                 0.01),
                    Gtk::PACK_SHRINK);
  m_colB.pack_start(*makeControl("Drift Speed", LightShowControl::DriftSpeed,
                                 tunables.drift_speed_scale.load(), 0.00, 3.00,
                                 0.01),
                    Gtk::PACK_SHRINK);

  m_gridBox.pack_start(m_colA, Gtk::PACK_SHRINK);
  m_gridBox.pack_start(m_colB, Gtk::PACK_SHRINK);

  auto *ok = Gtk::manage(new ImageButton(m_iconPath + "/ok.png",
                                         LIGHTSHOW_OK_SIZE, 0, 0));
  ok->set_halign(Gtk::ALIGN_CENTER);
  ok->signal_clicked().connect([this]() { m_signalDone.emit(); });

  m_centBox.pack_start(m_title, Gtk::PACK_SHRINK);
  m_centBox.pack_start(m_gridBox, Gtk::PACK_SHRINK);
  m_centBox.pack_start(*ok, Gtk::PACK_SHRINK);

  pack_start(m_centBox, Gtk::PACK_SHRINK);
  show_all_children();
}

Gtk::Box *LightShowSettingsPage::makeControl(
    const std::string &label, LightShowControl control, float value,
    double minValue, double maxValue, double step) {
  auto *row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
  row->set_spacing(LIGHTSHOW_ROW_SPACING);
  row->set_halign(Gtk::ALIGN_START);

  Pango::FontDescription labelFont;
  labelFont.set_family("Sans");
  labelFont.set_weight(Pango::WEIGHT_BOLD);
  labelFont.set_size(LIGHTSHOW_LABEL_FONT_SIZE * PANGO_SCALE);

  auto *name = Gtk::manage(new Gtk::Label(label));
  name->set_halign(Gtk::ALIGN_START);
  name->set_size_request(LIGHTSHOW_LABEL_WIDTH, -1);
  name->override_font(labelFont);

  auto *scale = Gtk::manage(new Gtk::Scale(Gtk::ORIENTATION_HORIZONTAL));
  scale->set_range(minValue, maxValue);
  scale->set_increments(step, step * 5.0);
  scale->set_value(value);
  scale->set_draw_value(false);
  scale->set_size_request(LIGHTSHOW_SCALE_WIDTH, -1);

  auto *valueLabel = Gtk::manage(new Gtk::Label(formatValue(value)));
  valueLabel->set_halign(Gtk::ALIGN_END);
  valueLabel->set_size_request(LIGHTSHOW_VALUE_WIDTH, -1);
  valueLabel->override_font(labelFont);

  scale->signal_value_changed().connect([this, scale, valueLabel, control]() {
    const float next = static_cast<float>(scale->get_value());
    valueLabel->set_text(formatValue(next));
    m_signalValueChanged.emit(control, next);
  });

  row->pack_start(*name, Gtk::PACK_SHRINK);
  row->pack_start(*scale, Gtk::PACK_SHRINK);
  row->pack_start(*valueLabel, Gtk::PACK_SHRINK);
  return row;
}

std::string LightShowSettingsPage::formatValue(float value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << value;
  return out.str();
}

sigc::signal<void, LightShowControl, float> &
LightShowSettingsPage::signal_value_changed() {
  return m_signalValueChanged;
}

sigc::signal<void> &LightShowSettingsPage::signal_done() {
  return m_signalDone;
}
