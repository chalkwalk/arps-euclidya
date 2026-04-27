#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GraphNode.h"

class CustomMacroSlider : public juce::Slider {
 public:
  std::function<void()> onRightClick;

  // Wired by NodeBlock to enable alt+drag intensity binding
  int *selectedMacroPtr = nullptr;
  MacroParam *macroParamRef = nullptr;
  std::function<int()> getNextFreeMacro;                   // → free macro index or -1
  std::function<void(std::vector<int>)> onHoverMacros;     // notify editor of hover state
  std::function<void()> onMappingChanged;                  // update DAW names + repaint
  std::function<void()> onBindingChanged;                  // deferred rebuild
  std::function<void(MacroParam *)> onRequestMidiLearn;    // alt+double-click → MIDI learn

  double defaultValue = 0.0;  // set by NodeBlock; double-click resets to this

  void mouseEnter(const juce::MouseEvent &e) override {
    juce::Slider::mouseEnter(e);
    if (onHoverMacros && macroParamRef != nullptr) {
      std::vector<int> indices;
      indices.reserve(macroParamRef->bindings.size());
      for (const auto &b : macroParamRef->bindings) {
        indices.push_back(b.macroIndex);
      }
      onHoverMacros(std::move(indices));
    }
  }

  void mouseExit(const juce::MouseEvent &e) override {
    juce::Slider::mouseExit(e);
    if (onHoverMacros) {
      onHoverMacros({});
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick) {
        onRightClick();
      }
      return;
    }
    // Alt = macro binding gestures (takes priority over all other modifiers)
    if (e.mods.isAltDown() && macroParamRef != nullptr) {
      if (e.getNumberOfClicks() >= 2 && onRequestMidiLearn) {
        onRequestMidiLearn(macroParamRef);
        return;
      }
      startAltDrag();
      return;
    }
    // Double-click (any modifier except Alt) = reset to default
    if (e.getNumberOfClicks() >= 2) {
      setValue(defaultValue, juce::sendNotificationSync);
      return;
    }
    // Shift = fine-adjust drag (Bitwig-style: ~¼ speed)
    if (e.mods.isShiftDown()) {
      enterFineDrag(getValue(), e.getPosition().y);
      return;
    }
    // Ctrl/Cmd = snap drag — bypass JUCE's mouseDown to avoid its own Ctrl=fine mode
    if (e.mods.isCommandDown()) {
      isSnapDragging = true;
      enterCoarseDrag(getValue(), e.getPosition().y);
      return;
    }
    // Normal drag
    isNormalDragging = true;
    juceMouseDownCalled = true;
    juce::Slider::mouseDown(e);
  }

  void mouseDoubleClick(const juce::MouseEvent & /*e*/) override {
    // Suppress JUCE's default text-entry popup; reset is handled in mouseDown
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (isAltDragging) {
      if (altDragMacroIndex < 0) {
        return;
      }
      float dy = static_cast<float>(e.getDistanceFromDragStartY());
      float newIntensity =
          juce::jlimit(-2.0f, 2.0f, altDragStartIntensity - (dy / 150.0f));
      applyIntensity(newIntensity);
      return;
    }
    // Handle Shift toggle mid-drag in either direction
    if (isFineDragging && !e.mods.isShiftDown()) {
      enterCoarseDrag(getValue(), e.getPosition().y);
    } else if ((isNormalDragging || isManualCoarseDragging) && e.mods.isShiftDown()) {
      enterFineDrag(getValue(), e.getPosition().y);
    }

    // Keep snap flag in sync with Ctrl/Cmd key throughout the drag.
    // If Ctrl is released during a JUCE-driven drag, switch to manual to avoid
    // the stale-origin jump that would occur if we let JUCE recompute from mouseDown.
    if (!isFineDragging) {
      bool ctrlDown = e.mods.isCommandDown();
      if (isNormalDragging && !ctrlDown && isSnapDragging) {
        enterCoarseDrag(getValue(), e.getPosition().y);
      }
      isSnapDragging = ctrlDown;
    }

    if (isFineDragging) {
      double range = getMaximum() - getMinimum();
      double delta = static_cast<double>(fineDragSwitchY - e.getPosition().y) *
                     range / 200.0 * fineDragScale;
      setValue(juce::jlimit(getMinimum(), getMaximum(), fineDragStartValue + delta),
               juce::sendNotificationSync);
      return;
    }
    if (isManualCoarseDragging) {
      double range = getMaximum() - getMinimum();
      double rawValue = coarseDragStartValue +
                        (static_cast<double>(coarseDragSwitchY - e.getPosition().y) *
                         range / 200.0);
      if (isSnapDragging) {
        double inc = range / 20.0;
        if (getInterval() > 0.0) {
          inc = getInterval() * 5.0;
        }
        rawValue = std::round(rawValue / inc) * inc;
      }
      setValue(juce::jlimit(getMinimum(), getMaximum(), rawValue),
               juce::sendNotificationSync);
      return;
    }
    juce::Slider::mouseDrag(e);
  }

  void mouseUp(const juce::MouseEvent &e) override {
    if (isAltDragging) {
      isAltDragging = false;
      altDragMacroIndex = -1;
      if (bindingModified && onBindingChanged) {
        // Defer so we don't destroy this slider from within its own mouseUp
        juce::MessageManager::callAsync([cb = onBindingChanged]() { cb(); });
      }
      return;
    }
    if (isFineDragging || isManualCoarseDragging) {
      isFineDragging = false;
      isManualCoarseDragging = false;
      isNormalDragging = false;
      isSnapDragging = false;
      // Close the JUCE gesture if we opened it (needed for undo/DAW automation)
      if (juceMouseDownCalled) {
        juceMouseDownCalled = false;
        juce::Slider::mouseUp(e);
      }
      return;
    }
    isSnapDragging = false;
    isNormalDragging = false;
    juceMouseDownCalled = false;
    if (e.mods.isPopupMenu()) {
      return;
    }
    juce::Slider::mouseUp(e);
  }

  double snapValue(double attemptedValue, juce::Slider::DragMode mode) override {
    if (!isSnapDragging || mode == juce::Slider::notDragging) {
      return attemptedValue;
    }
    double inc = (getMaximum() - getMinimum()) / 20.0;
    if (getInterval() > 0.0) {
      inc = getInterval() * 5.0;
    }
    return std::round(attemptedValue / inc) * inc;
  }

 private:
  static constexpr double fineDragScale = 0.25;

  bool isAltDragging = false;
  int altDragMacroIndex = -1;
  float altDragStartIntensity = 0.0f;
  bool bindingModified = false;

  bool isFineDragging = false;
  bool isManualCoarseDragging = false;
  bool isNormalDragging = false;
  double fineDragStartValue = 0.0;
  int fineDragSwitchY = 0;
  double coarseDragStartValue = 0.0;
  int coarseDragSwitchY = 0;

  bool isSnapDragging = false;
  bool juceMouseDownCalled = false;

  void enterFineDrag(double currentValue, int currentY) {
    fineDragStartValue = currentValue;
    fineDragSwitchY = currentY;
    isFineDragging = true;
    isManualCoarseDragging = false;
    isNormalDragging = false;
  }

  void enterCoarseDrag(double currentValue, int currentY) {
    coarseDragStartValue = currentValue;
    coarseDragSwitchY = currentY;
    isManualCoarseDragging = true;
    isFineDragging = false;
    isNormalDragging = false;
  }

  void startAltDrag() {
    int macroIdx = (selectedMacroPtr != nullptr) ? *selectedMacroPtr : -1;
    if (macroIdx == -1 && getNextFreeMacro) {
      macroIdx = getNextFreeMacro();
    }
    if (macroIdx < 0) {
      return;
    }

    altDragMacroIndex = macroIdx;
    altDragStartIntensity = 0.0f;
    for (const auto &b : macroParamRef->bindings) {
      if (b.macroIndex == macroIdx) {
        altDragStartIntensity = b.intensity;
        break;
      }
    }
    isAltDragging = true;
    bindingModified = false;
  }

  void applyIntensity(float intensity) {
    auto &bindings = macroParamRef->bindings;
    auto it =
        std::find_if(bindings.begin(), bindings.end(),
                     [this](const MacroBinding &b) {
                       return b.macroIndex == altDragMacroIndex;
                     });
    if (it == bindings.end()) {
      bindings.push_back({altDragMacroIndex, intensity});
    } else {
      it->intensity = intensity;
    }

    bindingModified = true;
    if (onMappingChanged) {
      onMappingChanged();
    }
    // Repaint this slider and the parent NodeBlock (for the intensity arc overlay)
    repaint();
    if (auto *p = getParentComponent()) {
      p->repaint();
    }
  }
};

