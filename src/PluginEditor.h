#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Re-using the debug channel panel from haken_editor
class ChannelPanel : public juce::Component {
public:
  ChannelPanel(int channelNumber);
  ~ChannelPanel() override = default;

  void paint(juce::Graphics &g) override;
  void addText(const juce::String &text);

private:
  int channel;
  juce::StringArray textLines;
  float getTextHeight() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelPanel)
};

class EuclideanArpEditor : public juce::AudioProcessorEditor,
                           private juce::Timer {
public:
  EuclideanArpEditor(EuclideanArpProcessor &);
  ~EuclideanArpEditor() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  void timerCallback() override;

  void printTextToChannel(int channel, const juce::String &text);
  void printTextToConsole(const juce::String &text);

private:
  EuclideanArpProcessor &audioProcessor;

  juce::OwnedArray<ChannelPanel> channelPanels;
  float panelWidth;
  float panelHeight;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuclideanArpEditor)
};
