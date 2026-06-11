#include "NodeBlock.h"

#include <utility>

#include "ArpsLookAndFeel.h"
#include "GraphCanvas.h"
#include "GraphEngine.h"
#include "MacroColours.h"
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
                              juce::OwnedArray<juce::Component> &components) {
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
        slider->defaultValue = slider->getValue();

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
                                  macroParam = element.macroParamRef,
                                  canvasPtr]() {
            juce::PopupMenu menu;
            if (macroParam->bindings.empty()) {
              menu.addItem(1, "No macro bindings", false, false);
            } else {
              for (const auto &b : macroParam->bindings) {
                menu.addItem(b.macroIndex + 2,
                             "Remove: Macro " + juce::String(b.macroIndex + 1));
              }
            }
            auto options = juce::PopupMenu::Options().withTargetScreenArea(
                slider->getScreenBounds());
            menu.showMenuAsync(
                options, [node, macroParam, canvasPtr](int result) {
                  if (result < 2)
                    return;
                  int macroIndex = result - 2;
                  canvasPtr->performMutation([node, macroParam, macroIndex]() {
                    auto &bindings = macroParam->bindings;
                    bindings.erase(
                        std::remove_if(bindings.begin(), bindings.end(),
                                       [macroIndex](const MacroBinding &b) {
                                         return b.macroIndex == macroIndex;
                                       }),
                        bindings.end());
                    node->parameterChanged();
                    if (node->onMappingChanged)
                      node->onMappingChanged();
                  });
                  canvasPtr->rebuild();
                });
          };

          sliderMacroInfos.push_back({slider, element.macroParamRef});

          // Shift+drag binding wiring
          slider->selectedMacroPtr = selectedMacroPtr;
          slider->macroParamRef = element.macroParamRef;
          slider->getNextFreeMacro = [canvasPtr]() {
            return canvasPtr->getEngine().getNextFreeMacro();
          };
          slider->onHoverMacros = [this](std::vector<int> v) {
            if (onHoverMacros) {
              onHoverMacros(std::move(v));
            }
          };
          slider->onMappingChanged = [node = targetNode]() {
            node->parameterChanged();
            if (node->onMappingChanged) {
              node->onMappingChanged();
            }
          };
          slider->onBindingChanged = [node = targetNode, canvasPtr]() {
            node->parameterChanged();
            if (node->onMappingChanged) {
              node->onMappingChanged();
            }
            canvasPtr->rebuild();
          };
          slider->onRequestMidiLearn = [this](MacroParam *mp) {
            if (onRequestMidiLearn) {
              onRequestMidiLearn(mp);
            }
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
                                  macroParam = element.macroParamRef,
                                  canvasPtr]() {
            juce::PopupMenu menu;
            if (macroParam->bindings.empty()) {
              menu.addItem(1, "No macro bindings", false, false);
            } else {
              for (const auto &b : macroParam->bindings) {
                menu.addItem(b.macroIndex + 2,
                             "Remove: Macro " + juce::String(b.macroIndex + 1));
              }
            }
            auto options = juce::PopupMenu::Options().withTargetScreenArea(
                button->getScreenBounds());
            menu.showMenuAsync(
                options, [node, macroParam, canvasPtr](int result) {
                  if (result < 2)
                    return;
                  int macroIndex = result - 2;
                  canvasPtr->performMutation([node, macroParam, macroIndex]() {
                    auto &bindings = macroParam->bindings;
                    bindings.erase(
                        std::remove_if(bindings.begin(), bindings.end(),
                                       [macroIndex](const MacroBinding &b) {
                                         return b.macroIndex == macroIndex;
                                       }),
                        bindings.end());
                    node->parameterChanged();
                    if (node->onMappingChanged)
                      node->onMappingChanged();
                  });
                  canvasPtr->rebuild();
                });
          };

          // Shift+click binding wiring
          button->selectedMacroPtr = selectedMacroPtr;
          button->macroParamRef = element.macroParamRef;
          button->getNextFreeMacro = [canvasPtr]() {
            return canvasPtr->getEngine().getNextFreeMacro();
          };
          button->onMappingChanged = [node = targetNode]() {
            node->parameterChanged();
            if (node->onMappingChanged) {
              node->onMappingChanged();
            }
          };
          button->onBindingChanged = [node = targetNode, canvasPtr]() {
            node->parameterChanged();
            if (node->onMappingChanged) {
              node->onMappingChanged();
            }
            canvasPtr->rebuild();
          };
          button->onHoverMacros = [this](std::vector<int> v) {
            if (onHoverMacros) {
              onHoverMacros(std::move(v));
            }
          };
          button->onRequestMidiLearn = [this](MacroParam *mp) {
            if (onRequestMidiLearn) {
              onRequestMidiLearn(mp);
            }
          };

          buttonMacroInfos.push_back(
              {button, element.macroParamRef, element.valueRef});
        }
        comp = button;
      } else if (element.type == UIElementType::ComboBox) {
        auto *combo = new CustomMacroComboBox();
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

        if (element.macroParamRef != nullptr) {
          GraphCanvas *canvasPtr = &parentCanvas;
          combo->onRightClick = [node = targetNode, combo,
                                 macroParam = element.macroParamRef,
                                 canvasPtr]() {
            juce::PopupMenu menu;
            if (macroParam->bindings.empty()) {
              menu.addItem(1, "No macro bindings", false, false);
            } else {
              for (const auto &b : macroParam->bindings) {
                menu.addItem(b.macroIndex + 2,
                             "Remove: Macro " + juce::String(b.macroIndex + 1));
              }
            }
            auto options = juce::PopupMenu::Options().withTargetScreenArea(
                combo->getScreenBounds());
            menu.showMenuAsync(
                options, [node, macroParam, canvasPtr](int result) {
                  if (result < 2)
                    return;
                  int macroIndex = result - 2;
                  canvasPtr->performMutation([node, macroParam, macroIndex]() {
                    auto &bindings = macroParam->bindings;
                    bindings.erase(
                        std::remove_if(bindings.begin(), bindings.end(),
                                       [macroIndex](const MacroBinding &b) {
                                         return b.macroIndex == macroIndex;
                                       }),
                        bindings.end());
                    node->parameterChanged();
                    if (node->onMappingChanged)
                      node->onMappingChanged();
                  });
                  canvasPtr->rebuild();
                });
          };

          combo->selectedMacroPtr = selectedMacroPtr;
          combo->macroParamRef = element.macroParamRef;
          combo->getNextFreeMacro = [canvasPtr]() {
            return canvasPtr->getEngine().getNextFreeMacro();
          };
          combo->onHoverMacros = [this](std::vector<int> v) {
            if (onHoverMacros) {
              onHoverMacros(std::move(v));
            }
          };
          combo->onMappingChanged = [node = targetNode]() {
            node->parameterChanged();
            if (node->onMappingChanged) {
              node->onMappingChanged();
            }
          };
          combo->onBindingChanged = [node = targetNode, canvasPtr]() {
            node->parameterChanged();
            if (node->onMappingChanged) {
              node->onMappingChanged();
            }
            canvasPtr->rebuild();
          };
          combo->onRequestMidiLearn = [this](MacroParam *mp) {
            if (onRequestMidiLearn) {
              onRequestMidiLearn(mp);
            }
          };
          comboMacroInfos.push_back(
              {combo, element.macroParamRef, element.valueRef});
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
  createComponents(layout.elements, dynamicComponents);

  if (layout.hasUnfoldedLayout()) {
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

    createComponents(layout.unfoldedElements, unfoldedComponents);
  }

  if (layout.elements.empty()) {
    customControls = node->createEditorComponent(apvts);
    if (customControls != nullptr) {
      addAndMakeVisible(customControls.get());
    }
  }

  updateTabVisibility();
  updateSize();
  startTimerHz(12);  // Steady UI sync
}

void NodeBlock::toggleExpansion() {
  // When trying to expand, check that the unfold footprint is clear.
  if (!isExpanded &&
      !parentCanvas.getEngine().isUnfoldFootprintClear(targetNode.get())) {
    // Flash the expand button to signal there's no room.
    unfoldBlockedFlashCounter = 6;
    expandButton.setColour(juce::TextButton::textColourOffId,
                           juce::Colours::orangered);
    startTimer(80);
    return;
  }

  isExpanded = !isExpanded;
  targetNode->isUnfoldedRuntime = isExpanded;
  expandButton.setToggleState(isExpanded, juce::dontSendNotification);
  expandButton.setButtonText(isExpanded ? "<" : ">");

  if (isExpanded) {
    toFront(false);
  }

  updateTabVisibility();
  updateSize();

  if (onPositionChanged) {
    onPositionChanged();
  }
}

void NodeBlock::updateTabVisibility() {
  auto layout = targetNode->getLayout();

  // Compact elements: shown only when folded — unfolded view fully replaces
  // compact
  if (!isExpanded) {
    auto [start, count] = layout.compactTabRange(activeCompactTab);
    for (int i = 0; i < dynamicComponents.size(); ++i) {
      dynamicComponents[i]->setVisible(i >= start && i < start + count);
    }
  } else {
    for (auto *c : dynamicComponents) {
      c->setVisible(false);
    }
  }

  // Unfolded elements: visible only when unfolded
  if (isExpanded) {
    auto [start, count] = layout.unfoldedTabElementRange(activeUnfoldedTab);
    for (int i = 0; i < unfoldedComponents.size(); ++i) {
      unfoldedComponents[i]->setVisible(i >= start && i < start + count);
    }
  } else {
    for (auto *c : unfoldedComponents) {
      c->setVisible(false);
    }
  }
}

void NodeBlock::updateSize() {
  auto layout = targetNode->getLayout();
  int gridW = layout.gridWidth;
  int gridH = layout.gridHeight;

  if (isExpanded && layout.hasUnfoldedLayout()) {
    const auto &e = layout.unfoldExtents;
    gridW += e.left + e.right;
    gridH += e.up + e.down;
  }

  setSize((gridW * Layout::GridPitch) - Layout::TramlineMargin,
          (gridH * Layout::GridPitch) - Layout::TramlineMargin);
}

juce::Rectangle<int> NodeBlock::compactBodyRect() const {
  auto layout = targetNode->getLayout();
  if (!isExpanded || !layout.hasUnfoldedLayout()) {
    return getLocalBounds();
  }
  int compactW =
      (layout.gridWidth * Layout::GridPitch) - Layout::TramlineMargin;
  int compactH =
      (layout.gridHeight * Layout::GridPitch) - Layout::TramlineMargin;
  const auto &e = layout.unfoldExtents;
  return {e.left * Layout::GridPitch, e.up * Layout::GridPitch, compactW,
          compactH};
}

void NodeBlock::timerCallback() {
  auto layout = targetNode->getLayout();

  auto syncElements = [](const std::vector<UIElement> &elements,
                         const juce::OwnedArray<juce::Component> &components) {
    for (int i = 0; i < components.size(); ++i) {
      if (i < (int)elements.size()) {
        const auto &element = elements[(size_t)i];
        auto *comp = components[i];

        if (auto *slider = dynamic_cast<juce::Slider *>(comp)) {
          if (element.dynamicMinRef != nullptr &&
              element.dynamicMaxRef != nullptr) {
            int curMin = *element.dynamicMinRef;
            int curMax = *element.dynamicMaxRef;
            if (curMin == curMax) {
              curMax = curMin + 1;
            }
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
    syncElements(layout.unfoldedElements, unfoldedComponents);
  }

  // Decay the blocked-unfold flash.
  if (unfoldBlockedFlashCounter > 0) {
    --unfoldBlockedFlashCounter;
    if (unfoldBlockedFlashCounter == 0) {
      expandButton.setColour(juce::TextButton::textColourOffId,
                             juce::Colours::white.withAlpha(0.7f));
    }
  }

  // Update combo boxes that have active macro bindings to show the effective
  // value
  for (const auto &info : comboMacroInfos) {
    if (info.macroParamRef == nullptr || info.macroParamRef->bindings.empty())
      continue;
    if (info.valueRef == nullptr || info.combo->isMouseButtonDown())
      continue;
    int numItems = info.combo->getNumItems();
    if (numItems <= 0)
      continue;
    int effective = targetNode->resolveMacroInt(
        *info.macroParamRef, *info.valueRef, 0, numItems - 1);
    int effectiveId = effective + 1;
    if (info.combo->getSelectedId() != effectiveId)
      info.combo->setSelectedId(effectiveId, juce::dontSendNotification);
  }

  // Repaint when macro selection state changes, or when any slider has active
  // bindings (effective value indicator follows live macro knob movement)
  int currentSelected = selectedMacroPtr ? *selectedMacroPtr : -1;
  bool hasActiveBindings = false;
  for (const auto &info : sliderMacroInfos) {
    if (info.macroParamRef != nullptr &&
        !info.macroParamRef->bindings.empty()) {
      hasActiveBindings = true;
      break;
    }
  }
  if (!hasActiveBindings) {
    for (const auto &info : buttonMacroInfos) {
      if (info.macroParamRef != nullptr &&
          !info.macroParamRef->bindings.empty()) {
        hasActiveBindings = true;
        break;
      }
    }
  }
  if (!hasActiveBindings) {
    for (const auto &info : comboMacroInfos) {
      if (info.macroParamRef != nullptr &&
          !info.macroParamRef->bindings.empty()) {
        hasActiveBindings = true;
        break;
      }
    }
  }
  if (currentSelected != lastKnownSelectedMacro || hasActiveBindings) {
    lastKnownSelectedMacro = currentSelected;
    repaint();
  }
}

void NodeBlock::paint(juce::Graphics &g) {
  bool unfolded = isExpanded && targetNode->getLayout().hasUnfoldedLayout();
  auto bounds = getLocalBounds().toFloat();

  // Body fill — slightly brighter slate when unfolded to hint at expanded state
  g.setColour(unfolded ? ArpsLookAndFeel::getForegroundSlate().brighter(0.1f)
                       : ArpsLookAndFeel::getForegroundSlate());
  g.fillRoundedRectangle(bounds, unfolded ? 8.0f : 6.0f);

  if (targetNode->bypassed) {
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());
    for (float x = bounds.getX() - bounds.getHeight(); x < bounds.getRight();
         x += 12.0f) {
      g.drawLine(x, bounds.getBottom(), x + bounds.getHeight(), bounds.getY(),
                 2.0f);
    }
    g.restoreState();
  }

  // Border (Neon tinted) — wraps the full panel when unfolded
  float cornerRadius = unfolded ? 8.0f : 6.0f;
  if (parentCanvas.isNodeSelected(targetNode.get())) {
    g.setColour(juce::Colour(0xff0df0e3));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 2.5f);
    g.setColour(ArpsLookAndFeel::getNeonColor().withAlpha(0.4f));
    g.drawRoundedRectangle(bounds.expanded(4.0f), cornerRadius + 2.0f, 3.0f);
  }

  // Highlight for proximity auto-connection
  if (parentCanvas.getProximityTargetNode() == targetNode.get()) {
    auto zone = parentCanvas.getProximityZone();
    g.setColour(juce::Colour(0x600df0e3));  // Semi-transparent neon
    auto boundsI = bounds.toNearestInt();
    if (zone == GraphCanvas::ProximityZone::Left) {
      g.fillRoundedRectangle(
          boundsI.removeFromLeft(boundsI.getWidth() / 4).toFloat(),
          cornerRadius);
    } else if (zone == GraphCanvas::ProximityZone::Right) {
      g.fillRoundedRectangle(
          boundsI.removeFromRight(boundsI.getWidth() / 4).toFloat(),
          cornerRadius);
    }
  } else {
    g.setColour(juce::Colour(0xff0df0e3).withAlpha(0.3f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.5f);
  }

  // Draw tab bars where applicable
  {
    auto layout = targetNode->getLayout();

    auto drawTabBar = [&](juce::Rectangle<int> tabRect,
                          const juce::StringArray &labels, int activeTab) {
      if (labels.size() <= 1) {
        return;
      }
      int n = labels.size();
      int tabW = tabRect.getWidth() / n;
      for (int t = 0; t < n; ++t) {
        int tx = tabRect.getX() + t * tabW;
        int tw = (t == n - 1) ? tabRect.getRight() - tx : tabW;
        juce::Rectangle<int> tRect(tx, tabRect.getY(), tw, tabRect.getHeight());
        auto tRectF = tRect.reduced(1).toFloat();
        if (t == activeTab) {
          g.setColour(juce::Colour(0xff0df0e3).withAlpha(0.25f));
          g.fillRoundedRectangle(tRectF, 3.0f);
        }
        g.setColour(juce::Colour(0xff0df0e3).withAlpha(0.5f));
        g.drawRoundedRectangle(tRectF, 3.0f, 1.0f);
        g.setColour(
            juce::Colours::white.withAlpha(t == activeTab ? 1.0f : 0.6f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText(labels[t], tRect, juce::Justification::centred);
      }
    };

    if (!isExpanded && layout.hasCompactTabs()) {
      auto compactBody = compactBodyRect();
      compactBody.removeFromTop(HEADER_HEIGHT);
      drawTabBar(compactBody.removeFromTop(TAB_HEIGHT), layout.compactTabLabels,
                 activeCompactTab);
    }

    if (isExpanded && layout.hasUnfoldedTabs()) {
      auto area = getLocalBounds();
      area.removeFromTop(HEADER_HEIGHT);
      drawTabBar(area.removeFromTop(TAB_HEIGHT), layout.unfoldedTabLabels,
                 activeUnfoldedTab);
    }
  }

  // Helper: pick fill/stroke colours based on port type
  auto portColors =
      [](GraphNode::PortType pt) -> std::pair<juce::Colour, juce::Colour> {
    if (pt == GraphNode::PortType::CC)
      return {juce::Colour(0xffaa44ff), juce::Colour(0xff7722cc)};
    if (pt == GraphNode::PortType::Agnostic)
      return {juce::Colour(0xffaaaaaa), juce::Colour(0xff777777)};
    // Notes (default)
    return {juce::Colour(0xffd4a017), juce::Colour(0xffa07800)};
  };

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

    auto [fill, stroke] = portColors(targetNode->getInputPortType(i));
    g.setColour(fill);
    g.fillPath(p);
    g.setColour(stroke);
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

    auto [fill, stroke] = portColors(targetNode->getOutputPortType(i));
    g.setColour(fill);
    g.fillPath(p);
    g.setColour(stroke);
    g.strokePath(p, juce::PathStrokeType(1.5f));

    g.restoreState();
  }
}

void NodeBlock::paintOverChildren(juce::Graphics &g) {
  MacroOverlay::paint(
      g, targetNode.get(), sliderMacroInfos, buttonMacroInfos, comboMacroInfos,
      selectedMacroPtr ? *selectedMacroPtr : -1, highlightedMacroIndex);
}

void NodeBlock::resized() {
  auto layout = targetNode->getLayout();
  bool unfolded = isExpanded && layout.hasUnfoldedLayout();

  // Header: top of full panel when unfolded, else compact body.
  auto headerSource = unfolded ? getLocalBounds() : compactBodyRect();
  auto compactBody = compactBodyRect();
  auto header = headerSource.removeFromTop(HEADER_HEIGHT);

  deleteButton.setBounds(header.removeFromRight(HEADER_HEIGHT).reduced(3));
  bypassButton.setBounds(header.removeFromLeft(HEADER_HEIGHT).reduced(3));
  if (expandButton.isVisible()) {
    expandButton.setBounds(header.removeFromRight(HEADER_HEIGHT).reduced(3));
  }
  titleLabel.setBounds(header.withTrimmedLeft(6));

  // Compact body content area (below header)
  compactBody.removeFromTop(HEADER_HEIGHT);
  if (layout.hasCompactTabs()) {
    compactBody.removeFromTop(TAB_HEIGHT);
  }

  // Places elements within a given body rect, mapping (gridW x gridH) subgrid
  // coordinates to pixel positions.
  auto layoutElementsInRect =
      [](const std::vector<UIElement> &elements,
         const juce::OwnedArray<juce::Component> &components, int startX,
         int startY, int bodyWidth, int bodyHeight, int gridW, int gridH) {
        constexpr int SUBS_PER_UNIT = 20;
        int gridCols = gridW * SUBS_PER_UNIT;
        int gridRows = gridH * SUBS_PER_UNIT;
        float subGridX = (float)bodyWidth / (float)gridCols;
        float subGridY = (float)bodyHeight / (float)gridRows;
        for (int i = 0; i < components.size(); ++i) {
          if (i < (int)elements.size()) {
            const auto &element = elements[(size_t)i];
            auto eb = element.gridBounds;
            components[i]->setBounds(
                startX + (int)(static_cast<float>(eb.getX()) * subGridX),
                startY + (int)(static_cast<float>(eb.getY()) * subGridY),
                (int)(static_cast<float>(eb.getWidth()) * subGridX),
                (int)(static_cast<float>(eb.getHeight()) * subGridY));
          }
        }
      };

  // Compact controls in compact body.
  layoutElementsInRect(
      layout.elements, dynamicComponents, compactBody.getX() + PORT_MARGIN,
      compactBody.getY(), compactBody.getWidth() - (PORT_MARGIN * 2),
      compactBody.getHeight() - 4, layout.gridWidth, layout.gridHeight);

  // Unfolded controls span the full panel below the header (and tab bar).
  if (unfolded) {
    const auto &e = layout.unfoldExtents;
    int unfoldGridW = layout.gridWidth + e.left + e.right;
    int unfoldGridH = layout.gridHeight + e.up + e.down;
    int ringTop = HEADER_HEIGHT;
    if (layout.hasUnfoldedTabs()) {
      ringTop += TAB_HEIGHT;
    }
    layoutElementsInRect(layout.unfoldedElements, unfoldedComponents,
                         PORT_MARGIN, ringTop, getWidth() - PORT_MARGIN * 2,
                         getHeight() - ringTop - 4, unfoldGridW, unfoldGridH);
  }

  if (customControls != nullptr) {
    customControls->setBounds(compactBody.reduced(PORT_MARGIN, 4));
  }
}

void NodeBlock::mouseDown(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  // Shift-click: toggle this node in/out of the multi-selection
  if (e.mods.isShiftDown() && !e.mods.isRightButtonDown()) {
    if (onAddToSelection) {
      onAddToSelection();
    }
    return;
  }

  // Tab bar click handling
  {
    auto layout = targetNode->getLayout();
    auto pos = e.getPosition();

    auto hitTabBar = [&](juce::Rectangle<int> tabRect,
                         const juce::StringArray &labels) -> int {
      if (labels.size() <= 1 || !tabRect.contains(pos)) {
        return -1;
      }
      int n = labels.size();
      int tabW = tabRect.getWidth() / n;
      int relX = pos.x - tabRect.getX();
      int t = juce::jlimit(0, n - 1, relX / (tabW > 0 ? tabW : 1));
      return t;
    };

    if (!isExpanded && layout.hasCompactTabs()) {
      auto compactBody = compactBodyRect();
      compactBody.removeFromTop(HEADER_HEIGHT);
      auto tabRect = compactBody.removeFromTop(TAB_HEIGHT);
      int t = hitTabBar(tabRect, layout.compactTabLabels);
      if (t >= 0) {
        activeCompactTab = t;
        updateTabVisibility();
        repaint();
        return;
      }
    }

    if (isExpanded && layout.hasUnfoldedTabs()) {
      auto area = getLocalBounds();
      area.removeFromTop(HEADER_HEIGHT);
      auto tabRect = area.removeFromTop(TAB_HEIGHT);
      int t = hitTabBar(tabRect, layout.unfoldedTabLabels);
      if (t >= 0) {
        activeUnfoldedTab = t;
        updateTabVisibility();
        repaint();
        return;
      }
    }
  }

  // If this node is already in a multi-selection, defer the selection change.
  // We need to start the group drag on first mouse movement without clearing
  // the selection here, so we set a flag and return early.
  if (parentCanvas.isNodeSelected(targetNode.get()) &&
      parentCanvas.getSelectionSize() > 1) {
    wasInGroupSelection = true;
    dragStartGridX = targetNode->gridX;
    dragStartGridY = targetNode->gridY;
    dragStartWorldX = targetNode->nodeX;
    dragStartWorldY = targetNode->nodeY;
    return;
  }
  wasInGroupSelection = false;

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

  // Start dragging the node
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

  // Lazily start the group drag on first movement after mouseDown saw a
  // multi-selected node (we deferred it to avoid calling onSelected first).
  if (wasInGroupSelection) {
    wasInGroupSelection = false;
    parentCanvas.beginGroupDrag(targetNode.get(), e);
  }

  // Delegate to canvas for group drags
  if (parentCanvas.isGroupDragging()) {
    parentCanvas.updateGroupDrag(e);
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

    // For a clone drag the original stays in place, so its own cells ARE
    // occupied and should block placement. For a move drag the node is
    // vacating its spot, so we ignore it in the collision check.
    const bool isCloneDrag = e.mods.isCtrlDown();
    GraphNode *ghostIgnoreNode = isCloneDrag ? nullptr : targetNode.get();

    if (newGridX != parentCanvas.getGhostX() ||
        newGridY != parentCanvas.getGhostY()) {
      parentCanvas.setGhostTarget(newGridX, newGridY,
                                  targetNode->getGridWidth(),
                                  targetNode->getGridHeight(), ghostIgnoreNode);
    }

    // When Ctrl is held the drag is a clone: the original stays in place and
    // only the ghost target shows where the copy will land. Actively reset to
    // the start position each frame so that pressing Ctrl mid-drag snaps the
    // block back immediately rather than freezing in a half-moved state.
    if (isCloneDrag) {
      targetNode->nodeX = dragStartWorldX;
      targetNode->nodeY = dragStartWorldY;
    } else {
      targetNode->nodeX =
          (static_cast<float>(dragStartGridX) * Layout::GridPitchFloat) +
          Layout::TramlineOffset + static_cast<float>(deltaX);
      targetNode->nodeY =
          (static_cast<float>(dragStartGridY) * Layout::GridPitchFloat) +
          Layout::TramlineOffset + static_cast<float>(deltaY);
    }

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

  // Delegate to canvas for group drags
  if (parentCanvas.isGroupDragging()) {
    parentCanvas.commitGroupDrag(e.mods.isCtrlDown());
    return;
  }

  // Plain click on a node that was in a group selection (no drag occurred):
  // now single-select it.
  if (wasInGroupSelection) {
    wasInGroupSelection = false;
    if (onSelected) {
      onSelected();
    }
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
    int finalGridX = 0;
    int finalGridY = 0;

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
          targetNode->syncWorldFromGrid();
        });
      } else {
        targetNode->gridX = finalGridX;
        targetNode->gridY = finalGridY;
        targetNode->syncWorldFromGrid();
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

juce::Rectangle<int> NodeBlock::getInputPortRect(int portIndex) const {
  bool unfolded = isExpanded && targetNode->getLayout().hasUnfoldedLayout();
  auto body = unfolded ? getLocalBounds() : compactBodyRect();
  int y = body.getY() + HEADER_HEIGHT + 10 + (portIndex * PORT_SPACING);
  return {body.getX(), y - PORT_RADIUS, PORT_RADIUS, PORT_RADIUS * 2};
}

juce::Rectangle<int> NodeBlock::getOutputPortRect(int portIndex) const {
  bool unfolded = isExpanded && targetNode->getLayout().hasUnfoldedLayout();
  auto body = unfolded ? getLocalBounds() : compactBodyRect();
  int y = body.getY() + HEADER_HEIGHT + 10 + (portIndex * PORT_SPACING);
  return {body.getRight() - PORT_RADIUS, y - PORT_RADIUS, PORT_RADIUS,
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
