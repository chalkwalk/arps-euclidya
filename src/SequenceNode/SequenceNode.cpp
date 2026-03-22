#include "SequenceNode.h"
#include "../MacroMappingMenu.h"
#include "../SharedMacroUI.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SequenceNodeEditor : public juce::Component,
                           public juce::ScrollBar::Listener {
public:
  SequenceNodeEditor(SequenceNode &node,
                     juce::AudioProcessorValueTreeState &apvts)
      : seqNode(node) {

    // Length slider
    lengthSlider.setRange(1, 16, 1);
    lengthSlider.setValue(node.seqLength, juce::dontSendNotification);
    lengthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lengthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    lengthSlider.onValueChange = [this]() {
      seqNode.seqLength = (int)lengthSlider.getValue();
      if (seqNode.onNodeDirtied)
        seqNode.onNodeDirtied();
      repaint();
    };
    addAndMakeVisible(lengthSlider);

    lengthLabel.setText("Length", juce::dontSendNotification);
    lengthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lengthLabel);

    lengthSlider.onRightClick = [this, &node, &apvts]() {
      MacroMappingMenu::showMenu(
          &lengthSlider, node.macroSeqLength,
          [this, &node, &apvts](int macroIndex) {
            node.macroSeqLength = macroIndex;
            if (macroIndex == -1) {
              lengthAttachment.reset();
              lengthSlider.setTooltip("");
            } else {
              lengthAttachment = std::make_unique<MacroAttachment>(
                  apvts, "macro_" + juce::String(macroIndex + 1), lengthSlider);
              lengthSlider.setTooltip("Mapped to Macro " +
                                      juce::String(macroIndex + 1));
              if (node.onMappingChanged)
                node.onMappingChanged();
            }
          });
    };

    if (node.macroSeqLength != -1) {
      lengthAttachment = std::make_unique<MacroAttachment>(
          apvts, "macro_" + juce::String(node.macroSeqLength + 1),
          lengthSlider);
    }

    // Vertical scrollbar for note range
    scrollbar.setRangeLimits(0.0, 128.0, juce::dontSendNotification);
    scrollbar.setCurrentRange(scrollOffset, visibleRows,
                              juce::dontSendNotification);
    scrollbar.setAutoHide(false);
    scrollbar.addListener(this);
    addAndMakeVisible(scrollbar);

    setSize(380, 220);
  }

  static juce::String noteName(int noteNumber) {
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    int octave = (noteNumber / 12) - 1;
    return juce::String(names[noteNumber % 12]) + juce::String(octave);
  }

  void paint(juce::Graphics &g) override {
    auto gridArea = getGridBounds();
    int cellH = gridArea.getHeight() / visibleRows;
    int cellW = gridArea.getWidth() / 16;

    // Draw row labels
    auto labelArea = juce::Rectangle<int>(gridArea.getX() - 35, gridArea.getY(),
                                          33, gridArea.getHeight());

    for (int i = 0; i < visibleRows; ++i) {
      // Scroll is inverted: higher offset = lower notes visible
      // Top of grid = highest visible note
      int noteNum = 127 - scrollOffset - i;
      if (noteNum < 0 || noteNum > 127)
        continue;

      auto rowLabel =
          labelArea.withY(gridArea.getY() + i * cellH).withHeight(cellH);

      // Highlight C notes for orientation
      bool isC = (noteNum % 12) == 0;
      g.setColour(isC ? juce::Colours::white
                      : juce::Colours::white.withAlpha(0.55f));
      g.setFont(isC ? 10.0f : 9.0f);
      g.drawText(noteName(noteNum), rowLabel, juce::Justification::centredRight,
                 true);
    }

    // Draw the grid
    g.setColour(juce::Colour(0xff222222));
    g.fillRect(gridArea);

    for (int i = 0; i < visibleRows; ++i) {
      int noteNum = 127 - scrollOffset - i;
      if (noteNum < 0 || noteNum > 127)
        continue;

      bool isC = (noteNum % 12) == 0;

      for (int c = 0; c < 16; ++c) {
        juce::Rectangle<int> cell(gridArea.getX() + c * cellW,
                                  gridArea.getY() + i * cellH, cellW, cellH);

        if (c >= seqNode.seqLength) {
          g.setColour(juce::Colours::black.withAlpha(0.5f));
          g.fillRect(cell.reduced(1));
        } else if (seqNode.grid[noteNum][c]) {
          g.setColour(juce::Colours::orange);
          g.fillRect(cell.reduced(1));
        } else {
          // Slightly brighter background for C notes to aid orientation
          g.setColour(isC ? juce::Colour(0xff3a3a3a)
                          : juce::Colour(0xff2d2d2d));
          g.fillRect(cell.reduced(1));
        }
      }
    }
  }

  void mouseDown(const juce::MouseEvent &e) override { handleGridClick(e); }
  void mouseDrag(const juce::MouseEvent &e) override { handleGridClick(e); }

  void mouseWheelMove(const juce::MouseEvent &e,
                      const juce::MouseWheelDetails &wheel) override {
    auto gridArea = getGridBounds();
    if (gridArea.contains(e.getPosition())) {
      // Scroll up = show higher notes (decrease offset), scroll down = show
      // lower
      int delta = (wheel.deltaY > 0) ? -2 : 2;
      scrollOffset = std::clamp(scrollOffset + delta, 0, 128 - visibleRows);
      scrollbar.setCurrentRange(scrollOffset, visibleRows,
                                juce::dontSendNotification);
      repaint();
    } else {
      Component::mouseWheelMove(e, wheel);
    }
  }

  void handleGridClick(const juce::MouseEvent &e) {
    auto b = getGridBounds();
    if (!b.contains(e.getPosition()))
      return;

    int cellW = b.getWidth() / 16;
    int cellH = b.getHeight() / visibleRows;
    int c = (e.x - b.getX()) / cellW;
    int i = (e.y - b.getY()) / cellH;

    int noteNum = 127 - scrollOffset - i;

    if (noteNum >= 0 && noteNum <= 127 && c >= 0 && c < 16) {
      if (e.mods.isLeftButtonDown()) {
        seqNode.grid[noteNum][c] = true;
      } else if (e.mods.isRightButtonDown()) {
        seqNode.grid[noteNum][c] = false;
      }
      if (seqNode.onNodeDirtied)
        seqNode.onNodeDirtied();
      repaint();
    }
  }

  void resized() override {
    auto b = getLocalBounds();
    auto leftBlock = b.removeFromLeft(70);
    lengthLabel.setBounds(leftBlock.removeFromBottom(18));
    lengthSlider.setBounds(leftBlock.withSizeKeepingCentre(55, 55));

    // Scrollbar on the right edge
    scrollbar.setBounds(b.removeFromRight(14));
  }

  juce::Rectangle<int> getGridBounds() const {
    return getLocalBounds().withTrimmedLeft(105).withTrimmedRight(16).reduced(
        4);
  }

