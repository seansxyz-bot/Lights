#pragma once

#include "gamedayanimation.h"

class BaseballGameDayAnimation : public GameDayAnimation {
public:
  explicit BaseballGameDayAnimation(GameDayAnimationData data);

protected:
  void drawBackground(const Cairo::RefPtr<Cairo::Context> &cr, int w, int h,
                      double t) override;
};

