#include "NodeBlock.h"

#include <utility>

#include "ArpsLookAndFeel.h"
#include "GraphCanvas.h"
#include "GraphEngine.h"
#include "MacroColours.h"
#include "MacroMappingMenu.h"
#include "MacroParameter.h"
#include "NodeFactory.h"
#include "SharedMacroUI.h"

NodeBlock::NodeBlock(const std::shared_ptr<GraphNode> &node,
                     juce::AudioProcessorValueTreeState &apvts,
                     GraphCanvas &canvas)
    : targetNode(node), parentCanvas(canvas) {
  titleLabel.setText(node->getName(), juce::dontSendNotification);
  titleLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  titleLabel.setInterceptsMouseClicks(false, false);
  addAndMakeVisible(titleLabel);

  deleteButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colours::transparentBlack);
  deleteButton.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colours::transparentBlack);
  deleteButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colours::white);
  deleteButton.setColour(juce::TextButton::textColourOnId, juce::Colours::red);
  addAndMakeVisible(deleteButton);
  deleteButton.onClick = [this] {
    // Defer deletion: rebuild() destroys nodeBlocks (including this NodeBlock)
    // so we must not do it while this button's click handler is on the stack.
    juce::MessageManager::callAsync([cb = onDelete]() {
      if (cb) {
        cb();
      }
    });
  };

  bypassButton.setClickingTogglesState(true);
  bypassButton.setToggleState(targetNode->bypassed, juce::dontSendNotification);
  bypassButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colours::transparentBlack);
  bypassButton.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colours::orange.withAlpha(0.5f));
  bypassButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colours::white.withAlpha(0.5f));
  bypassButton.setColour(juce::TextButton::textColourOnId,
                         juce::Colours::white);
  addAndMakeVisible(bypassButton);
  bypassButton.onClick = [this] {
    targetNode->bypassed = bypassButton.getToggleState();
    if (targetNode->onNodeDirtied) {
      targetNode->onNodeDirtied();
    }
    repaint();
  };

  auto createComponents = [this, &apvts](
                              const std::vector<UIElement> &elements,
                              juce::OwnedArray<juce::Component> &components,
                              std::vector<std::unique_ptr<
                                  juce::AudioProcessorValueTreeState::Listener>>
                                  &attachments) {
    for (const auto &element : elements) {
      juce::Component *comp = nullptr;

      if (element.type == UIElementType::RotarySlider) {
        auto *slider = new CustomMacroSlider();
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        if (element.bipolar) {
          slider->getProperties().set("bipolar", true);
        }

        if (element.floatValueRef != nullptr) {
          slider->setRange(element.floatMin, element.floatMax, element.step);
          slider->setValue(*element.floatValueRef, juce::dontSendNotification);
          slider->onValueChange = [node = targetNode, slider,
                                   valRef = element.floatValueRef]() {
            *valRef = (float)slider->getValue();
            node->parameterChanged();
            if (node->onNodeDirtied) {
              node->onNodeDirtied();
            }
          };
        } else {
          slider->setRange(element.minValue, element.maxValue, element.step);
          if (element.valueRef != nullptr) {
            slider->setValue(*element.valueRef, juce::dontSendNotification);
            slider->onValueChange = [node = targetNode, slider,
                                     valRef = element.valueRef]() {
              *valRef = (int)slider->getValue();
              node->parameterChanged();
              if (node->onNodeDirtied) {
                node->onNodeDirtied();
              }
            };
          }
        }

        if (element.macroParamRef != nullptr) {
          auto updateSliderVisibility = [slider](const MacroParam *param) {
            if (param->bindings.empty()) {
              slider->removeColour(juce::Slider::rotarySliderFillColourId);
              slider->removeColour(juce::Slider::rotarySliderOutlineColourId);
            } else {
              auto c = getMacroColour(param->bindings[0].macroIndex);
              slider->setColour(juce::Slider::rotarySliderFillColourId, c);
              slider->setColour(juce::Slider::rotarySliderOutlineColourId,
                                c.withAlpha(0.3f));
            }
          };

          updateSliderVisibility(element.macroParamRef);

          GraphCanvas *canvasPtr = &parentCanvas;
          slider->onRightClick = [node = targetNode, slider,
                                  macroParam = element.macroParamRef, canvasPtr,
                                  &apvts]() {
            MacroMappingMenu::showMenu(
                slider, *macroParam,
                [node, macroParam, canvasPtr, slider, &apvts](int macroIndex) {
                  canvasPtr->performMutation(
                      [node, macroParam, canvasPtr, slider, &apvts,
                       macroIndex]() {
                        if (macroIndex == -1) {
                          macroParam->bindings.clear();
                        } else {
                          int finalIndex = macroIndex;
                          if (finalIndex == -2) {
                            finalIndex =
                                canvasPtr->getEngine().getNextFreeMacro();
                            if (finalIndex == -1)
                              return;
                          }

                          auto &bindings = macroParam->bindings;
                          auto it = std::find_if(
                              bindings.begin(), bindings.end(),
                              [finalIndex](const MacroBinding &b) {
                                return b.macroIndex == finalIndex;
                              });
                          if (it != bindings.end()) {
                            bindings.erase(it);
                          } else {
                            auto *ap = dynamic_cast<MacroParameter *>(
                                apvts.getParameter("macro_" +
                                                   juce::String(finalIndex +
                                                                1)));
                            if (ap != nullptr && !ap->isMapped()) {
                              float norm = (float)(
                                  (slider->getValue() - slider->getMinimum()) /
                                  (slider->getMaximum() - slider->getMinimum()));
                              ap->setValueNotifyingHost(norm);
                            }
                            bindings.push_back({finalIndex, 1.0f});
                          }
                        }

                        node->parameterChanged();
                        if (node->onMappingChanged)
                          node->onMappingChanged();

                        canvasPtr->rebuild();
                      });
                });
          };

          sliderMacroInfos.push_back({slider, element.macroParamRef});

          // Shift+drag binding wiring
          slider->selectedMacroPtr = selectedMacroPtr;
          slider->macroParamRef = element.macroParamRef;
          slider->getNextFreeMacro = [canvasPtr]() {
            return canvasPtr->getEngine().getNextFreeMacro();
          };
          slider->onAutoSelectMacro = onAutoSelectMacro;
          slider->onMappingChanged = [node = targetNode]() {
            node->parameterChanged();
            if (node->onMappingChanged)
              node->onMappingChanged();
          };
          slider->onBindingChanged = [node = targetNode, canvasPtr]() {
            node->parameterChanged();
            if (node->onMappingChanged)
              node->onMappingChanged();
            canvasPtr->rebuild();
          };
        }
        comp = slider;
      } else if (element.type == UIElementType::Label) {
        auto *label = new juce::Label();
        label->setText(element.label, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setInterceptsMouseClicks(false, false);

        if (element.colorHex.isNotEmpty()) {
          label->setColour(juce::Label::textColourId,
                           juce::Colour::fromString(element.colorHex));
          label->setFont(
              juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        }

        comp = label;
      } else if (element.type == UIElementType::PushButton ||
                 element.type == UIElementType::Toggle) {
        auto *button = new CustomMacroButton();
        button->setButtonText(element.label);

        if (element.type == UIElementType::Toggle) {
          button->setClickingTogglesState(true);
        }

        if (element.valueRef != nullptr) {
          button->setToggleState(*element.valueRef != 0,
                                 juce::dontSendNotification);

          button->onClick = [node = targetNode, button,
                             valRef = element.valueRef, element]() {
            if (element.macroParamRef != nullptr &&
                !element.macroParamRef->bindings.empty()) {
              return;
            }

            if (element.type == UIElementType::Toggle) {
              *valRef = button->getToggleState() ? 1 : 0;
            } else {
              *valRef = (*valRef == 0) ? 1 : 0;
            }

            node->parameterChanged();
            if (node->onNodeDirtied) {
              node->onNodeDirtied();
            }
          };
        }

        if (element.macroParamRef != nullptr) {
          GraphCanvas *canvasPtr = &parentCanvas;
          button->onRightClick = [node = targetNode, button,
                                  macroParam = element.macroParamRef, canvasPtr,
                                  &apvts]() {
            MacroMappingMenu::showMenu(
                button, *macroParam,
                [node, macroParam, canvasPtr, button, &apvts](int macroIndex) {
                  canvasPtr->performMutation(
                      [node, macroParam, canvasPtr, button, &apvts,
                       macroIndex]() {
                        if (macroIndex == -1) {
                          macroParam->bindings.clear();
                        } else {
                          int finalIndex = macroIndex;
                          if (finalIndex == -2) {
                            finalIndex =
                                canvasPtr->getEngine().getNextFreeMacro();
                            if (finalIndex == -1)
                              return;
                          }

                          auto &bindings = macroParam->bindings;
                          auto it = std::find_if(
                              bindings.begin(), bindings.end(),
                              [finalIndex](const MacroBinding &b) {
                                return b.macroIndex == finalIndex;
                              });
                          if (it != bindings.end()) {
                            bindings.erase(it);
                          } else {
                            auto *ap = dynamic_cast<MacroParameter *>(
                                apvts.getParameter("macro_" +
                                                   juce::String(finalIndex +
                                                                1)));
                            if (ap != nullptr && !ap->isMapped()) {
                              float norm =
                                  button->getToggleState() ? 1.0f : 0.0f;
                              ap->setValueNotifyingHost(norm);
                            }
                            bindings.push_back({finalIndex, 1.0f});
                          }
                        }

                        node->parameterChanged();
                        if (node->onMappingChanged)
                          node->onMappingChanged();

                        canvasPtr->rebuild();
                      });
                });
          };

          if (!element.macroParamRef->bindings.empty()) {
            juce::String paramID =
                "macro_" +
                juce::String(element.macroParamRef->bindings[0].macroIndex + 1);
            attachments.push_back(
                std::make_unique<ButtonAttachment>(apvts, paramID, *button));
          }

          // Shift+click binding wiring
          button->selectedMacroPtr = selectedMacroPtr;
          button->macroParamRef = element.macroParamRef;
          button->getNextFreeMacro = [canvasPtr]() {
            return canvasPtr->getEngine().getNextFreeMacro();
          };
          button->onAutoSelectMacro = onAutoSelectMacro;
          button->onMappingChanged = [node = targetNode]() {
            node->parameterChanged();
            if (node->onMappingChanged)
              node->onMappingChanged();
          };
          button->onBindingChanged = [node = targetNode, canvasPtr]() {
            node->parameterChanged();
            if (node->onMappingChanged)
              node->onMappingChanged();
            canvasPtr->rebuild();
          };
        }
        comp = button;
      } else if (element.type == UIElementType::ComboBox) {
        auto *combo = new juce::ComboBox();
        int i = 1;
        for (const auto &opt : element.options) {
          combo->addItem(opt, i++);
        }
        if (element.valueRef != nullptr) {
          combo->setSelectedId(*element.valueRef + 1,
                               juce::dontSendNotification);
          combo->onChange = [node = targetNode, combo,
                             valRef = element.valueRef]() {
            *valRef = combo->getSelectedId() - 1;
            node->parameterChanged();
            if (node->onNodeDirtied) {
              node->onNodeDirtied();
            }
          };
        }
        comp = combo;
      } else if (element.type == UIElementType::Custom) {
        if (auto custom =
                targetNode->createCustomComponent(element.customType, &apvts)) {
          comp = custom.release();
        }
      }

      if (comp != nullptr) {
        components.add(comp);
        addAndMakeVisible(comp);
      }
    }
  };

  auto layout = node->getLayout();
  createComponents(layout.elements, dynamicComponents, dynamicAttachments);

  if (layout.extendedGridWidth > 0 && layout.extendedGridHeight > 0) {
    addAndMakeVisible(expandButton);
    expandButton.setClickingTogglesState(true);
    expandButton.setColour(juce::TextButton::buttonColourId,
                           juce::Colours::transparentBlack);
    expandButton.setColour(juce::TextButton::buttonOnColourId,
                           juce::Colours::transparentBlack);
    expandButton.setColour(juce::TextButton::textColourOffId,
                           juce::Colours::white.withAlpha(0.7f));
    expandButton.setColour(juce::TextButton::textColourOnId,
                           juce::Colours::white);
    expandButton.onClick = [this] { toggleExpansion(); };

    createComponents(layout.extendedElements, extendedComponents,
                     extendedAttachments);
    for (auto *comp : extendedComponents) {
      comp->setVisible(false);
    }
  }

  if (layout.elements.empty()) {
    customControls = node->createEditorComponent(apvts);
    if (customControls != nullptr) {
      addAndMakeVisible(customControls.get());
    }
  }

  updateSize();
  startTimerHz(12);  // Steady UI sync
}

void NodeBlock::toggleExpansion() {
  isExpanded = !isExpanded;
  expandButton.setToggleState(isExpanded, juce::dontSendNotification);
  expandButton.setButtonText(isExpanded ? "<" : ">");

  for (auto *comp : extendedComponents) {
    comp->setVisible(isExpanded);
  }

  if (isExpanded) {
    toFront(false);
  }

  updateSize();

  if (onPositionChanged) {
    onPositionChanged();
  }
}

void NodeBlock::updateSize() {
  auto layout = targetNode->getLayout();
  int gridW = layout.gridWidth;
  int gridH = layout.gridHeight;

  if (isExpanded && layout.extendedGridWidth > 0 &&
      layout.extendedGridHeight > 0) {
    gridW = layout.extendedGridWidth;
    gridH = layout.extendedGridHeight;
  }

  int width = (gridW * Layout::GridPitch) - Layout::TramlineMargin;
  int height = (gridH * Layout::GridPitch) - Layout::TramlineMargin;

  setSize(width, height);
}

void NodeBlock::timerCallback() {
  auto layout = targetNode->getLayout();

  auto syncElements = [](const std::vector<UIElement> &elements,
                         const juce::OwnedArray<juce::Component> &components) {
    for (int i = 0; i < components.size(); ++i) {
      if (i < (int)elements.size()) {
        auto &element = elements[(size_t)i];
        auto *comp = components[i];

        if (auto *slider = dynamic_cast<juce::Slider *>(comp)) {
          if (element.dynamicMinRef != nullptr &&
              element.dynamicMaxRef != nullptr) {
            int curMin = *element.dynamicMinRef;
            int curMax = *element.dynamicMaxRef;
            if (curMin == curMax)
              curMax = curMin + 1;
            if (slider->getMinimum() != curMin ||
                slider->getMaximum() != curMax) {
              slider->setRange(curMin, curMax, element.step);
            }
          }

          if (!slider->isMouseButtonDown()) {
            if (element.floatValueRef != nullptr) {
              slider->setValue(*element.floatValueRef,
                               juce::dontSendNotification);
            } else if (element.valueRef != nullptr) {
              slider->setValue(*element.valueRef, juce::dontSendNotification);
            }
          }
        } else if (auto *button = dynamic_cast<juce::TextButton *>(comp)) {
          if (element.label != button->getButtonText()) {
            button->setButtonText(element.label);
          }
          if (element.valueRef != nullptr && !button->isMouseButtonDown()) {
            bool state = (*element.valueRef != 0);
            if (button->getToggleState() != state) {
              button->setToggleState(state, juce::dontSendNotification);
            }
          }
        } else if (auto *combo = dynamic_cast<juce::ComboBox *>(comp)) {
          if (element.valueRef != nullptr && !combo->isMouseButtonDown()) {
            int state = *element.valueRef + 1;
            if (combo->getSelectedId() != state) {
              combo->setSelectedId(state, juce::dontSendNotification);
            }
          }
        } else if (auto *label = dynamic_cast<juce::Label *>(comp)) {
          if (element.label != label->getText()) {
            label->setText(element.label, juce::dontSendNotification);
          }
        }
      }
    }
  };

  syncElements(layout.elements, dynamicComponents);
  if (isExpanded) {
    syncElements(layout.extendedElements, extendedComponents);
  }

  // Repaint when macro selection state changes, or when any slider has active
  // bindings (effective value indicator follows live macro knob movement)
  int currentSelected = selectedMacroPtr ? *selectedMacroPtr : -1;
  bool hasActiveBindings = false;
  for (const auto &info : sliderMacroInfos) {
    if (info.macroParamRef != nullptr && !info.macroParamRef->bindings.empty()) {
      hasActiveBindings = true;
      break;
    }
  }
  if (currentSelected != lastKnownSelectedMacro || hasActiveBindings) {
    lastKnownSelectedMacro = currentSelected;
    repaint();
  }
}

void NodeBlock::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();

  // Unified Node body: rounded rectangle (no distinct header bar anymore)
  g.setColour(ArpsLookAndFeel::getForegroundSlate());
  g.fillRoundedRectangle(bounds, 6.0f);

  if (targetNode->bypassed) {
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colours::white.withAlpha(0.1f));
    for (float x = -bounds.getHeight(); x < bounds.getWidth(); x += 12.0f) {
      g.drawLine(x, bounds.getHeight(), x + bounds.getHeight(), 0.0f, 2.0f);
    }
  }

  // Border (Neon tinted)
  if (parentCanvas.getSelectedNode() == targetNode.get()) {
    // Outer glow for selected node
    g.setColour(juce::Colour(0xff0df0e3));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f,
                           2.5f);

    // Selection Halo
    g.setColour(ArpsLookAndFeel::getNeonColor().withAlpha(0.4f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().expanded(4.0f), 8.0f,
                           3.0f);
  }

  // Highlight for proximity auto-connection
  if (parentCanvas.getProximityTargetNode() == targetNode.get()) {
    auto zone = parentCanvas.getProximityZone();
    g.setColour(juce::Colour(0x600df0e3));  // Semi-transparent neon
    if (zone == GraphCanvas::ProximityZone::Left) {
      g.fillRoundedRectangle(
          getLocalBounds().removeFromLeft(getWidth() / 4).toFloat(), 6.0f);
    } else if (zone == GraphCanvas::ProximityZone::Right) {
      g.fillRoundedRectangle(
          getLocalBounds().removeFromRight(getWidth() / 4).toFloat(), 6.0f);
    }
  } else {
    g.setColour(juce::Colour(0xff0df0e3).withAlpha(0.3f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f,
                           1.5f);
  }

  // Draw input ports (Half stadiums flush with left edge)
  int numIn = targetNode->getNumInputPorts();
  for (int i = 0; i < numIn; ++i) {
    auto rect = getInputPortRect(i).toFloat();

    // Half-stadium path (rounded on the right, flat on the left)
    juce::Path p;
    p.addRoundedRectangle(rect.getX() - rect.getWidth(), rect.getY(),
                          rect.getWidth() * 2.0f, rect.getHeight(),
                          rect.getHeight() * 0.5f);

    g.saveState();
    g.reduceClipRegion(rect.toNearestInt());  // Clip to just the right half

    g.setColour(juce::Colour(0xff44cc44));
    g.fillPath(p);
    g.setColour(juce::Colour(0xff228822));
    g.strokePath(p, juce::PathStrokeType(1.5f));

    g.restoreState();
  }

  // Draw output ports (Half stadiums flush with right edge)
  int numOut = targetNode->getNumOutputPorts();
  for (int i = 0; i < numOut; ++i) {
    auto rect = getOutputPortRect(i).toFloat();

    // Half-stadium path (rounded on the left, flat on the right)
    juce::Path p;
    p.addRoundedRectangle(rect.getX(), rect.getY(), rect.getWidth() * 2.0f,
                          rect.getHeight(), rect.getHeight() * 0.5f);

    g.saveState();
    g.reduceClipRegion(rect.toNearestInt());  // Clip to just the left half

    g.setColour(juce::Colour(0xff4499ff));
    g.fillPath(p);
    g.setColour(juce::Colour(0xff2266aa));
    g.strokePath(p, juce::PathStrokeType(1.5f));

    g.restoreState();
  }
}