class CustomMacroButton : public juce::TextButton {
 public:
  std::function<void()> onRightClick;

  // Wired by NodeBlock to enable alt+click intensity binding
  int *selectedMacroPtr = nullptr;
  MacroParam *macroParamRef = nullptr;
  std::function<int()> getNextFreeMacro;
  std::function<void(std::vector<int>)> onHoverMacros;
  std::function<void()> onMappingChanged;
  std::function<void()> onBindingChanged;
  std::function<void(MacroParam *)> onRequestMidiLearn;

  void mouseEnter(const juce::MouseEvent &e) override {
    juce::TextButton::mouseEnter(e);
    if (onHoverMacros && macroParamRef != nullptr) {
      std::vector<int> indices;
      indices.reserve(macroParamRef->bindings.size());
      for (const auto &b : macroParamRef->bindings) {
        indices.push_back(b.macroIndex);
      }
      onHoverMacros(std::move(indices));
    }
  }

  void mouseExit(const juce::MouseEvent &e) override {
    juce::TextButton::mouseExit(e);
    if (onHoverMacros) {
      onHoverMacros({});
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick) {
        onRightClick();
      }
      return;
    }
    if (e.mods.isAltDown() && macroParamRef != nullptr) {
      if (e.getNumberOfClicks() >= 2 && onRequestMidiLearn) {
        onRequestMidiLearn(macroParamRef);
        return;
      }
      handleAltClick();
      return;
    }
    juce::TextButton::mouseDown(e);
  }

 private:
  void handleAltClick() {
    int macroIdx = (selectedMacroPtr != nullptr) ? *selectedMacroPtr : -1;
    if (macroIdx == -1 && getNextFreeMacro) {
      macroIdx = getNextFreeMacro();
    }
    if (macroIdx < 0 || macroParamRef == nullptr) {
      return;
    }

    auto &bindings = macroParamRef->bindings;
    auto it =
        std::find_if(bindings.begin(), bindings.end(),
                     [macroIdx](const MacroBinding &b) {
                       return b.macroIndex == macroIdx;
                     });
    if (it == bindings.end()) {
      bindings.push_back({macroIdx, 1.0f});
    } else {
      // Toggle: flip between +1 and removed
      bindings.erase(it);
    }

    if (onMappingChanged) {
      onMappingChanged();
    }
    if (onBindingChanged) {
      juce::MessageManager::callAsync([cb = onBindingChanged]() { cb(); });
    }
  }
};

