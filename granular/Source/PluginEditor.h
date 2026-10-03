#pragma once

#include "PluginProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <functional>
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

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// Live display: the 4-second recording with every playing grain, and an output spectrum
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

// Two-axis morph pad: drag the dot to push two chosen controls at once
class XYPad : public juce::Component, private juce::Timer
{
public:
    explicit XYPad (GrainProcessor&);
    ~XYPad() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setFromMouse (juce::Point<float>);
    juce::Rectangle<float> padArea() const;

    GrainProcessor& proc;
    juce::RangedAudioParameter* px;
    juce::RangedAudioParameter* py;
    float lastX = -1.0f, lastY = -1.0f;
    juce::String lastLabels;
};

// Everything is laid out at a fixed size on this canvas, which is then scaled to the window
class Canvas : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter;
    void paint (juce::Graphics& g) override { if (painter) painter (g); }
};

class GrainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GrainEditor (GrainProcessor&);
    ~GrainEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void updateModulationDots();

    static constexpr int baseWidth = 1100;
    static constexpr int baseHeight = 880;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        int dest = -1;   // modulation destination shown on this knob
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

    void timerCallback() override;
    void paintCanvas (juce::Graphics&);
    void addKnob (Group&, const juce::String& paramId, const juce::String& name);
    void addChoice (Group&, const juce::String& paramId, const juce::String& name);
    void layoutRow (juce::Rectangle<int> row, std::initializer_list<int> groupIndexes);
    void layoutGroup (Group&);

    GrainProcessor& proc;
    GrainLookAndFeel lnf;   // declared first so it outlives the controls

    Canvas canvas;
    Visualizer visualizer;
    XYPad pad;
    juce::Label padXLabel, padYLabel;
    juce::ComboBox padXBox, padYBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> padXAttachment, padYAttachment;

    juce::ComboBox presetBox;
    juce::TextButton freezeButton { "Freeze" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<Group> groups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainEditor)
};