void NodeBlock::paintOverChildren(juce::Graphics &g) {
  // Always draw intensity arcs for existing bindings, regardless of selection
  for (const auto &info : sliderMacroInfos) {
    if (info.macroParamRef == nullptr || info.macroParamRef->bindings.empty())
      continue;

    auto sliderBounds = info.slider->getBounds().toFloat().reduced(2.0f);
    float radius =
        (juce::jmin(sliderBounds.getWidth(), sliderBounds.getHeight()) / 2.0f) -
        2.0f;
    if (radius <= 0.0f)
      continue;

    float cx = sliderBounds.getCentreX();
    float cy = sliderBounds.getCentreY();
    auto rp = info.slider->getRotaryParameters();
    float sweep = rp.endAngleRadians - rp.startAngleRadians;
    float trackWidth = radius * 0.4f;
    float arcStroke = 2.5f;
    float arcGap = arcStroke + 1.0f;  // gap between concentric rings
    float firstArcRadius = radius + trackWidth * 0.5f + 2.0f;

    int ringIndex = 0;
    for (const auto &binding : info.macroParamRef->bindings) {
      float absIntensity = std::abs(binding.intensity);
      if (absIntensity < 0.001f)
        continue;

      float arcRadius = firstArcRadius + static_cast<float>(ringIndex) * arcGap;
      ++ringIndex;

      auto colour = getMacroColour(binding.macroIndex);
      // Arc extent: intensity 1.0 = half the sweep
      float arcAngle = absIntensity * sweep * 0.5f;

      float arcStart, arcEnd;
      if (binding.intensity >= 0.0f) {
        // Positive: extends from start angle clockwise
        arcStart = rp.startAngleRadians;
        arcEnd = rp.startAngleRadians + arcAngle;
      } else {
        // Negative: extends from end angle counter-clockwise
        arcEnd = rp.endAngleRadians;
        arcStart = rp.endAngleRadians - arcAngle;
      }

      juce::Path arc;
      arc.addCentredArc(cx, cy, arcRadius, arcRadius, 0.0f, arcStart, arcEnd,
                        true);
      g.setColour(colour.withAlpha(0.7f));
      g.strokePath(arc, juce::PathStrokeType(arcStroke,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    }

    // Effective value indicator: arc from set value to effective value + ring
    {
      auto setVal = (float)info.slider->getValue();
      auto minVal = (float)info.slider->getMinimum();
      auto maxVal = (float)info.slider->getMaximum();
      float range = maxVal - minVal;
      float effectiveVal = targetNode->resolveMacroFloat(
          *info.macroParamRef, setVal, minVal, maxVal);

      float setPos =
          range > 0.0f
              ? juce::jlimit(0.0f, 1.0f, (setVal - minVal) / range)
              : 0.0f;
      float effectivePos =
          range > 0.0f
              ? juce::jlimit(0.0f, 1.0f, (effectiveVal - minVal) / range)
              : 0.0f;

      float setAngle = rp.startAngleRadians + setPos * sweep;
      float effectiveAngle = rp.startAngleRadians + effectivePos * sweep;

      // Arc bridging set → effective on the track circle
      if (std::abs(effectiveAngle - setAngle) > 0.01f) {
        float arcFrom = juce::jmin(setAngle, effectiveAngle);
        float arcTo = juce::jmax(setAngle, effectiveAngle);
        juce::Path bridgeArc;
        bridgeArc.addCentredArc(cx, cy, radius, radius, 0.0f, arcFrom, arcTo,
                                true);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.strokePath(bridgeArc,
                     juce::PathStrokeType(trackWidth * 0.35f,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
      }

      // Hollow ring at effective value position
      float ex = cx + radius * std::cos(effectiveAngle -
                                        juce::MathConstants<float>::halfPi);
      float ey = cy + radius * std::sin(effectiveAngle -
                                        juce::MathConstants<float>::halfPi);
      g.setColour(juce::Colours::white.withAlpha(0.9f));
      g.drawEllipse(ex - 4.0f, ey - 4.0f, 8.0f, 8.0f, 1.5f);
    }
  }

  // Draw faint "bindable" rings when a macro is selected
  if (selectedMacroPtr == nullptr || *selectedMacroPtr == -1)
    return;

  int macroIdx = *selectedMacroPtr;
  auto colour = getMacroColour(macroIdx);

  for (const auto &info : sliderMacroInfos) {
    bool alreadyBound = false;
    if (info.macroParamRef != nullptr) {
      for (const auto &b : info.macroParamRef->bindings) {
        if (b.macroIndex == macroIdx) {
          alreadyBound = true;
          break;
        }
      }
    }

    if (!alreadyBound) {
      auto bounds = info.slider->getBounds().toFloat();
      float r =
          (juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f) - 1.0f;
      if (r > 0.0f) {
        auto centre = bounds.getCentre();
        g.setColour(colour.withAlpha(0.30f));
        g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 2.0f);
      }
    }
  }
}

void NodeBlock::resized() {
  auto bounds = getLocalBounds();

  // Floating Header Area
  auto header = bounds.removeFromTop(HEADER_HEIGHT);

  // Right aligned delete button
  deleteButton.setBounds(header.removeFromRight(HEADER_HEIGHT).reduced(3));

  // Left aligned bypass button
  bypassButton.setBounds(header.removeFromLeft(HEADER_HEIGHT).reduced(3));

  // Title
  titleLabel.setBounds(header.withTrimmedLeft(6));

  if (expandButton.isVisible()) {
    expandButton.setBounds(header.removeFromRight(HEADER_HEIGHT).reduced(3));
  }

  // Body: dynamic controls
  auto layout = targetNode->getLayout();

  auto layoutElements = [this](
                            const std::vector<UIElement> &elements,
                            const juce::OwnedArray<juce::Component> &components,
                            int gridW, int gridH) {
    // Available body area for placing controls
    int bodyWidth = getWidth() - (PORT_MARGIN * 2);
    int bodyHeight = getHeight() - HEADER_HEIGHT - 4;

    constexpr int SUBS_PER_UNIT = 20;
    int gridCols = gridW * SUBS_PER_UNIT;
    int gridRows = gridH * SUBS_PER_UNIT;

    float subGridX = (float)bodyWidth / (float)gridCols;
    float subGridY = (float)bodyHeight / (float)gridRows;

    int startX = PORT_MARGIN;
    int startY = HEADER_HEIGHT;

    for (int i = 0; i < components.size(); ++i) {
      if (i < (int)elements.size()) {
        auto &element = elements[(size_t)i];
        auto eb = element.gridBounds;

        components[i]->setBounds(startX + (int)(eb.getX() * subGridX),
                                 startY + (int)(eb.getY() * subGridY),
                                 (int)(eb.getWidth() * subGridX),
                                 (int)(eb.getHeight() * subGridY));
      }
    }
  };

  if (isExpanded) {
    layoutElements(layout.extendedElements, extendedComponents,
                   layout.extendedGridWidth, layout.extendedGridHeight);
  } else {
    layoutElements(layout.elements, dynamicComponents, layout.gridWidth,
                   layout.gridHeight);
  }

  if (customControls != nullptr) {
    customControls->setBounds(bounds.reduced(PORT_MARGIN, 4));
  }
}

void NodeBlock::mouseDown(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  // Notify canvas of selection (for z-ordering and cable highlighting)
  if (onSelected) {
    onSelected();
  }

  // Right-click on header to show replacement menu
  if (e.mods.isRightButtonDown() && e.y < 28) {
    juce::PopupMenu m;
    juce::PopupMenu replaceMenu;

    auto types = NodeFactory::getAvailableNodeTypes();
    int i = 1;
    for (const auto &type : types) {
      // Don't include current type in replacement list
      if (type != targetNode->getName()) {
        replaceMenu.addItem(i, type);
      }
      i++;
    }

    m.addSubMenu("Replace with...", replaceMenu);

    m.showMenuAsync(juce::PopupMenu::Options(), [this, types](int result) {
      if (result > 0 && result <= (int)types.size()) {
        if (onReplaceRequest) {
          onReplaceRequest(types[(size_t)result - 1]);
        }
      }
    });

    return;
  }

  auto pos = e.getPosition();

  // Check if we hit a port
  int outPort = hitTestOutputPort(pos);
  if (outPort >= 0) {
    isDraggingCable = true;
    isDraggingNode = false;
    if (onPortDragStart) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragStart(this, outPort, true, canvasPos);
    }
    return;
  }

  int inPort = hitTestInputPort(pos);
  if (inPort >= 0) {
    isDraggingCable = true;
    isDraggingNode = false;
    if (onPortDragStart) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragStart(this, inPort, false, canvasPos);
    }
    return;
  }

  // Otherwise, start dragging the node
  isDraggingNode = true;
  isDraggingCable = false;

  // No longer using standard ComponentDragger since we do strict grid hopping
  // But we store the starting point of the drag
  dragStartGridX = targetNode->gridX;
  dragStartGridY = targetNode->gridY;
  dragStartWorldX = targetNode->nodeX;
  dragStartWorldY = targetNode->nodeY;

  parentCanvas.setGhostTarget(dragStartGridX, dragStartGridY,
                              targetNode->getGridWidth(),
                              targetNode->getGridHeight(), targetNode.get());
}

void NodeBlock::mouseDrag(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  if (isDraggingCable) {
    if (onPortDragging) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragging(canvasPos);
    }
    return;
  }

  if (isDraggingNode) {
    // Actually, juce::MouseEvent::getDistanceFromDragStartX() is measured in
    // the component's local coordinates. When the component is scaled via
    // AffineTransform, its local coordinate system remains constant (e.g.,
    // width is always 190). So the distance drag is exactly how many abstract
    // pixels we've moved.
    int deltaX = e.getDistanceFromDragStartX();
    int deltaY = e.getDistanceFromDragStartY();

    // Convert to grid slots: roughly 100px per slot
    int gridDeltaX = (int)std::round((float)deltaX / Layout::GridPitchFloat);
    int gridDeltaY = (int)std::round((float)deltaY / Layout::GridPitchFloat);

    int newGridX = dragStartGridX + gridDeltaX;
    int newGridY = dragStartGridY + gridDeltaY;

    if (newGridX != parentCanvas.getGhostX() ||
        newGridY != parentCanvas.getGhostY()) {
      // Set ghost target; parentCanvas handles the overlap check to tint it
      // red/green
      parentCanvas.setGhostTarget(
          newGridX, newGridY, targetNode->getGridWidth(),
          targetNode->getGridHeight(), targetNode.get());
    }

    // We ALWAYS move the floating physical coordinate of the node to follow
    // the mouse linearly. This must be outside the if block so it updates
    // continuously!
    targetNode->nodeX = dragStartGridX * Layout::GridPitchFloat +
                        Layout::TramlineOffset + deltaX;
    targetNode->nodeY = dragStartGridY * Layout::GridPitchFloat +
                        Layout::TramlineOffset + deltaY;

    if (onPositionChanged) {
      onPositionChanged();
    }

    parentCanvas.updateProximityHighlight(
        e.getEventRelativeTo(&parentCanvas).getPosition(), targetNode.get());
  }
}

