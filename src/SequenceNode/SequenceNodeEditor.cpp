#include <juce_gui_basics/juce_gui_basics.h>

#include "../GraphCanvas.h"
#include "../SharedMacroUI.h"
#include "SequenceNode.h"

class SequenceNodeEditor : public juce::Component,
                           public juce::ScrollBar::Listener {
 public:
  SequenceNodeEditor(SequenceNode &node,
                     juce::AudioProcessorValueTreeState & /*apvts*/)
      : seqNode(node) {
    lengthSlider.setRange(1, 16, 1);
    lengthSlider.setValue(node.seqLength, juce::dontSendNotification);
    lengthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    lengthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    lengthSlider.onValueChange = [this]() {
      seqNode.seqLength = (int)lengthSlider.getValue();
      if (seqNode.onNodeDirtied) {
        seqNode.onNodeDirtied();
      }
      repaint();
    };
    addAndMakeVisible(lengthSlider);

    lengthLabel.setText("Length", juce::dontSendNotification);
    lengthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lengthLabel);

    lengthSlider.onRightClick = [this, &node]() {
      GraphCanvas *canvasPtr = findParentComponentOfClass<GraphCanvas>();
      if (canvasPtr == nullptr) {
        return;
      }

      juce::PopupMenu menu;
      if (node.macroSeqLength.bindings.empty()) {
        menu.addItem(1, "No macro bindings", false, false);
      } else {
        for (const auto &b : node.macroSeqLength.bindings) {
          menu.addItem(b.macroIndex + 2,
                       "Remove: Macro " + juce::String(b.macroIndex + 1));
        }
      }
      auto options = juce::PopupMenu::Options().withTargetScreenArea(
          lengthSlider.getScreenBounds());
      menu.showMenuAsync(options, [&node, canvasPtr](int result) {
        if (result < 2) {
          return;
        }
        int macroIndex = result - 2;
        canvasPtr->performMutation([&node, macroIndex]() {
          auto &bindings = node.macroSeqLength.bindings;
          bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                        [macroIndex](const MacroBinding &b) {
                                          return b.macroIndex == macroIndex;
                                        }),
                         bindings.end());
          node.parameterChanged();
          if (node.onMappingChanged) {
            node.onMappingChanged();
          }
        });
        canvasPtr->rebuild();
      });
    };

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

    auto labelArea = juce::Rectangle<int>(gridArea.getX() - 35, gridArea.getY(),
                                          33, gridArea.getHeight());

    for (int i = 0; i < visibleRows; ++i) {
      int noteNum = 127 - scrollOffset - i;
      if (noteNum < 0 || noteNum > 127) {
        continue;
      }

      auto rowLabel =
          labelArea.withY(gridArea.getY() + (i * cellH)).withHeight(cellH);

      bool isC = (noteNum % 12) == 0;
      g.setColour(isC ? juce::Colours::white
                      : juce::Colours::white.withAlpha(0.55f));
      g.setFont(isC ? 10.0f : 9.0f);
      g.drawText(noteName(noteNum), rowLabel, juce::Justification::centredRight,
                 true);
    }

    g.setColour(juce::Colour(0xff222222));
    g.fillRect(gridArea);

    for (int i = 0; i < visibleRows; ++i) {
      int noteNum = 127 - scrollOffset - i;
      if (noteNum < 0 || noteNum > 127) {
        continue;
      }

      bool isC = (noteNum % 12) == 0;

      for (int c = 0; c < 16; ++c) {
        juce::Rectangle<int> cell(gridArea.getX() + (c * cellW),
                                  gridArea.getY() + (i * cellH), cellW, cellH);

        if (c >= seqNode.seqLength) {
          g.setColour(juce::Colours::black.withAlpha(0.5f));
          g.fillRect(cell.reduced(1));
        } else if (seqNode.grid[noteNum][c]) {
          g.setColour(juce::Colours::orange);
          g.fillRect(cell.reduced(1));
        } else {
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
    if (!b.contains(e.getPosition())) {
      return;
    }

    int cellW = b.getWidth() / 16;
    int cellH = b.getHeight() / visibleRows;
    int c = (e.x - b.getX()) / cellW;
    int i = (e.y - b.getY()) / cellH;

    int noteNum = 127 - scrollOffset - i;

    if (noteNum >= 0 && noteNum <= 127 && c >= 0 && c < 16) {
      GraphCanvas *canvasPtr = findParentComponentOfClass<GraphCanvas>();
      if (canvasPtr != nullptr) {
        bool isClearing = e.mods.isRightButtonDown();
        bool newState = !isClearing;

        if (seqNode.grid[noteNum][c] != newState) {
          canvasPtr->performMutation([this, noteNum, c, newState]() {
            seqNode.grid[noteNum][c] = newState;
            if (seqNode.onNodeDirtied) {
              seqNode.onNodeDirtied();
            }
          });
          repaint();
        }
      } else {
        if (e.mods.isLeftButtonDown()) {
          seqNode.grid[noteNum][c] = true;
        } else if (e.mods.isRightButtonDown()) {
          seqNode.grid[noteNum][c] = false;
        }
        if (seqNode.onNodeDirtied) {
          seqNode.onNodeDirtied();
        }
        repaint();
      }
    }
  }

  void resized() override {
    auto b = getLocalBounds();
    auto leftBlock = b.removeFromLeft(70);
    lengthLabel.setBounds(leftBlock.removeFromBottom(18));
    lengthSlider.setBounds(leftBlock.withSizeKeepingCentre(55, 55));

    scrollbar.setBounds(b.removeFromRight(14));
  }

  [[nodiscard]] juce::Rectangle<int> getGridBounds() const {
    return getLocalBounds().withTrimmedLeft(105).withTrimmedRight(16).reduced(
        4);
  }

 private:
  void scrollBarMoved(juce::ScrollBar * /*scrollBarThatHasMoved*/,
                      double newRange) override {
    scrollOffset = (int)newRange;
    repaint();
  }

  SequenceNode &seqNode;
  CustomMacroSlider lengthSlider;
  juce::Label lengthLabel;

  static constexpr int visibleRows = 13;
  int scrollOffset = 128 - 13 - 60;

  juce::ScrollBar scrollbar{true};
};

class SequenceNodeCustomComponent : public SequenceNodeEditor {
 public:
  SequenceNodeCustomComponent(SequenceNode &node,
                              juce::AudioProcessorValueTreeState &apvts)
      : SequenceNodeEditor(node, apvts) {}
};

std::unique_ptr<juce::Component> SequenceNode::createCustomComponent(
    const juce::String &name, juce::AudioProcessorValueTreeState *apvts) {
  juce::ignoreUnused(name);
  if (apvts != nullptr) {
    return std::make_unique<SequenceNodeCustomComponent>(*this, *apvts);
  }
  return nullptr;
}
