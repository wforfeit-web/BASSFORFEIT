#pragma once

#include "PluginProcessor.h"
#include <vector>
#include <memory>

// White and blue styling, Helvetica type, ring-style knobs
class BffLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BffLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};

class BassforfeitEditor : public juce::AudioProcessorEditor
{
public:
    explicit BassforfeitEditor (BassforfeitProcessor&);
    ~BassforfeitEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Item
    {
        juce::Component* control;
        juce::Label* label;
        bool isChoice;
    };

    struct Group
    {
        juce::String title;
        std::vector<Item> items;
        juce::Rectangle<int> area;
    };

    void addKnob (Group& group, const juce::String& paramId, const juce::String& name);
    void addChoice (Group& group, const juce::String& paramId, const juce::String& name);

    BassforfeitProcessor& proc;
    BffLookAndFeel lnf; // declared before the controls so it outlives them

    juce::Label presetLabel;
    juce::ComboBox presetBox;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<Group> groups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassforfeitEditor)
};
