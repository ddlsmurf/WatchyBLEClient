#include "watchy_display_state.h"

void WatchyDisplayState::alignTextTest(const GFXfont *font)
{
  alignText(font, "LT", ALIGN_NEAR, ALIGN_NEAR);
  alignText(font, "LM", ALIGN_NEAR, ALIGN_MIDDLE);
  alignText(font, "LB", ALIGN_NEAR, ALIGN_FAR);
  alignText(font, "MT", ALIGN_MIDDLE, ALIGN_NEAR);
  alignText(font, "MM", ALIGN_MIDDLE, ALIGN_MIDDLE);
  alignText(font, "MB", ALIGN_MIDDLE, ALIGN_FAR);
  alignText(font, "RT", ALIGN_FAR, ALIGN_NEAR);
  alignText(font, "RM", ALIGN_FAR, ALIGN_MIDDLE);
  alignText(font, "RB", ALIGN_FAR, ALIGN_FAR);
}