#include "footballgamedayanimation.h"

#include <cmath>
#include <utility>

FootballGameDayAnimation::FootballGameDayAnimation(GameDayAnimationData data)
    : GameDayAnimation(std::move(data)) {}

void FootballGameDayAnimation::drawBackground(const Cairo::RefPtr<Cairo::Context> &cr,
                                              int w, int h, double t) {
  auto grad = Cairo::LinearGradient::create(0, 0, 0, h);
  grad->add_color_stop_rgb(0, 0.02, 0.03, 0.04);
  grad->add_color_stop_rgb(0.35, 0.08, 0.22, 0.10);
  grad->add_color_stop_rgb(1, 0.03, 0.36, 0.14);
  cr->set_source(grad);
  cr->rectangle(0, 0, w, h);
  cr->fill();

  cr->set_source_rgba(1, 1, 1, 0.30);
  cr->set_line_width(3);
  for (int x = 62; x < w; x += 90) {
    cr->move_to(x, 190);
    cr->line_to(x, h);
    cr->stroke();
  }
  cr->set_source_rgba(1, 1, 1, 0.70);
  cr->set_line_width(6);
  cr->move_to(w * 0.5, 170);
  cr->line_to(w * 0.5, h);
  cr->stroke();

  cr->select_font_face("DejaVu Sans", Cairo::FONT_SLANT_NORMAL,
                       Cairo::FONT_WEIGHT_BOLD);
  cr->set_font_size(54);
  cr->set_source_rgba(1, 1, 1, 0.45);
  cr->move_to(w * 0.5 - 42, h - 78 + std::sin(t) * 2);
  cr->show_text("50");

  cr->set_source_rgba(1, 1, 1, 0.13);
  for (int i = 0; i < 8; ++i) {
    cr->arc(90 + i * 135, 68, 58, 0, 6.283185307);
    cr->fill();
  }
}
