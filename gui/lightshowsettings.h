#pragma once

#include "../engine/lightshow.h"

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>

#if (SCREEN == 1)
#define LIGHTSHOW_TOP_MARGIN 14
#define LIGHTSHOW_OUTER_SPACING 12
#define LIGHTSHOW_COL_SPACING 34
#define LIGHTSHOW_ROW_SPACING 7
#define LIGHTSHOW_LABEL_WIDTH 118
#define LIGHTSHOW_VALUE_WIDTH 46
#define LIGHTSHOW_SCALE_WIDTH 230
#define LIGHTSHOW_TITLE_FONT_SIZE 24
#define LIGHTSHOW_LABEL_FONT_SIZE 15
#define LIGHTSHOW_OK_SIZE 86
#else
#define LIGHTSHOW_TOP_MARGIN 4
#define LIGHTSHOW_OUTER_SPACING 7
#define LIGHTSHOW_COL_SPACING 18
#define LIGHTSHOW_ROW_SPACING 5
#define LIGHTSHOW_LABEL_WIDTH 86
#define LIGHTSHOW_VALUE_WIDTH 38
#define LIGHTSHOW_SCALE_WIDTH 150
#define LIGHTSHOW_TITLE_FONT_SIZE 16
#define LIGHTSHOW_LABEL_FONT_SIZE 11
#define LIGHTSHOW_OK_SIZE 60
#endif

enum class LightShowControl {
  AgcTarget,
  MasterGain,
  Gamma,
  Saturation,
  Contrast,
  LogGain,
  BassGain,
  MidGain,
  HighGain,
  DriftAmount,
  DriftSpeed,
};

class LightShowSettingsPage : public Gtk::Box {
public:
  LightShowSettingsPage(const std::string &iconPath,
                        const LightShowTunables &tunables);

  sigc::signal<void, LightShowControl, float> &signal_value_changed();
  sigc::signal<void> &signal_done();

private:
  Gtk::Box *makeControl(const std::string &label, LightShowControl control,
                        float value, double minValue, double maxValue,
                        double step);
  static std::string formatValue(float value);

  std::string m_iconPath;

  Gtk::Box m_centBox{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_gridBox{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box m_colA{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box m_colB{Gtk::ORIENTATION_VERTICAL};
  Gtk::Label m_title{"LightShow"};

  sigc::signal<void, LightShowControl, float> m_signalValueChanged;
  sigc::signal<void> m_signalDone;
};