private:
  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *, double newRange) override {
    scrollOffset = (int)newRange;
    repaint();
  }

  SequenceNode &seqNode;
  CustomMacroSlider lengthSlider;
  juce::Label lengthLabel;
  std::unique_ptr<MacroAttachment> lengthAttachment;

  static constexpr int visibleRows = 13; // Just over one octave
  int scrollOffset = 128 - 13 - 60;      // Start around C4 area

  juce::ScrollBar scrollbar{true}; // vertical
};

// Wrap ScrollBar::Listener properly
class SequenceNodeCustomComponent : public SequenceNodeEditor {
public:
  SequenceNodeCustomComponent(SequenceNode &node,
                              juce::AudioProcessorValueTreeState &apvts)
      : SequenceNodeEditor(node, apvts) {}
};

SequenceNode::SequenceNode(std::array<std::atomic<float> *, 32> &m)
    : macros(m) {}

NodeLayout SequenceNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = 4;
  layout.gridHeight = 2;

  UIElement custom;
  custom.type = UIElementType::Custom;
  custom.customType = "Editor";
  custom.gridBounds = {0, 0, 15, 7};
  layout.elements.push_back(custom);

  return layout;
}

std::unique_ptr<juce::Component>
SequenceNode::createCustomComponent(const juce::String &name,
                                    juce::AudioProcessorValueTreeState *apvts) {
  juce::ignoreUnused(name);
  if (apvts != nullptr)
    return std::make_unique<SequenceNodeCustomComponent>(*this, *apvts);
  return nullptr;
}

