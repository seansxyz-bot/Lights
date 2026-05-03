#pragma once

#include "gamedayanimation.h"

class HockeyGameDayAnimation : public GameDayAnimation {
public:
  explicit HockeyGameDayAnimation(GameDayAnimationData data);

protected:
  void drawBackground(const Cairo::RefPtr<Cairo::Context> &cr, int w, int h,
                      double t) override;
};

