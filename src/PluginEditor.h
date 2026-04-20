#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

#include "ArpsLookAndFeel.h"
#include "GraphCanvas.h"
#include "MacroColours.h"
#include "MacroParameter.h"
#include "ModuleLibraryPanel.h"
#include "PatchBrowserPanel.h"
#include "PluginProcessor.h"
#include "TransportBar.h"
#include "Tuning/TuningPanel.h"

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
  std::vector<int> highlightedMacros;

  ArpsEuclidyaProcessor &audioProcessor;

  // Library panel
  ModuleLibraryPanel libraryPanel;
  juce::TextButton toggleSidebarButton;
  bool isSidebarExpanded = true;

  // Patch Management
  PatchLibrary patchLibrary;
  PatchBrowserPanel patchBrowser;

  // Tuning
  TuningLibrary tuningLibrary;
  TuningPanel tuningPanel;

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
      label.setInterceptsMouseClicks(false, false);
    }

    void mouseDown(const juce::MouseEvent &e) override {
      if (e.mods.isPopupMenu()) {
        juce::PopupMenu menu;
        menu.addItem(1, "Clear all bindings for Macro " +
                            juce::String(macroIndex + 1));
        menu.addItem(2, "Clear learned CC",
                     learnedCCPtr != nullptr && *learnedCCPtr >= 0);
        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(this),
            [this](int result) {
              if (result == 1 && onClearMacro) { onClearMacro(); }
              if (result == 2 && onClearLearnedCC) { onClearLearnedCC(); }
            });
      }
    }

    void mouseUp(const juce::MouseEvent &e) override {
      if (e.mouseWasClicked() && !e.mods.isPopupMenu() && onClicked) {
        onClicked(macroIndex);
      }
    }


    void mouseEnter(const juce::MouseEvent &e) override {
      juce::Component::mouseEnter(e);
      if (onHoverMacro) { onHoverMacro(macroIndex); }
    }

    void mouseExit(const juce::MouseEvent &e) override {
      juce::Component::mouseExit(e);
      if (onHoverMacro) { onHoverMacro(-1); }
    }

    void mouseDoubleClick(const juce::MouseEvent &e) override {
      if (e.mods.isShiftDown()) {
        if (onEnterLearn) {
          onEnterLearn(macroIndex);
        }
        return;
      }
      if (onToggleBipolar) {
        onToggleBipolar(macroIndex);
      }
      // Re-select after the toggle-off caused by the second click in the pair
      if (onClicked) {
        onClicked(macroIndex);
      }
    }

    void paint(juce::Graphics &g) override {
      auto colour = getMacroColour(macroIndex);
      bool isSelected = (selectedMacroPtr && *selectedMacroPtr == macroIndex);
      bool isLearning = (learnMacroIndexPtr != nullptr &&
                         learnMacroIndexPtr->load() == macroIndex);

      // Draw background wrapper
      g.setColour(juce::Colour(0xff222222));
      g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

      // Hover highlight: bright white outer ring when a bound control is hovered
      bool isHovered =
          highlightedMacrosPtr != nullptr &&
          std::find(highlightedMacrosPtr->begin(), highlightedMacrosPtr->end(),
                    macroIndex) != highlightedMacrosPtr->end();
      if (isHovered) {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f,
                               2.0f);
      }

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

      // MIDI learn: pulsing amber ring
      if (isLearning) {
        float phase = std::fmod(
            (float)(juce::Time::getMillisecondCounterHiRes() / 500.0), 1.0f);
        float alpha = 0.4f + (0.6f * std::abs(std::sin(phase * juce::MathConstants<float>::pi)));
        g.setColour(juce::Colour(0xffffaa00).withAlpha(alpha));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 2.5f);
        g.setColour(juce::Colour(0xffffaa00).withAlpha(alpha * 0.2f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
      }

      // Learned-CC indicator: small "CC#" label bottom-left
      if (learnedCCPtr != nullptr && *learnedCCPtr >= 0) {
        g.setColour(juce::Colour(0xffffaa00).withAlpha(0.8f));
        g.setFont(juce::FontOptions(7.0f));
        g.drawText("CC" + juce::String(*learnedCCPtr),
                   getLocalBounds().removeFromBottom(10).removeFromLeft(24),
                   juce::Justification::centred, false);
      }

      // Bipolar indicator: "±" in the top-right corner
      bool isBipolar =
          (macroParamPtr != nullptr && macroParamPtr->isBipolar());
      if (isBipolar) {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(juce::FontOptions(7.0f));
        g.drawText(juce::String::fromUTF8("\xc2\xb1"),
                   getLocalBounds().removeFromTop(10).removeFromRight(10),
                   juce::Justification::centred, false);
      }
    }

    // Draw a small tick at 12 o'clock on the slider to mark the bipolar zero
    // point (value = 0.5).
    void paintOverChildren(juce::Graphics &g) override {
      if (macroParamPtr == nullptr || !macroParamPtr->isBipolar()) {
        return;
      }

      auto sliderBounds = slider.getBoundsInParent().toFloat();
      float cx = sliderBounds.getCentreX();
      float cy = sliderBounds.getCentreY();
      float radius =
          (juce::jmin(sliderBounds.getWidth(), sliderBounds.getHeight()) / 2.0f) -
          2.0f;
      // 12 o'clock is straight up from centre (angle = -π/2)
      float tickAngle = -juce::MathConstants<float>::halfPi;
      float trackWidth = radius * 0.4f;
      float inner = radius - (trackWidth * 0.5f);
      float outer = radius + (trackWidth * 0.5f);
      g.setColour(juce::Colours::white.withAlpha(0.7f));
      g.drawLine(cx + (inner * std::cos(tickAngle)),
                 cy + (inner * std::sin(tickAngle)),
                 cx + (outer * std::cos(tickAngle)),
                 cy + (outer * std::sin(tickAngle)), 1.5f);
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
    const std::vector<int> *highlightedMacrosPtr = nullptr;
    MacroParameter *macroParamPtr = nullptr;
    const std::atomic<int> *learnMacroIndexPtr = nullptr;
    const int *learnedCCPtr = nullptr;
    std::function<void(int)> onClicked;
    std::function<void(int)> onToggleBipolar;
    std::function<void()> onClearMacro;
    std::function<void(int)> onHoverMacro;
    std::function<void(int)> onEnterLearn;
    std::function<void()> onClearLearnedCC;
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

    bool mouseDownOnKey(int midiNoteNumber, const juce::MouseEvent & /*e*/) override {
      if (keyboardState.isNoteOn(1, midiNoteNumber)) {
        keyboardState.noteOff(1, midiNoteNumber, 0.0f);
      } else {
        keyboardState.noteOn(1, midiNoteNumber, 1.0f);
      }
      return false;  // Prevent default press/hold behaviour
    }

    bool mouseDraggedToKey(int /*noteNumber*/, const juce::MouseEvent & /*e*/) override {
      return false;  // Prevent drag painting
    }

    void mouseUpOnKey(int /*noteNumber*/, const juce::MouseEvent & /*e*/) override {
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
