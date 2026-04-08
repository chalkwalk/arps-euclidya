#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

#include "ArpsLookAndFeel.h"
#include "GraphCanvas.h"
#include "MacroColours.h"
#include "ModuleLibraryPanel.h"
#include "PatchBrowserPanel.h"
#include "PluginProcessor.h"
#include "TransportBar.h"

class ArpsEuclidyaEditor : public juce::AudioProcessorEditor,
                           public juce::DragAndDropContainer,
                           private juce::Timer {
 public:
  ArpsEuclidyaEditor(ArpsEuclidyaProcessor &);
  ~ArpsEuclidyaEditor() override;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void timerCallback() override;

  void rebuildCanvas();

 private:
  void addNodeFromLibrary(const juce::String &nodeType);
  void setSelectedMacro(int index);

  int selectedMacro = -1;

  ArpsEuclidyaProcessor &audioProcessor;

  // Library panel
  ModuleLibraryPanel libraryPanel;
  juce::TextButton toggleSidebarButton;
  bool isSidebarExpanded = true;

  // Patch Management
  PatchLibrary patchLibrary;
  PatchBrowserPanel patchBrowser;

  // Graph canvas
  std::unique_ptr<GraphCanvas> graphCanvas;

  // Standalone Transport
  std::unique_ptr<TransportBar> transportBar;

  // Macros
  class MacroControl : public juce::Component {
   public:
    MacroControl() {
      addAndMakeVisible(slider);
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
      // Listen to slider mouse events so we can detect a click on the knob
      slider.addMouseListener(this, false);

      addAndMakeVisible(label);
      label.setJustificationType(juce::Justification::centred);
      label.setFont(juce::Font(juce::FontOptions(10.0f)));
      label.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    }

    void mouseUp(const juce::MouseEvent &e) override {
      if (e.mouseWasClicked() && onClicked)
        onClicked(macroIndex);
    }

    void paint(juce::Graphics &g) override {
      auto colour = getMacroColour(macroIndex);
      bool isSelected = (selectedMacroPtr && *selectedMacroPtr == macroIndex);

      // Draw background wrapper
      g.setColour(juce::Colour(0xff222222));
      g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

      if (isSelected) {
        // Bright solid outline for the selected macro
        g.setColour(colour);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f,
                               2.5f);
        // Inner glow fill
        g.setColour(colour.withAlpha(0.12f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
      } else if (isMapped) {
        // Dim colored border for mapped-but-not-selected
        g.setColour(colour.withAlpha(0.55f));
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
    int macroIndex = 0;
    int *selectedMacroPtr = nullptr;
    std::function<void(int)> onClicked;
  };

  juce::Component macroBar;
  juce::OwnedArray<MacroControl> macroControls;
  juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment>
      macroAttachments;

  ArpsLookAndFeel lookAndFeel;

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
      return false;  // Prevent default press/hold behaviour
    }

    bool mouseDraggedToKey(int, const juce::MouseEvent &) override {
      return false;  // Prevent drag painting
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

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpsEuclidyaEditor)
};
