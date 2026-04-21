// ui_metrics.h
#pragma once

#include <algorithm>
#include <gtkmm.h>

namespace UiMetrics {

inline int screen_width() {
  auto display = Gdk::Display::get_default();
  if (!display)
    return 1024;

  auto monitor = display->get_primary_monitor();
  if (!monitor)
    return 1024;

  Gdk::Rectangle geo;
  monitor->get_geometry(geo);
  return geo.get_width();
}

inline bool is_small_screen() { return screen_width() <= 1024; }

inline int button_side() { return is_small_screen() ? 239 : 384; }

inline int button_margin() { return is_small_screen() ? 20 : 32; }

inline int color_picker_size() { return is_small_screen() ? 319 : 512; }

inline int color_bar_size() { return is_small_screen() ? 35 : 56; }

inline int keypad_button_size() { return is_small_screen() ? 72 : 96; }

} // namespace UiMetrics