class CustomMacroComboBox : public juce::ComboBox {
 public:
  std::function<void()> onRightClick;

  // Wired by NodeBlock to enable alt+click intensity binding
  int *selectedMacroPtr = nullptr;
  MacroParam *macroParamRef = nullptr;
  std::function<int()> getNextFreeMacro;
  std::function<void(std::vector<int>)> onHoverMacros;
  std::function<void()> onMappingChanged;
  std::function<void()> onBindingChanged;
  std::function<void(MacroParam *)> onRequestMidiLearn;

  void mouseEnter(const juce::MouseEvent &e) override {
    juce::ComboBox::mouseEnter(e);
    if (onHoverMacros && macroParamRef != nullptr) {
      std::vector<int> indices;
      indices.reserve(macroParamRef->bindings.size());
      for (const auto &b : macroParamRef->bindings) {
        indices.push_back(b.macroIndex);
      }
      onHoverMacros(std::move(indices));
    }
  }

  void mouseExit(const juce::MouseEvent &e) override {
    juce::ComboBox::mouseExit(e);
    if (onHoverMacros) {
      onHoverMacros({});
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick) {
        onRightClick();
      }
      return;
    }
    if (e.mods.isAltDown() && macroParamRef != nullptr) {
      if (e.getNumberOfClicks() >= 2 && onRequestMidiLearn) {
        onRequestMidiLearn(macroParamRef);
        return;
      }
      handleAltClick();
      return;
    }
    juce::ComboBox::mouseDown(e);
  }

 private:
  void handleAltClick() {
    if (macroParamRef == nullptr) {
      return;
    }

    auto &bindings = macroParamRef->bindings;
    int macroIdx = (selectedMacroPtr != nullptr) ? *selectedMacroPtr : -1;

    if (macroIdx == -1) {
      // No macro selected: clear all bindings if any exist, else grab a free one
      if (!bindings.empty()) {
        bindings.clear();
      } else {
        if (getNextFreeMacro) {
          macroIdx = getNextFreeMacro();
        }
        if (macroIdx >= 0) {
          bindings.push_back({macroIdx, 1.0f});
        }
      }
    } else {
      auto it = std::find_if(bindings.begin(), bindings.end(),
                             [macroIdx](const MacroBinding &b) {
                               return b.macroIndex == macroIdx;
                             });
      if (it == bindings.end()) {
        bindings.push_back({macroIdx, 1.0f});
      } else {
        bindings.erase(it);
      }
    }

    if (onMappingChanged) {
      onMappingChanged();
    }
    if (onBindingChanged) {
      juce::MessageManager::callAsync([cb = onBindingChanged]() { cb(); });
    }
  }
};

