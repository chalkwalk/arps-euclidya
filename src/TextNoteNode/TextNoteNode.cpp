#include "TextNoteNode.h"

// ---  Custom text editor component ----------------------------------------

class TextNoteCustomComponent : public juce::Component {
 public:
  explicit TextNoteCustomComponent(TextNoteNode& node) : targetNode(node) {
    editor.setMultiLine(true, true);
    editor.setReturnKeyStartsNewLine(true);
    editor.setScrollbarsShown(false);
    editor.setPopupMenuEnabled(true);

    editor.setColour(juce::TextEditor::backgroundColourId,
                     juce::Colours::transparentBlack);
    editor.setColour(juce::TextEditor::outlineColourId,
                     juce::Colours::transparentBlack);
    editor.setColour(juce::TextEditor::focusedOutlineColourId,
                     juce::Colour(0xff0df0e3).withAlpha(0.4f));
    editor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffd4d4d4));

    editor.setFont(juce::Font(juce::FontOptions(13.0f)));
    editor.setText(targetNode.noteText, false);

    editor.onTextChange = [this]() { targetNode.noteText = editor.getText(); };

    addAndMakeVisible(editor);
    setWantsKeyboardFocus(true);
  }

  void resized() override { editor.setBounds(getLocalBounds().reduced(2)); }

  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour(0xff1e1e2e));
  }

 private:
  TextNoteNode& targetNode;
  juce::TextEditor editor;
};

// --- TextNoteNode implementation -------------------------------------------

TextNoteNode::TextNoteNode(int w, int h) : sizeW(w), sizeH(h) {}

std::string TextNoteNode::getName() const {
  if (sizeW == 2 && sizeH == 1) {
    return "Note";
  }
  return "Note Large";
}

NodeLayout TextNoteNode::getLayout() const {
  NodeLayout layout;
  layout.gridWidth = sizeW;
  layout.gridHeight = sizeH;

  UIElement el;
  el.type = UIElementType::Custom;
  el.customType = "TextArea";
  // Fill the full body grid (20 sub-units per grid unit)
  el.gridBounds = {0, 0, sizeW * 20, sizeH * 20};
  layout.elements.push_back(el);

  return layout;
}

void TextNoteNode::saveNodeState(juce::XmlElement* xml) {
  if (xml != nullptr) {
    xml->setAttribute("noteText", noteText);
    xml->setAttribute("sizeW", sizeW);
    xml->setAttribute("sizeH", sizeH);
  }
}

void TextNoteNode::loadNodeState(juce::XmlElement* xml) {
  if (xml != nullptr) {
    noteText = xml->getStringAttribute("noteText", "");
    // Size is baked in at construction from the factory, so we just restore
    // the text. The sizes are informational here.
    sizeW = xml->getIntAttribute("sizeW", sizeW);
    sizeH = xml->getIntAttribute("sizeH", sizeH);
  }
}

std::unique_ptr<juce::Component> TextNoteNode::createCustomComponent(
    const juce::String& name, juce::AudioProcessorValueTreeState* apvts) {
  juce::ignoreUnused(apvts);
  if (name == "TextArea") {
    return std::make_unique<TextNoteCustomComponent>(*this);
  }
  return nullptr;
}
