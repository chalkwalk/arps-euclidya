#pragma once

#include "EuclideanLookAndFeel.h"
#include "GraphCanvas.h"
#include "ModuleLibraryPanel.h"
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class EuclideanArpEditor : public juce::AudioProcessorEditor,
                           public juce::DragAndDropContainer,
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
  ModuleLibraryPanel libraryPanel;
  juce::TextButton toggleSidebarButton;
  bool isSidebarExpanded = true;

  // Graph canvas
  std::unique_ptr<GraphCanvas> graphCanvas;

  // Macros
  juce::Component macroBar;
  juce::OwnedArray<juce::Slider> macroSliders;
  juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment>
      macroAttachments;

  EuclideanLookAndFeel customLookAndFeel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuclideanArpEditor)
};
