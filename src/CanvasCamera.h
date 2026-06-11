#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "LayoutConstants.h"

class CanvasCamera {
 public:
  static constexpr float MIN_ZOOM = 0.2f;
  static constexpr float MAX_ZOOM = 3.0f;

  float panX = 0.0f;
  float panY = 0.0f;
  float zoomFactor = 1.0f;

  [[nodiscard]] juce::AffineTransform getTransform() const;

  // Returns the grid-coordinate of the center of the given screen viewport.
  [[nodiscard]] juce::Point<int> getViewportGridCenter(
      juce::Rectangle<float> viewport) const;

  // Anchor-preserving wheel zoom; returns true if the zoom actually changed.
  bool applyWheelZoom(float wheelAmount, juce::Point<float> mousePos);

  // Fit content rect inside the viewport; updates panX/panY/zoomFactor.
  void zoomToFit(juce::Rectangle<float> viewport,
                 juce::Rectangle<float> content);

  // Called by scrollBarMoved to update pan from a scroll position.
  void setPanFromScrollX(double newRangeStart);
  void setPanFromScrollY(double newRangeStart);
};
