#pragma once

#include "EuclideanLookAndFeel.h"
#include "GraphCanvas.h"
#include "ModuleLibraryPanel.h"
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

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
  class MacroControl : public juce::Component {
  public:
    MacroControl() {
      addAndMakeVisible(slider);
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

      addAndMakeVisible(label);
      label.setJustificationType(juce::Justification::centred);
      label.setFont(juce::Font(10.0f));
      label.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    }

    void paint(juce::Graphics &g) override {
      // Draw background wrapper
      g.setColour(juce::Colour(0xff222222));
      g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

      if (isMapped) {
        // Draw mapped highlight (cyan/teal border)
        g.setColour(juce::Colour(0xff00ffff).withAlpha(0.6f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f,
                               1.5f);
      } else {
        g.setColour(juce::Colour(0xff444444));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f,
                               1.0f);
      }
    }

    void resized() override {
      auto bounds = getLocalBounds().reduced(2);
      label.setBounds(bounds.removeFromBottom(16));
      slider.setBounds(bounds);
    }

    juce::Slider slider;
    juce::Label label;
    bool isMapped = false;
  };

  juce::Component macroBar;
  juce::OwnedArray<MacroControl> macroControls;
  juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment>
      macroAttachments;

  EuclideanLookAndFeel customLookAndFeel;

  class LatchingKeyboardComponent : public juce::MidiKeyboardComponent {
  public:
    LatchingKeyboardComponent(juce::MidiKeyboardState &state, Orientation o)
        : juce::MidiKeyboardComponent(state, o), keyboardState(state) {}

    bool mouseDownOnKey(int midiNoteNumber, const juce::MouseEvent &) override {
      if (keyboardState.isNoteOn(1, midiNoteNumber)) {
        keyboardState.noteOff(1, midiNoteNumber, 0.0f);
      } else {
        keyboardState.noteOn(1, midiNoteNumber, 1.0f);
      }
      return false; // Prevent default press/hold behaviour
    }

    bool mouseDraggedToKey(int, const juce::MouseEvent &) override {
      return false; // Prevent drag painting
    }

    void mouseUpOnKey(int, const juce::MouseEvent &) override {
      // Do nothing, releasing the mouse should not lift the latch
    }

  private:
    juce::MidiKeyboardState &keyboardState;
  };

  LatchingKeyboardComponent midiKeyboard;
  juce::TextButton clearNotesButton;

  juce::OpenGLContext openGLContext;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuclideanArpEditor)
};
