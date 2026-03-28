#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class CustomMacroSlider : public juce::Slider {
 public:
  std::function<void()> onRightClick;

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick)
        onRightClick();
    } else {
      juce::Slider::mouseDown(e);
    }
  }
};

class CustomMacroButton : public juce::TextButton {
 public:
  std::function<void()> onRightClick;

  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (onRightClick)
        onRightClick();
    } else {
      juce::TextButton::mouseDown(e);
    }
  }
};

class MacroAttachment : public juce::AudioProcessorValueTreeState::Listener,
                        public juce::Slider::Listener {
 public:
  MacroAttachment(juce::AudioProcessorValueTreeState &s,
                  const juce::String &pID, juce::Slider &sl)
      : state(s), paramID(pID), slider(sl) {
    state.addParameterListener(paramID, this);
    slider.addListener(this);
    parameterChanged(paramID, *state.getRawParameterValue(paramID));
  }
  ~MacroAttachment() override {
    state.removeParameterListener(paramID, this);
    slider.removeListener(this);
  }
  void parameterChanged(const juce::String &, float newValue) override {
    juce::MessageManager::callAsync([this, newValue]() {
      juce::ScopedValueSetter<bool> svs(isUpdating, true);
      double val = slider.getMinimum() +
                   newValue * (slider.getMaximum() - slider.getMinimum());
      slider.setValue(val, juce::sendNotificationSync);
    });
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
