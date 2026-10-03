#pragma once

#include "PluginProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

// Black and white styling with Helvetica type
class GrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GrainLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
};

// Live display: the 4-second grain buffer with every playing grain, and an output spectrum
class Visualizer : public juce::Component, private juce::Timer
{
public:
    explicit Visualizer (GrainProcessor&);
    ~Visualizer() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void refresh();   // pulls new audio from the processor

private:
    void timerCallback() override { refresh(); repaint(); }

    GrainProcessor& proc;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> ring {};
    int ringPos = 0;
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> spectrum {};
    std::array<float, fftSize> window {};
};

class GrainEditor : public juce::AudioProcessorEditor
{
public:
    explicit GrainEditor (GrainProcessor&);
    ~GrainEditor() override;

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

    void addKnob (Group&, const juce::String& paramId, const juce::String& name);
    void addChoice (Group&, const juce::String& paramId, const juce::String& name);
    void layoutGroup (Group&);

    GrainProcessor& proc;
    GrainLookAndFeel lnf;   // declared first so it outlives the controls

    Visualizer visualizer;
    juce::TextButton freezeButton { "Freeze" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<Group> groups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainEditor)
};