class MacroAttachment : public juce::AudioProcessorValueTreeState::Listener,
                        public juce::Slider::Listener,
                        private juce::AsyncUpdater {
 public:
  MacroAttachment(juce::AudioProcessorValueTreeState &s,
                  const juce::String &pID, juce::Slider &sl)
      : state(s), paramID(pID), slider(sl) {
    state.addParameterListener(paramID, this);
    slider.addListener(this);
    parameterChanged(paramID, *state.getRawParameterValue(paramID));
  }
  ~MacroAttachment() override {
    cancelPendingUpdate();
    state.removeParameterListener(paramID, this);
    slider.removeListener(this);
  }
  void parameterChanged(const juce::String &, float newValue) override {
    targetValue = newValue;
    triggerAsyncUpdate();
  }
  void handleAsyncUpdate() override {
    juce::ScopedValueSetter<bool> svs(isUpdating, true);
    double val = slider.getMinimum() +
                 targetValue * (slider.getMaximum() - slider.getMinimum());
    slider.setValue(val, juce::sendNotificationSync);
  }
  void sliderValueChanged(juce::Slider *) override {
    if (!isUpdating) {
      float norm = (float)((slider.getValue() - slider.getMinimum()) /
                           (slider.getMaximum() - slider.getMinimum()));
      if (auto *param = state.getParameter(paramID))
        param->setValueNotifyingHost(norm);
    }
  }

 private:
  juce::AudioProcessorValueTreeState &state;
  juce::String paramID;
  juce::Slider &slider;
  bool isUpdating = false;
  std::atomic<float> targetValue{0.0f};
};

class ButtonAttachment : public juce::AudioProcessorValueTreeState::Listener,
                         private juce::Timer {
 public:
  ButtonAttachment(juce::AudioProcessorValueTreeState &s,
                   const juce::String &pID, juce::Button &b)
      : state(s), paramID(pID), button(b) {
    state.addParameterListener(paramID, this);
    startTimerHz(30);
    parameterChanged(paramID, *state.getRawParameterValue(paramID));
  }
  ~ButtonAttachment() override {
    state.removeParameterListener(paramID, this);
    stopTimer();
  }
  void parameterChanged(const juce::String &, float newValue) override {
    targetValue = newValue >= 0.5f;
  }

  void timerCallback() override {
    if (button.getToggleState() != targetValue) {
      button.setToggleState(targetValue, juce::dontSendNotification);
    }
  }

 private:
  juce::AudioProcessorValueTreeState &state;
  juce::String paramID;
  juce::Button &button;
  std::atomic<bool> targetValue{false};
};