void NodeBlock::mouseUp(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  auto mousePosOnCanvas = e.getEventRelativeTo(&parentCanvas).getPosition();

  if (isDraggingCable && onPortDragEnd) {
    onPortDragEnd(mousePosOnCanvas);
  }
  if (isDraggingNode) {
    if (e.mods.isCtrlDown()) {
      int finalX = parentCanvas.getGhostX();
      int finalY = parentCanvas.getGhostY();
      cancelDrag();  // Revert original
      parentCanvas.requestNodeClone(targetNode.get(), finalX, finalY);
      return;
    }

    // Snap to the ghost slot if it's valid...
    int finalGridX = dragStartGridX;
    int finalGridY = dragStartGridY;

    if (parentCanvas.isGhostValid()) {
      finalGridX = parentCanvas.getGhostX();
      finalGridY = parentCanvas.getGhostY();
    } else {
      auto nearest = parentCanvas.getEngine().findClosestFreeSpot(
          parentCanvas.getGhostX(), parentCanvas.getGhostY(),
          targetNode->getGridWidth(), targetNode->getGridHeight(),
          targetNode.get());
      finalGridX = nearest.x;
      finalGridY = nearest.y;
    }

    if (finalGridX != dragStartGridX || finalGridY != dragStartGridY) {
      if (parentCanvas.performMutation) {
        parentCanvas.performMutation([this, finalGridX, finalGridY]() {
          targetNode->gridX = finalGridX;
          targetNode->gridY = finalGridY;
          targetNode->nodeX = (float)(targetNode->gridX * Layout::GridPitch) +
                              Layout::TramlineOffset;
          targetNode->nodeY = (float)(targetNode->gridY * Layout::GridPitch) +
                              Layout::TramlineOffset;
        });
      } else {
        targetNode->gridX = finalGridX;
        targetNode->gridY = finalGridY;
        targetNode->nodeX = (float)(targetNode->gridX * Layout::GridPitch) +
                            Layout::TramlineOffset;
        targetNode->nodeY = (float)(targetNode->gridY * Layout::GridPitch) +
                            Layout::TramlineOffset;
      }
    } else {
      // Revert to original position if the snap doesn't result in a grid change
      targetNode->nodeX = dragStartWorldX;
      targetNode->nodeY = dragStartWorldY;
    }

    parentCanvas.clearGhostTarget();

    if (onPositionChanged) {
      onPositionChanged();
    }

    if (!parentCanvas.attemptSignalPathInsertion(targetNode.get(),
                                                 mousePosOnCanvas)) {
      parentCanvas.attemptProximityConnection(targetNode.get(),
                                              mousePosOnCanvas);
    }
    parentCanvas.clearProximityHighlight();
  }

  isDraggingCable = false;
  isDraggingNode = false;
}

