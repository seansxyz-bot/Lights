#include "hockeygamedayanimation.h"

#include <cmath>
#include <utility>

HockeyGameDayAnimation::HockeyGameDayAnimation(GameDayAnimationData data)
    : GameDayAnimation(std::move(data)) {}

void HockeyGameDayAnimation::drawBackground(const Cairo::RefPtr<Cairo::Context> &cr,
                                            int w, int h, double t) {
  auto grad = Cairo::LinearGradient::create(0, 0, 0, h);
  grad->add_color_stop_rgb(0, 0.05, 0.08, 0.12);
  grad->add_color_stop_rgb(0.45, 0.68, 0.86, 0.93);
  grad->add_color_stop_rgb(1, 0.88, 0.96, 1.0);
  cr->set_source(grad);
  cr->rectangle(0, 0, w, h);
  cr->fill();

  cr->set_source_rgba(1, 1, 1, 0.20);
  for (int i = 0; i < 9; ++i) {
    cr->move_to(0, 150 + i * 34 + std::sin(t + i) * 4);
    cr->line_to(w, 110 + i * 38);
  }
  cr->set_line_width(2);
  cr->stroke();

  cr->set_source_rgba(0.05, 0.20, 0.44, 0.62);
  cr->set_line_width(5);
  cr->move_to(w * 0.5, 155);
  cr->line_to(w * 0.5, h);
  cr->stroke();
  cr->arc(w * 0.5, 356, 112, 0, 6.283185307);
  cr->stroke();

  cr->set_source_rgba(0.9, 0.95, 1.0, 0.16);
  for (int i = 0; i < 7; ++i) {
    cr->arc(120 + i * 140, 70, 70, 0, 6.283185307);
    cr->fill();
  }
}
