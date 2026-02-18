#pragma once

#include "GraphCanvas.h"
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class EuclideanArpEditor : public juce::AudioProcessorEditor,
                           private juce::Timer {
public:
  EuclideanArpEditor(EuclideanArpProcessor &);
  ~EuclideanArpEditor() override;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void timerCallback() override;

private:
  void addNodeFromLibrary(const juce::String &nodeType);

  EuclideanArpProcessor &audioProcessor;

  // Library panel
  juce::Viewport libraryViewport;
  juce::Component libraryContent;
  juce::OwnedArray<juce::TextButton> libraryButtons;

  // Graph canvas
  std::unique_ptr<GraphCanvas> graphCanvas;

  // Macros
  juce::Component macroBar;
  juce::OwnedArray<juce::Slider> macroSliders;
  juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment>
      macroAttachments;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuclideanArpEditor)
};