void NodeBlock::cancelDrag() {
  if (isDraggingNode) {
    targetNode->gridX = dragStartGridX;
    targetNode->gridY = dragStartGridY;
    targetNode->nodeX = dragStartWorldX;
    targetNode->nodeY = dragStartWorldY;
    parentCanvas.clearGhostTarget();
    if (onPositionChanged) {
      onPositionChanged();
    }
  }
  isDraggingNode = false;
  isDraggingCable = false;
}

juce::Rectangle<int> NodeBlock::getInputPortRect(int portIndex) {
  int y = HEADER_HEIGHT + 10 + (portIndex * PORT_SPACING);
  // Flush with the left edge. Width is RADIUS. Height is 2*RADIUS.
  return {0, y - PORT_RADIUS, PORT_RADIUS, PORT_RADIUS * 2};
}

juce::Rectangle<int> NodeBlock::getOutputPortRect(int portIndex) const {
  int y = HEADER_HEIGHT + 10 + (portIndex * PORT_SPACING);
  // Flush with the right edge. Width is RADIUS. Height is 2*RADIUS.
  return {getWidth() - PORT_RADIUS, y - PORT_RADIUS, PORT_RADIUS,
          PORT_RADIUS * 2};
}

juce::Point<int> NodeBlock::getInputPortCentre(int portIndex) const {
  auto rect = getInputPortRect(portIndex);
  return getPosition() + rect.getCentre();
}

