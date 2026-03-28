#pragma once

namespace Layout {
// The length of a single logical grid cell along X/Y axes (including margins)
constexpr int GridPitch = 100;
constexpr float GridPitchFloat = static_cast<float>(GridPitch);

// The width in pixels of the empty tramlines separating physical nodes
constexpr int TramlineMargin = 10;

// The required float offset inside the grid pitch to securely land the node
// past the grid boundary
constexpr float TramlineOffset = static_cast<float>(TramlineMargin) / 2.0f;
}  // namespace Layout
