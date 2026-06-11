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

  // Loop-drag state
  enum class LoopDragMode { None, Create, ResizeStart, ResizeEnd };
  LoopDragMode loopDragMode = LoopDragMode::None;
  double loopDragAnchorPpq = 0.0;  // fixed end during a resize drag

  static constexpr double kPpqPerBar = 4.0;

  double ppqForX(float x) const {
    return viewStartPpq + ((double)x / pixelsPerBar) * kPpqPerBar;
  }
  float xForPpq(double ppq) const {
    return (float)((ppq - viewStartPpq) / kPpqPerBar * pixelsPerBar);
  }

  // Snap ppq to beat or sixteenth grid; snapDisabled=true bypasses.
  double snapPpq(double ppq, bool snapDisabled) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineRuler)
};