juce::Point<int> NodeBlock::getOutputPortCentre(int portIndex) const {
  auto rect = getOutputPortRect(portIndex);
  return getPosition() + rect.getCentre();
}

int NodeBlock::hitTestInputPort(juce::Point<int> localPoint) const {
  int numIn = targetNode->getNumInputPorts();
  auto localFloat = localPoint.toFloat();
  for (int i = 0; i < numIn; ++i) {
    auto rect = getInputPortRect(i);
    auto centre = rect.getCentre().toFloat();
    if (localFloat.getDistanceFrom(centre) <= (float)PORT_HIT_RADIUS) {
      return i;
    }
  }
  return -1;
}

int NodeBlock::hitTestOutputPort(juce::Point<int> localPoint) const {
  int numOut = targetNode->getNumOutputPorts();
  auto localFloat = localPoint.toFloat();
  for (int i = 0; i < numOut; ++i) {
    auto rect = getOutputPortRect(i);
    auto centre = rect.getCentre().toFloat();
    if (localFloat.getDistanceFrom(centre) <= (float)PORT_HIT_RADIUS) {
      return i;
    }
  }
  return -1;
}

NodeDragPreview::NodeDragPreview(juce::String nodeType, int gridW, int gridH,
                                 int numIn, int numOut)
    : type(std::move(nodeType)),
      gridW(gridW),
      gridH(gridH),
      numIn(numIn),
      numOut(numOut) {
  int width = (gridW * Layout::GridPitch) - Layout::TramlineMargin;
  int height = (gridH * Layout::GridPitch) - Layout::TramlineMargin;
  setSize(width, height);
  setInterceptsMouseClicks(false, false);
}

