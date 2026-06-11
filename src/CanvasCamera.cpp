#include "CanvasCamera.h"

juce::AffineTransform CanvasCamera::getTransform() const {
  return juce::AffineTransform::scale(zoomFactor).translated(panX, panY);
}

juce::Point<int> CanvasCamera::getViewportGridCenter(
    juce::Rectangle<float> viewport) const {
  float cx = viewport.getCentreX();
  float cy = viewport.getCentreY();
  getTransform().inverted().transformPoint(cx, cy);
  return {(int)std::round(cx / Layout::GridPitchFloat),
          (int)std::round(cy / Layout::GridPitchFloat)};
}

bool CanvasCamera::applyWheelZoom(float wheelAmount,
                                  juce::Point<float> mousePos) {
  float zoomDelta = wheelAmount > 0.0f ? 1.1f : 0.9f;
  float newZoom = juce::jlimit(MIN_ZOOM, MAX_ZOOM, zoomFactor * zoomDelta);

  if (std::abs(newZoom - zoomFactor) <= 0.0001f)
    return false;

  float worldX = (mousePos.x - panX) / zoomFactor;
  float worldY = (mousePos.y - panY) / zoomFactor;

  zoomFactor = newZoom;
  panX = mousePos.x - worldX * zoomFactor;
  panY = mousePos.y - worldY * zoomFactor;
  return true;
}

void CanvasCamera::zoomToFit(juce::Rectangle<float> viewport,
                             juce::Rectangle<float> content) {
  if (content.getWidth() <= 0.0f || content.getHeight() <= 0.0f)
    return;

  float zoomX = viewport.getWidth() / content.getWidth();
  float zoomY = viewport.getHeight() / content.getHeight();

  zoomFactor = juce::jlimit(0.1f, MAX_ZOOM, std::min(zoomX, zoomY));

  float scaledW = content.getWidth() * zoomFactor;
  float scaledH = content.getHeight() * zoomFactor;

  panX = (viewport.getWidth() - scaledW) * 0.5f - content.getX() * zoomFactor;
  panY = (viewport.getHeight() - scaledH) * 0.5f - content.getY() * zoomFactor;
}

void CanvasCamera::setPanFromScrollX(double newRangeStart) {
  panX = (float)(-(double)newRangeStart * (double)zoomFactor);
}

void CanvasCamera::setPanFromScrollY(double newRangeStart) {
  panY = (float)(-(double)newRangeStart * (double)zoomFactor);
}
