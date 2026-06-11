#include "TimelineRuler.h"

#include <cmath>

#include "ArpsLookAndFeel.h"

TimelineRuler::TimelineRuler(ClockManager &clockManager) : clock(clockManager) {
  setOpaque(true);
}

void TimelineRuler::tick() {
  // Auto-scroll: if the playhead exits the right edge while playing and the
  // user isn't mid-drag, page the view forward so the playhead lands ~10%
  // from the left.
  if (!draggingPlayhead && clock.isStandaloneRunning()) {
    const double ppq = clock.getTransportPpq();
    const double viewEndPpq =
        viewStartPpq + (ppqForX((float)getWidth()) - viewStartPpq);
    if (ppq >= viewEndPpq) {
      viewStartPpq = ppq - (ppqForX((float)getWidth()) - ppqForX(0.0f)) * 0.1;
      viewStartPpq = std::max(0.0, viewStartPpq);
    }
  }
  repaint();
}

double TimelineRuler::snapPpq(double ppq, bool snapDisabled) const {
  if (snapDisabled)
    return ppq;
  // Use sixteenth-note snapping when zoomed in enough, beat otherwise.
  const double snapUnit = (pixelsPerBar >= 128.0) ? 0.25 : 1.0;
  return std::round(ppq / snapUnit) * snapUnit;
}

void TimelineRuler::paint(juce::Graphics &g) {
  const int w = getWidth();
  const int h = getHeight();
  g.fillAll(juce::Colour(0xff1A1A1A));

  // Determine bar-label spacing: densest that keeps labels >= ~36 px apart.
  // Label spacing candidates: every 1, 2, 4, 8 bars.
  int labelEvery = 1;
  for (int candidate : {1, 2, 4, 8}) {
    if (pixelsPerBar * (double)candidate >= 36.0) {
      labelEvery = candidate;
      break;
    }
    labelEvery = candidate;
  }

  const int firstBar = (int)std::floor(viewStartPpq / kPpqPerBar);
  const int lastBar = (int)std::ceil(ppqForX((float)w) / kPpqPerBar) + 1;

  // Draw beat ticks (faint, only when zoom is high enough)
  const bool drawBeats = (pixelsPerBar >= 64.0);
  if (drawBeats) {
    g.setColour(juce::Colour(0xff333333));
    for (int bar = firstBar; bar <= lastBar; ++bar) {
      for (int beat = 1; beat < 4; ++beat) {
        double ppq = (double)bar * kPpqPerBar + (double)beat;
        float x = xForPpq(ppq);
        if (x < 0.0f || x > (float)w)
          continue;
        g.drawVerticalLine((int)x, (float)(h / 2), (float)h);
      }
    }
  }

  // Draw bar lines + labels
  g.setFont(juce::Font(juce::FontOptions(10.0f)));
  for (int bar = firstBar; bar <= lastBar; ++bar) {
    double ppq = (double)bar * kPpqPerBar;
    float x = xForPpq(ppq);
    if (x < 0.0f || x > (float)w)
      continue;
    g.setColour(juce::Colour(0xff555555));
    g.drawVerticalLine((int)x, 0.0f, (float)h);
    if ((bar % labelEvery) == 0 && bar >= 0) {
      g.setColour(juce::Colour(0xff888888));
      g.drawText(juce::String(bar + 1), (int)x + 2, 2, 32, h - 4,
                 juce::Justification::topLeft, false);
    }
  }

  // Draw playhead
  const double playPpq = clock.getTransportPpq();
  float px = xForPpq(playPpq);
  if (px >= 0.0f && px <= (float)w) {
    const juce::Colour neon = ArpsLookAndFeel::getNeonColor();
    g.setColour(neon);
    g.drawVerticalLine((int)px, 0.0f, (float)h);
    // Small downward triangle at the top
    juce::Path triangle;
    triangle.addTriangle(px - 4.0f, 0.0f, px + 4.0f, 0.0f, px, 6.0f);
    g.fillPath(triangle);
  }
}

void TimelineRuler::mouseDown(const juce::MouseEvent &e) {
  // Seek/drag in the lower two-thirds
  const float lowerThirdY = (float)getHeight() * (1.0f / 3.0f);
  if (e.y >= (int)lowerThirdY) {
    draggingPlayhead = true;
    const bool noSnap = e.mods.isShiftDown();
    clock.seekTransport(snapPpq(ppqForX((float)e.x), noSnap));
  }
}

void TimelineRuler::mouseDrag(const juce::MouseEvent &e) {
  if (draggingPlayhead) {
    const bool noSnap = e.mods.isShiftDown();
    clock.seekTransport(snapPpq(ppqForX((float)e.x), noSnap));
    repaint();
  }
}

void TimelineRuler::mouseUp(const juce::MouseEvent & /*e*/) {
  draggingPlayhead = false;
}

void TimelineRuler::mouseDoubleClick(const juce::MouseEvent & /*e*/) {
  // Commit 4: toggle loop enable on double-click in the loop band.
}

void TimelineRuler::mouseWheelMove(const juce::MouseEvent &e,
                                   const juce::MouseWheelDetails &wheel) {
  if (e.mods.isShiftDown()) {
    // Shift+wheel: horizontal scroll
    const double scrollDelta =
        -wheel.deltaY * (ppqForX((float)getWidth()) - viewStartPpq) * 0.15;
    viewStartPpq = std::max(0.0, viewStartPpq + scrollDelta);
  } else {
    // Zoom anchored at the cursor's PPQ position (same math as
    // CanvasCamera::applyWheelZoom).
    const double ppqUnderCursor = ppqForX((float)e.x);
    const double zoomDelta = wheel.deltaY > 0.0f ? 1.15 : (1.0 / 1.15);
    pixelsPerBar = juce::jlimit(8.0, 400.0, pixelsPerBar * zoomDelta);
    // Recompute viewStartPpq so ppqUnderCursor stays under the cursor.
    viewStartPpq = ppqUnderCursor - ((double)e.x / pixelsPerBar) * kPpqPerBar;
    viewStartPpq = std::max(0.0, viewStartPpq);
  }
  repaint();
}
