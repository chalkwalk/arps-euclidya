#include "../LayoutParser.h"
#include "../SharedMacroUI.h"
#include "BinaryData.h"
#include "EuclideanVisualizer.h"
#include "MidiOutNode.h"

// ---------------------------------------------------------------------------
// CC Registry Panel
// ---------------------------------------------------------------------------

class CCRegistryPanel : public juce::Component, private juce::Timer {
 public:
  explicit CCRegistryPanel(MidiOutNode &node) : midiOutNode(node) {
    startTimerHz(10);
  }

  ~CCRegistryPanel() override { stopTimer(); }

  void timerCallback() override { repaint(); }

  void paint(juce::Graphics &g) override {
    auto &lanes = midiOutNode.ccLanes;
    auto b = getLocalBounds();

    g.setColour(juce::Colour(0xff111111));
    g.fillRect(b);

    if (lanes.empty()) {
      g.setColour(juce::Colours::darkgrey);
      g.setFont(10.0f);
      g.drawText("Connect a CC Modulator to port 1", b,
                 juce::Justification::centred);
      return;
    }

    const int rowH = 18;
    const int padX = 4;
    // Column widths (pixels inside padded area)
    const int colCC = 28;     // "CC##"
    const int colName = 60;   // name label
    const int colAnchor = 40; // anchor knob area
    const int colHold = 40;   // Hold/Reset button area
    const int colSlew = 40;   // slew area
    const int colVal = 32;    // current value bar

    g.setFont(9.5f);

    // Header row
    int hx = padX;
    g.setColour(juce::Colour(0xff888888));
    g.drawText("CC#", hx, 0, colCC, rowH, juce::Justification::centredLeft);
    hx += colCC;
    g.drawText("Name", hx, 0, colName, rowH, juce::Justification::centredLeft);
    hx += colName;
    g.drawText("Anchor", hx, 0, colAnchor, rowH,
               juce::Justification::centredLeft);
    hx += colAnchor;
    g.drawText("Rest", hx, 0, colHold, rowH, juce::Justification::centredLeft);
    hx += colHold;
    g.drawText("Slew", hx, 0, colSlew, rowH, juce::Justification::centredLeft);
    hx += colSlew;
    g.drawText("Value", hx, 0, colVal, rowH, juce::Justification::centredLeft);

    for (int i = 0; i < (int)lanes.size(); ++i) {
      const auto &lane = lanes[(size_t)i];
      int y = (i + 1) * rowH;
      if (y + rowH > b.getHeight()) break;

      // Alternating row background
      if (i % 2 == 0) {
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(0, y, b.getWidth(), rowH);
      }

      int rx = padX;
      g.setColour(juce::Colours::white);

      // CC# — with CC74 warning tint
      if (lane.ccNumber == 74) {
        g.setColour(juce::Colour(0xffffaa44));
      }
      g.drawText(juce::String(lane.ccNumber), rx, y, colCC, rowH,
                 juce::Justification::centredLeft);
      g.setColour(juce::Colours::white);
      rx += colCC;

      // Name
      g.drawText(lane.name, rx, y, colName, rowH,
                 juce::Justification::centredLeft);
      rx += colName;

      // Anchor value bar (draggable — handled in mouse callbacks)
      {
        auto ab = juce::Rectangle<int>(rx, y + 3, colAnchor - 4, rowH - 6);
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(ab);
        float anchorFrac = std::clamp(lane.anchorValue, 0.0f, 1.0f);
        g.setColour(juce::Colour(0xff557799));
        g.fillRect(ab.withWidth(juce::roundToInt(ab.getWidth() * anchorFrac)));
        g.setColour(juce::Colour(0xff888888));
        g.drawRect(ab);
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(juce::roundToInt(anchorFrac * 127.0f)), ab,
                   juce::Justification::centred);
      }
      rx += colAnchor;

      // Hold/Reset toggle
      {
        auto tb =
            juce::Rectangle<int>(rx + 2, y + 3, colHold - 6, rowH - 6);
        g.setColour(lane.holdOnRest ? juce::Colour(0xff224422)
                                    : juce::Colour(0xff442222));
        g.fillRoundedRectangle(tb.toFloat(), 2.0f);
        g.setColour(juce::Colours::white);
        g.drawText(lane.holdOnRest ? "Hold" : "Reset", tb,
                   juce::Justification::centred);
      }
      rx += colHold;

      // Slew bar
      {
        auto sb = juce::Rectangle<int>(rx, y + 3, colSlew - 4, rowH - 6);
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(sb);
        float slewFrac = std::clamp(lane.slewAmount, 0.0f, 1.0f);
        g.setColour(juce::Colour(0xff664488));
        g.fillRect(sb.withWidth(juce::roundToInt(sb.getWidth() * slewFrac)));
        g.setColour(juce::Colour(0xff888888));
        g.drawRect(sb);
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(slewFrac, 2), sb, juce::Justification::centred);
      }
      rx += colSlew;