void SequenceNode::saveNodeState(juce::XmlElement *xml) {
  if (xml) {
    xml->setAttribute("seqLength", seqLength);
    xml->setAttribute("macroSeqLength", macroSeqLength);

    // Compress: only save active cells as "noteNum,step;" pairs
    juce::String activeStr;
    for (int n = 0; n < 128; ++n) {
      for (int c = 0; c < 16; ++c) {
        if (grid[n][c]) {
          activeStr += juce::String(n) + "," + juce::String(c) + ";";
        }
      }
    }
    xml->setAttribute("activeGrid", activeStr);
  }
}

void SequenceNode::loadNodeState(juce::XmlElement *xml) {
  if (xml) {
    seqLength = xml->getIntAttribute("seqLength", 8);
    macroSeqLength = xml->getIntAttribute("macroSeqLength", -1);

    // Clear grid
    for (int n = 0; n < 128; ++n)
      for (int c = 0; c < 16; ++c)
        grid[n][c] = false;

    // Load sparse format
    juce::String activeStr = xml->getStringAttribute("activeGrid");
    if (activeStr.isNotEmpty()) {
      juce::StringArray pairs;
      pairs.addTokens(activeStr, ";", "");
      for (const auto &pair : pairs) {
        if (pair.isEmpty())
          continue;
        int comma = pair.indexOfChar(',');
        if (comma > 0) {
          int n = pair.substring(0, comma).getIntValue();
          int c = pair.substring(comma + 1).getIntValue();
          if (n >= 0 && n < 128 && c >= 0 && c < 16)
            grid[n][c] = true;
        }
      }
    } else {
      // Legacy fallback: 8-row grid string
      juce::String gridStr = xml->getStringAttribute("grid");
      if (gridStr.length() == 128) {
        int baseNote = xml->getIntAttribute("baseNote", 60);
        int idx = 0;
        for (int r = 0; r < 8; ++r) {
          int noteNum = baseNote + (7 - r);
          for (int c = 0; c < 16; ++c) {
            grid[noteNum][c] = (gridStr[idx++] == '1');
          }
        }
      }
    }
  }
}

void SequenceNode::process() {
  int actualLen =
      macroSeqLength != -1 && macros[(size_t)macroSeqLength] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroSeqLength]->load() * 15.0f)
          : seqLength;
  actualLen = std::clamp(actualLen, 1, 16);

  NoteSequence outSeq;

  for (int c = 0; c < actualLen; ++c) {
    std::vector<HeldNote> stepNotes;
    for (int n = 0; n < 128; ++n) {
      if (grid[n][c]) {
        HeldNote hn;
        hn.noteNumber = n;
        hn.channel = 1;
        hn.velocity = 100;
        hn.sourceNoteNumber = n; // For manual steps, the note is its own source
        hn.sourceChannel = 1;
        stepNotes.push_back(hn);
      }
    }
    outSeq.push_back(stepNotes);
  }

  outputSequences[0] = outSeq;

  auto conn = connections.find(0);
  if (conn != connections.end()) {
    for (const auto &connection : conn->second) {
      connection.targetNode->setInputSequence(connection.targetInputPort,
                                              outputSequences[0]);
    }
  }
}
