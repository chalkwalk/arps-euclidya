#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace PortGeo {

constexpr int PORT_RADIUS = 8;
constexpr int PORT_HIT_RADIUS = 14;
constexpr int PORT_SPACING = 24;
constexpr int HEADER_HEIGHT = 28;
constexpr int PORT_MARGIN = 14;
constexpr int TAB_HEIGHT = 20;

inline juce::Rectangle<int> inputPortRect(juce::Rectangle<int> body,
                                          int portIndex) {
  int y = body.getY() + HEADER_HEIGHT + 10 + (portIndex * PORT_SPACING);
  return {body.getX(), y - PORT_RADIUS, PORT_RADIUS, PORT_RADIUS * 2};
}

inline juce::Rectangle<int> outputPortRect(juce::Rectangle<int> body,
                                           int portIndex) {
  int y = body.getY() + HEADER_HEIGHT + 10 + (portIndex * PORT_SPACING);
  return {body.getRight() - PORT_RADIUS, y - PORT_RADIUS, PORT_RADIUS,
          PORT_RADIUS * 2};
}

inline int hitTestInput(juce::Point<int> localPoint, juce::Rectangle<int> body,
                        int numPorts) {
  auto localFloat = localPoint.toFloat();
  for (int i = 0; i < numPorts; ++i) {
    auto centre = inputPortRect(body, i).getCentre().toFloat();
    if (localFloat.getDistanceFrom(centre) <= (float)PORT_HIT_RADIUS)
      return i;
  }
  return -1;
}

inline int hitTestOutput(juce::Point<int> localPoint, juce::Rectangle<int> body,
                         int numPorts) {
  auto localFloat = localPoint.toFloat();
  for (int i = 0; i < numPorts; ++i) {
    auto centre = outputPortRect(body, i).getCentre().toFloat();
    if (localFloat.getDistanceFrom(centre) <= (float)PORT_HIT_RADIUS)
      return i;
  }
  return -1;
}

}  // namespace PortGeo