      // Current value bar
      {
        auto vb = juce::Rectangle<int>(rx, y + 3, colVal - 4, rowH - 6);
        g.setColour(juce::Colour(0xff222222));
        g.fillRect(vb);
        float curFrac = std::clamp(lane.currentValue, 0.0f, 1.0f);
        g.setColour(juce::Colour(0xffaa44ff).withAlpha(0.7f));
        g.fillRect(vb.withWidth(juce::roundToInt(vb.getWidth() * curFrac)));
        g.setColour(juce::Colour(0xff888888));
        g.drawRect(vb);
      }
    }

    // CC74 warning note (if any lane has CC74)
    for (const auto &lane : lanes) {
      if (lane.ccNumber == 74) {
        g.setColour(juce::Colour(0xffffaa44));
        g.setFont(8.5f);
        g.drawText("* CC 74 may conflict with MPE Timbre",
                   padX,
                   (int)(lanes.size() + 1) * rowH,
                   b.getWidth() - padX * 2,
                   rowH,
                   juce::Justification::centredLeft);
        break;
      }
    }
  }

  void mouseDown(const juce::MouseEvent &e) override {
    auto &lanes = midiOutNode.ccLanes;
    const int rowH = 18;
    const int padX = 4;
    const int colCC = 28;
    const int colName = 60;
    const int colAnchor = 40;
    const int colHold = 40;
    const int colSlew = 40;

    int row = (e.y / rowH) - 1;
    if (row < 0 || row >= (int)lanes.size()) return;

    auto &lane = lanes[(size_t)row];
    int y = (row + 1) * rowH;
    int rx = padX + colCC + colName;

    // Anchor bar — also draggable, handle on mouseDown too
    {
      auto ab = juce::Rectangle<int>(rx, y + 3, colAnchor - 4, rowH - 6);
      if (ab.contains(e.x, e.y)) {
        float v = (float)(e.x - ab.getX()) / (float)ab.getWidth();
        lane.anchorValue = std::clamp(v, 0.0f, 1.0f);
        repaint();
        return;
      }
    }
    rx += colAnchor;

    // Hold/Reset toggle — only on mouseDown (not drag)
    {
      auto tb = juce::Rectangle<int>(rx + 2, y + 3, colHold - 6, rowH - 6);
      if (tb.contains(e.x, e.y)) {
        lane.holdOnRest = !lane.holdOnRest;
        repaint();
        return;
      }
    }
    rx += colHold;

    // Slew bar — also draggable, handle on mouseDown too
    {
      auto sb = juce::Rectangle<int>(rx, y + 3, colSlew - 4, rowH - 6);
      if (sb.contains(e.x, e.y)) {
        float v = (float)(e.x - sb.getX()) / (float)sb.getWidth();
        lane.slewAmount = std::clamp(v, 0.0f, 1.0f);
        repaint();
        return;
      }
    }
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    auto &lanes = midiOutNode.ccLanes;
    const int rowH = 18;
    const int padX = 4;
    const int colCC = 28;
    const int colName = 60;
    const int colAnchor = 40;
    const int colHold = 40;
    const int colSlew = 40;

    int row = (e.y / rowH) - 1;
    if (row < 0 || row >= (int)lanes.size()) return;

    auto &lane = lanes[(size_t)row];
    int y = (row + 1) * rowH;
    int rx = padX + colCC + colName;

    // Anchor bar drag
    {
      auto ab = juce::Rectangle<int>(rx, y + 3, colAnchor - 4, rowH - 6);
      if (ab.contains(e.getMouseDownX(), e.getMouseDownY())) {
        float v = (float)(e.x - ab.getX()) / (float)ab.getWidth();
        lane.anchorValue = std::clamp(v, 0.0f, 1.0f);
        repaint();
        return;
      }
    }
    rx += colAnchor + colHold;  // skip Hold column — never draggable

    // Slew bar drag
    {
      auto sb = juce::Rectangle<int>(rx, y + 3, colSlew - 4, rowH - 6);
      if (sb.contains(e.getMouseDownX(), e.getMouseDownY())) {
        float v = (float)(e.x - sb.getX()) / (float)sb.getWidth();
        lane.slewAmount = std::clamp(v, 0.0f, 1.0f);
        repaint();
        return;
      }
    }
  }

  MidiOutNode &midiOutNode;
};

// --- Visualizer Component (the only custom component for MidiOutNode) ---
class MidiOutVisualizer : public juce::Component, private juce::Timer {
 public:
  MidiOutVisualizer(MidiOutNode &node) : midiOutNode(node) {
    addAndMakeVisible(visualizer);
    startTimerHz(30);
  }

  ~MidiOutVisualizer() override { stopTimer(); }

  void timerCallback() override {
    visualizer.update(midiOutNode.getPattern(), midiOutNode.getPatternIndex(),
                      midiOutNode.getRhythm(), midiOutNode.getRhythmIndex(),
                      midiOutNode.lastTickPlayedNote);
  }

