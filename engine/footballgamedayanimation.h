#pragma once

#include "gamedayanimation.h"

class FootballGameDayAnimation : public GameDayAnimation {
public:
  explicit FootballGameDayAnimation(GameDayAnimationData data);

protected:
  void drawBackground(const Cairo::RefPtr<Cairo::Context> &cr, int w, int h,
                      double t) override;
};

