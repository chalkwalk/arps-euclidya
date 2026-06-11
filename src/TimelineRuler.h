#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ClockManager.h"

class TimelineRuler : public juce::Component {
 public:
  explicit TimelineRuler(ClockManager &clockManager);

  void paint(juce::Graphics &g) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;
  void mouseDoubleClick(const juce::MouseEvent &e) override;  // Commit 4
  void mouseWheelMove(const juce::MouseEvent &e,
                      const juce::MouseWheelDetails &wheel) override;

  // Called from TransportBar's 30 Hz timer: follow-scroll + repaint.
  void tick();

 private:
  ClockManager &clock;
  double pixelsPerBar = 48.0;  // zoom; clamped to [8, 400]
  double viewStartPpq = 0.0;   // scroll; >= 0
  bool draggingPlayhead = false;

  // Manual pan (middle-button drag) state
  bool panning = false;
  float panAnchorX = 0.0f;
  double panAnchorViewStartPpq = 0.0;

  // Loop-drag state
  enum class LoopDragMode { None, Create, ResizeStart, ResizeEnd };
  LoopDragMode loopDragMode = LoopDragMode::None;
  double loopDragAnchorPpq = 0.0;  // fixed end during a resize drag

  static constexpr double kPpqPerBar = 4.0;
  // Auto-follow keeps the playhead inside this central band [low, high] of the
  // view width while playing; crossing it re-centres the view on the playhead.
  static constexpr double kFollowBandLow = 0.25;
  static constexpr double kFollowBandHigh = 0.75;

  double ppqForX(float x) const {
    return viewStartPpq + ((double)x / pixelsPerBar) * kPpqPerBar;
  }
  float xForPpq(double ppq) const {
    return (float)((ppq - viewStartPpq) / kPpqPerBar * pixelsPerBar);
  }

  // Snap ppq to beat or sixteenth grid; snapDisabled=true bypasses.
  double snapPpq(double ppq, bool snapDisabled) const;

  // View width in PPQ at the current zoom.
  double viewWidthPpq() const {
    return ((double)getWidth() / pixelsPerBar) * kPpqPerBar;
  }
  // Scroll so the playhead sits at the centre, clamped so the view never
  // scrolls before bar 1 (the "cap" at the timeline start).
  void centreOnPlayhead();
  // Auto-follow: re-centre only when the playhead leaves the central band.
  void followPlayhead();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineRuler)
};