  void resized() override { visualizer.setBounds(getLocalBounds()); }

 private:
  MidiOutNode &midiOutNode;
  EuclideanVisualizer visualizer;
};

// --- getLayout: load from JSON and bind all runtime pointers ---
NodeLayout MidiOutNode::getLayout() const {
  auto layout = LayoutParser::parseFromJSON(BinaryData::MidiOutNode_json,
                                            BinaryData::MidiOutNode_jsonSize);

  // Cast away const for pointer binding (NodeBlock reads/writes these)
  auto *self = const_cast<MidiOutNode *>(this);

  auto bindElements = [self](std::vector<UIElement> &elements) {
    for (auto &el : elements) {
      if (el.label == "pSteps") {
        el.valueRef = &self->pSteps;
        el.macroParamRef = &self->macroPSteps;
      } else if (el.label == "pBeats") {
        el.valueRef = &self->pBeats;
        el.macroParamRef = &self->macroPBeats;
        el.dynamicMinRef = &self->ui_pBeatsMin;
        el.dynamicMaxRef = &self->ui_pBeatsMax;
      } else if (el.label == "pOffset") {
        el.valueRef = &self->pOffset;
        el.macroParamRef = &self->macroPOffset;
        el.dynamicMinRef = &self->ui_pOffsetMin;
        el.dynamicMaxRef = &self->ui_pOffsetMax;
      } else if (el.label == "rSteps") {
        el.valueRef = &self->rSteps;
        el.macroParamRef = &self->macroRSteps;
      } else if (el.label == "rBeats") {
        el.valueRef = &self->rBeats;
        el.macroParamRef = &self->macroRBeats;
        el.dynamicMinRef = &self->ui_rBeatsMin;
        el.dynamicMaxRef = &self->ui_rBeatsMax;
      } else if (el.label == "rOffset") {
        el.valueRef = &self->rOffset;
        el.macroParamRef = &self->macroROffset;
        el.dynamicMinRef = &self->ui_rOffsetMin;
        el.dynamicMaxRef = &self->ui_rOffsetMax;
      } else if (el.label == "pressureToVelocity") {
        el.floatValueRef = &self->pressureToVelocity;
        el.macroParamRef = &self->macroPressureToVelocity;
      } else if (el.label == "timbreToVelocity") {
        el.floatValueRef = &self->timbreToVelocity;
        el.macroParamRef = &self->macroTimbreToVelocity;
      } else if (el.label == "clockDivisionIndex") {
        el.valueRef = &self->clockDivisionIndex;
        el.macroParamRef = &self->macroClockDivisionIndex;
      } else if (el.label == "syncMode") {
        el.valueRef = &self->ui_syncMode;
        el.macroParamRef = &self->macroSyncModeParam;
      } else if (el.label == "patternMode") {
        el.valueRef = &self->ui_patternMode;
        el.macroParamRef = &self->macroPatternModeParam;
      } else if (el.label == "triplet") {
        el.valueRef = &self->ui_triplet;
      } else if (el.label == "patternResetOnRelease") {
        el.valueRef = &self->ui_patternResetOnRelease;
      } else if (el.label == "rhythmResetOnRelease") {
        el.valueRef = &self->ui_rhythmResetOnRelease;
      } else if (el.label == "outputChannel") {
        el.valueRef = &self->outputChannel;
      } else if (el.label == "channelMode") {
        el.valueRef = &self->ui_channelMode;
        el.macroParamRef = &self->macroChannelMode;
      } else if (el.label == "passExpressions") {
        el.valueRef = &self->ui_passExpressions;
      } else if (el.label == "humTiming") {
        el.floatValueRef = &self->humTiming;
        el.macroParamRef = &self->macroHumTiming;
      } else if (el.label == "humVelocity") {
        el.floatValueRef = &self->humVelocity;
        el.macroParamRef = &self->macroHumVelocity;
      } else if (el.label == "humGate") {
        el.floatValueRef = &self->humGate;
        el.macroParamRef = &self->macroHumGate;
      } else if (el.label == "gatePercent") {
        el.floatValueRef = &self->gatePercent;
        el.macroParamRef = &self->macroGatePercent;
      } else if (el.label == "flexGate") {
        el.valueRef = &self->ui_flexGate;
      }
    }
  };

  bindElements(layout.elements);
  bindElements(layout.unfoldedElements);

  return layout;
}

std::unique_ptr<juce::Component> MidiOutNode::createCustomComponent(
    const juce::String &name, juce::AudioProcessorValueTreeState *apvts) {
  juce::ignoreUnused(apvts);
  if (name == "Visualizer") {
    return std::make_unique<MidiOutVisualizer>(*this);
  }
  if (name == "CCRegistry") {
    return std::make_unique<CCRegistryPanel>(*this);
  }
  return nullptr;
}
