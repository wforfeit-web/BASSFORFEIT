#pragma once

#include "PluginProcessor.h"
#include <functional>
#include <memory>
#include <vector>

// Black and white styling with Helvetica type (matches GRAINFORFEIT)
class TalkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TalkLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

// Live display: the words with the one being spoken highlighted, its phonemes,
// the output waveform, and where the voice sits in vowel space
class TalkDisplay : public juce::Component, private juce::Timer
{
public:
    explicit TalkDisplay (TalkProcessor&);
    ~TalkDisplay() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void refresh();

private:
    void timerCallback() override { refresh(); repaint(); }

    TalkProcessor& proc;
    std::array<float, 2048> ring {};
    int ringPos = 0;
    struct TrailPoint { float f1, f2, level; };
    std::vector<TrailPoint> trail;
};

// Everything is laid out at a fixed size on this canvas, which is then scaled to the window
class TalkCanvas : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter;
    void paint (juce::Graphics& g) override { if (painter) painter (g); }
};

class TalkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TalkEditor (TalkProcessor&);
    ~TalkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 1040;
    static constexpr int baseHeight = 810;

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
        int kind;   // 0 knob, 1 menu, 2 button
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
    void exportWav();
    void showStatus (const juce::String& message);

    TalkProcessor& proc;
    TalkLookAndFeel lnf;   // declared first so it outlives the controls

    TalkCanvas canvas;
    juce::TextEditor textBox;
    juce::Label hint;
    TalkDisplay display;

    juce::ComboBox voiceBox;
    juce::TextButton talkButton { "Talk" }, exportButton { "Export WAV" };

    juce::TextButton holdButton { "On" };
    juce::Label holdLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAttachment;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<Group> groups;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::uint32 lastEdit = 0;
    bool textDirty = false;
    juce::uint32 statusUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TalkEditor)
};
