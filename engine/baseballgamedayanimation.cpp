#include "baseballgamedayanimation.h"

#include <cmath>
#include <utility>

BaseballGameDayAnimation::BaseballGameDayAnimation(GameDayAnimationData data)
    : GameDayAnimation(std::move(data)) {}

void BaseballGameDayAnimation::drawBackground(const Cairo::RefPtr<Cairo::Context> &cr,
                                              int w, int h, double t) {
  auto sky = Cairo::LinearGradient::create(0, 0, 0, h);
  sky->add_color_stop_rgb(0, 0.02, 0.03, 0.08);
  sky->add_color_stop_rgb(0.35, 0.05, 0.10, 0.18);
  sky->add_color_stop_rgb(1, 0.04, 0.28, 0.12);
  cr->set_source(sky);
  cr->rectangle(0, 0, w, h);
  cr->fill();

  cr->set_source_rgb(0.11, 0.38, 0.15);
  cr->rectangle(0, 250, w, h - 250);
  cr->fill();

  cr->set_source_rgb(0.62, 0.40, 0.22);
  cr->move_to(w * 0.5, 338);
  cr->line_to(180, h);
  cr->line_to(844, h);
  cr->close_path();
  cr->fill();

  cr->set_source_rgb(0.84, 0.78, 0.64);
  cr->move_to(w * 0.5, 388);
  cr->line_to(396, 492);
  cr->line_to(w * 0.5, 566);
  cr->line_to(628, 492);
  cr->close_path();
  cr->fill();

  cr->set_source_rgba(1, 1, 1, 0.35);
  cr->set_line_width(4);
  cr->move_to(w * 0.5, 566);
  cr->line_to(290, 265);
  cr->move_to(w * 0.5, 566);
  cr->line_to(734, 265);
  cr->stroke();

  cr->set_source_rgba(1, 1, 1, 0.14);
  for (int i = 0; i < 8; ++i) {
    cr->arc(90 + i * 135, 68 + std::sin(t + i) * 2, 58, 0, 6.283185307);
    cr->fill();
  }
}