void NodeDragPreview::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();

  // Draw translucent body
  g.setColour(ArpsLookAndFeel::getForegroundSlate().withAlpha(0.6f));
  g.fillRoundedRectangle(bounds, 6.0f);

  // Border
  g.setColour(juce::Colours::white.withAlpha(0.4f));
  g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

  // Header/Title
  g.setColour(juce::Colours::white.withAlpha(0.8f));
  g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  g.drawText(type, 10, 0, getWidth() - 20, NodeBlock::HEADER_HEIGHT,
             juce::Justification::centredLeft);

  // Ports
  g.setColour(ArpsLookAndFeel::getBackgroundCharcoal().withAlpha(0.8f));
  for (int i = 0; i < numIn; ++i) {
    int py = NodeBlock::HEADER_HEIGHT + 10 + (i * NodeBlock::PORT_SPACING);
    g.fillEllipse(0.0f, (float)(py - NodeBlock::PORT_RADIUS),
                  (float)NodeBlock::PORT_RADIUS,
                  (float)(NodeBlock::PORT_RADIUS * 2));
  }
  for (int i = 0; i < numOut; ++i) {
    int py = NodeBlock::HEADER_HEIGHT + 10 + (i * NodeBlock::PORT_SPACING);
    g.fillEllipse((float)(getWidth() - NodeBlock::PORT_RADIUS),
                  (float)(py - NodeBlock::PORT_RADIUS),
                  (float)NodeBlock::PORT_RADIUS,
                  (float)(NodeBlock::PORT_RADIUS * 2));
  }
}
