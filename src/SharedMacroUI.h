#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GraphNode.h"

class CustomMacroSlider : public juce::Slider {
 public:
  std::function<void()> onRightClick;

  // Wired by NodeBlock to enable shift+drag intensity binding
  int *selectedMacroPtr = nullptr;
  MacroParam *macroParamRef = nullptr;
  std::function<int()> getNextFreeMacro;          // → free macro index or -1
  std::function<void(int)> onAutoSelectMacro;     // notify editor of auto-selection
  std::function<void()> onMappingChanged;          // update DAW names + repaint
  std::function<void()> onBindingChanged;          // deferred rebuild

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick)
        onRightClick();
      return;
    }
    if (e.mods.isShiftDown() && macroParamRef != nullptr) {
      startShiftDrag();
      return;  // suppress normal slider drag while shift is held
    }
    juce::Slider::mouseDown(e);
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (isShiftDragging) {
      if (shiftDragMacroIndex < 0)
        return;
      float dy = static_cast<float>(e.getDistanceFromDragStartY());
      float newIntensity =
          juce::jlimit(-2.0f, 2.0f, shiftDragStartIntensity - dy / 150.0f);
      applyIntensity(newIntensity);
      return;
    }
    juce::Slider::mouseDrag(e);
  }

  void mouseUp(const juce::MouseEvent &e) override {
    if (isShiftDragging) {
      isShiftDragging = false;
      shiftDragMacroIndex = -1;
      if (bindingModified && onBindingChanged) {
        // Defer so we don't destroy this slider from within its own mouseUp
        juce::MessageManager::callAsync([cb = onBindingChanged]() { cb(); });
      }
      return;
    }
    if (e.mods.isPopupMenu())
      return;
    juce::Slider::mouseUp(e);
  }

 private:
  bool isShiftDragging = false;
  int shiftDragMacroIndex = -1;
  float shiftDragStartIntensity = 0.0f;
  bool bindingModified = false;

  void startShiftDrag() {
    int macroIdx = (selectedMacroPtr != nullptr) ? *selectedMacroPtr : -1;
    if (macroIdx == -1) {
      if (getNextFreeMacro)
        macroIdx = getNextFreeMacro();
      if (macroIdx != -1 && onAutoSelectMacro)
        onAutoSelectMacro(macroIdx);
    }
    if (macroIdx < 0)
      return;

    shiftDragMacroIndex = macroIdx;
    shiftDragStartIntensity = 0.0f;
    for (const auto &b : macroParamRef->bindings) {
      if (b.macroIndex == macroIdx) {
        shiftDragStartIntensity = b.intensity;
        break;
      }
    }
    isShiftDragging = true;
    bindingModified = false;
  }

  void applyIntensity(float intensity) {
    auto &bindings = macroParamRef->bindings;
    auto it =
        std::find_if(bindings.begin(), bindings.end(),
                     [this](const MacroBinding &b) {
                       return b.macroIndex == shiftDragMacroIndex;
                     });
    if (it == bindings.end())
      bindings.push_back({shiftDragMacroIndex, intensity});
    else
      it->intensity = intensity;

    bindingModified = true;
    if (onMappingChanged)
      onMappingChanged();
    // Repaint this slider and the parent NodeBlock (for the intensity arc overlay)
    repaint();
    if (auto *p = getParentComponent())
      p->repaint();
  }
};

class CustomMacroButton : public juce::TextButton {
 public:
  std::function<void()> onRightClick;

  // Wired by NodeBlock to enable shift+click intensity binding
  int *selectedMacroPtr = nullptr;
  MacroParam *macroParamRef = nullptr;
  std::function<int()> getNextFreeMacro;
  std::function<void(int)> onAutoSelectMacro;
  std::function<void()> onMappingChanged;
  std::function<void()> onBindingChanged;

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick)
        onRightClick();
      return;
    }
    if (e.mods.isShiftDown() && macroParamRef != nullptr) {
      handleShiftClick();
      return;
    }
    juce::TextButton::mouseDown(e);
  }

 private:
  void handleShiftClick() {
    int macroIdx = (selectedMacroPtr != nullptr) ? *selectedMacroPtr : -1;
    if (macroIdx == -1) {
      if (getNextFreeMacro)
        macroIdx = getNextFreeMacro();
      if (macroIdx != -1 && onAutoSelectMacro)
        onAutoSelectMacro(macroIdx);
    }
    if (macroIdx < 0 || macroParamRef == nullptr)
      return;

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

    if (onMappingChanged)
      onMappingChanged();
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
