#include "PluginEditor.h"

//==============================================================================
ChannelPanel::ChannelPanel(int channelNumber) : channel(channelNumber) {
  setOpaque(true);
}

void ChannelPanel::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(juce::FontOptions(12.0f)));

  float fontHeight = g.getCurrentFont().getHeight();
  float y = 2.0f;
  for (const auto &line : textLines) {
    g.drawFittedText(line, 2, y, getWidth() - 4, fontHeight,
                     juce::Justification::topLeft, 1);
    y += g.getCurrentFont().getHeight();
    if (y > getHeight())
      break;
  }

  g.setColour(juce::Colours::grey);
  g.drawRect(getLocalBounds());
}

void ChannelPanel::addText(const juce::String &text) {
  juce::StringArray newLines;
  newLines.addLines(text);

  for (const auto &line : newLines)
    textLines.add(line);
  while (getTextHeight() > getHeight())
    textLines.remove(0);

  repaint();
}

float ChannelPanel::getTextHeight() const {
  juce::Font font(juce::FontOptions(12.0f));
  float height = 0.0f;
  for (const auto &line : textLines) {
    juce::ignoreUnused(line);
    height += font.getHeight();
  }
  return height;
}

//==============================================================================
EuclideanArpEditor::EuclideanArpEditor(EuclideanArpProcessor &p)
    : AudioProcessorEditor(p), audioProcessor(p) {
  // Panels 1-16 for MIDI channels, Panel 0 for generic console
  for (int currentPanel = 0; currentPanel <= 16; ++currentPanel) {
    ChannelPanel *panel = new ChannelPanel(currentPanel);
    channelPanels.add(panel);
    addAndMakeVisible(panel);
  }
  setSize(800, 600);
  startTimerHz(30);
}

EuclideanArpEditor::~EuclideanArpEditor() { stopTimer(); }

void EuclideanArpEditor::timerCallback() {
  auto &fifo = audioProcessor.midiLogFifo;
  auto &buffer = audioProcessor.midiLogBuffer;

  int numReady = fifo.getNumReady();
  if (numReady > 0) {
    int start1, size1, start2, size2;
    fifo.prepareToRead(numReady, start1, size1, start2, size2);

    auto readEvents = [&](int start, int size) {
      for (int i = 0; i < size; ++i) {
        const auto &event = buffer[(size_t)(start + i)];
        juce::String text;
        if (event.logType == 0)
          text = "Note On: " + juce::String(event.data1) +
                 " V: " + juce::String(event.data2);
        else if (event.logType == 1)
          text = "Note Off: " + juce::String(event.data1);
        else if (event.logType == 2)
          text = "CC " + juce::String(event.data1) + ": " +
                 juce::String((int)event.data2);

        printTextToChannel(event.channel, text);
      }
    };

    if (size1 > 0)
      readEvents(start1, size1);
    if (size2 > 0)
      readEvents(start2, size2);

    fifo.finishedRead(size1 + size2);
  }
}

void EuclideanArpEditor::paint(juce::Graphics &g) { juce::ignoreUnused(g); }

void EuclideanArpEditor::resized() {
  panelWidth = getWidth() / 4.0f;
  panelHeight = getHeight() / 5.0f;

  int currentPanel = 0;

  // Console panel at top spanning full width
  channelPanels[currentPanel++]->setBounds(0, 0, getWidth(), panelHeight);

  // 16 Midi Channel Panels spanning 4x4 grid below console
  for (int row = 1; row < 5; ++row) {
    for (int column = 0; column < 4; ++column) {
      float x = panelWidth * column;
      float y = panelHeight * row;
      channelPanels[currentPanel++]->setBounds(x, y, panelWidth, panelHeight);
    }
  }
}

void EuclideanArpEditor::printTextToChannel(int channel,
                                            const juce::String &text) {
  if (channel >= 1 && channel <= 16) {
    channelPanels[channel]->addText(text);
  }
}

void EuclideanArpEditor::printTextToConsole(const juce::String &text) {
  channelPanels[0]->addText(text);
}
