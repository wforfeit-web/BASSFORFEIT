#include "PluginEditor.h"

namespace
{
const juce::Colour kInk   (0xff1f2a44);
const juce::Colour kMuted (0xff5f6b8a);
const juce::Colour kLine  (0xffdbe4f3);
const juce::Colour kBlue  (0xff2f6fd6);
const juce::Colour kSky   (0xffeaf2fe);

juce::Font makeFont (float height, bool bold)
{
   #if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
   #else
    return juce::Font (height, bold ? juce::Font::bold : juce::Font::plain);
   #endif
}
} // namespace

//==============================================================================
BffLookAndFeel::BffLookAndFeel()
{
   #if JUCE_MAC || JUCE_IOS
    setDefaultSansSerifTypefaceName ("Helvetica Neue");
   #else
    setDefaultSansSerifTypefaceName ("Arial"); // Windows has no Helvetica; Arial is its metric twin
   #endif

    setColour (juce::ResizableWindow::backgroundColourId, juce::Colours::white);
    setColour (juce::Label::textColourId, kInk);

    setColour (juce::Slider::textBoxTextColourId, kMuted);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentWhite);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentWhite);
    setColour (juce::Slider::textBoxHighlightColourId, kSky);

    setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
    setColour (juce::TextEditor::textColourId, kInk);
    setColour (juce::TextEditor::highlightColourId, kSky);
    setColour (juce::TextEditor::outlineColourId, kLine);
    setColour (juce::TextEditor::focusedOutlineColourId, kBlue);
    setColour (juce::CaretComponent::caretColourId, kInk);

    setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::ComboBox::outlineColourId, kLine);
    setColour (juce::ComboBox::arrowColourId, kBlue);
    setColour (juce::ComboBox::buttonColourId, juce::Colours::white);
    setColour (juce::ComboBox::focusedOutlineColourId, kBlue);

    setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kBlue);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void BffLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = 4.0f;
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (kLine);
    g.strokePath (track, stroke);

    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (kBlue);
        g.strokePath (value, stroke);
    }

    const float knobR = arcR * 0.62f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::white);
    g.fillEllipse (knob);
    g.setColour (kLine);
    g.drawEllipse (knob, 1.0f);

    const auto tip = centre.getPointOnCircumference (knobR * 0.8f, angle);
    g.setColour (kBlue);
    g.drawLine (juce::Line<float> (centre, tip), 2.5f);
}

//==============================================================================
BassforfeitEditor::BassforfeitEditor (BassforfeitProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetLabel.setColour (juce::Label::textColourId, kMuted);
    addAndMakeVisible (presetLabel);

    presetBox.addItemList (BassforfeitProcessor::presetNames(), 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0) proc.setCurrentProgram (index);
    };
    addAndMakeVisible (presetBox);

    Group osc    { "Oscillators", {}, {} };
    addChoice (osc, "osc1", "Osc 1");
    addChoice (osc, "osc2", "Osc 2");
    addKnob   (osc, "detune", "Detune");
    addKnob   (osc, "sub", "Sub");

    Group filt   { "Filter", {}, {} };
    addKnob (filt, "cutoff", "Cutoff");
    addKnob (filt, "res", "Resonance");
    addKnob (filt, "drive", "Drive");

    Group wobble { "Wobble", {}, {} };
    addChoice (wobble, "rate", "Rate");
    addChoice (wobble, "shape", "Shape");
    addKnob   (wobble, "depth", "Depth");

    Group scream { "Scream", {}, {} };
    addKnob (scream, "scream", "Scream");
    addKnob (scream, "vowel", "Vowel");
    addKnob (scream, "talk", "Talk");
    addKnob (scream, "bite", "Bite");

    Group amp    { "Amp", {}, {} };
    addKnob (amp, "attack", "Attack");
    addKnob (amp, "release", "Release");
    addKnob (amp, "glide", "Glide");
    addKnob (amp, "volume", "Volume");

    groups.push_back (std::move (osc));
    groups.push_back (std::move (filt));
    groups.push_back (std::move (wobble));
    groups.push_back (std::move (scream));
    groups.push_back (std::move (amp));

    sendLookAndFeelChange();   // apply the white and blue colours to every knob's value readout
    setSize (820, 544);
}

BassforfeitEditor::~BassforfeitEditor()
{
    setLookAndFeel (nullptr);
}

void BassforfeitEditor::addKnob (Group& group, const juce::String& paramId, const juce::String& name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (makeFont (13.0f, true));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);

    group.items.push_back ({ &k->slider, &k->label, false });
    knobs.push_back (std::move (k));
}

void BassforfeitEditor::addChoice (Group& group, const juce::String& paramId, const juce::String& name)
{
    auto c = std::make_unique<Choice>();
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c->box.addItemList (param->choices, 1);
    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (makeFont (13.0f, true));
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c->box);

    group.items.push_back ({ &c->box, &c->label, true });
    choices.push_back (std::move (c));
}

//==============================================================================
void BassforfeitEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);

    // Soft sky-blue light across the top
    const float w = (float) getWidth();
    g.setGradientFill (juce::ColourGradient (kSky, w * 0.5f, 0.0f, juce::Colours::white, w * 0.5f, 190.0f, false));
    g.fillRect (getLocalBounds().removeFromTop (190));

    g.setColour (kInk);
    g.setFont (makeFont (30.0f, true));
    g.drawText ("BASSFORFEIT", juce::Rectangle<int> (16, 14, 420, 36), juce::Justification::centredLeft);

    g.setColour (kMuted);
    g.setFont (makeFont (13.0f, false));
    g.drawText ("Mono bass synth with a tempo-synced wobble and scream",
                juce::Rectangle<int> (17, 48, 440, 20), juce::Justification::centredLeft);

    for (const auto& grp : groups)
    {
        const auto r = grp.area.toFloat();
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

        g.setColour (kInk);
        g.setFont (makeFont (14.0f, true));
        g.drawText (grp.title, grp.area.reduced (10).removeFromTop (22), juce::Justification::centred);
    }
}

void BassforfeitEditor::resized()
{
    auto b = getLocalBounds().reduced (16);

    auto header = b.removeFromTop (56);
    presetBox.setBounds (header.removeFromRight (190).withSizeKeepingCentre (190, 30));
    presetLabel.setBounds (header.removeFromRight (64).withSizeKeepingCentre (64, 30));

    b.removeFromTop (12);
    const int rowH = 140, gap = 12;

    if (groups.size() < 5) return;

    auto row1 = b.removeFromTop (rowH);
    groups[0].area = row1.removeFromLeft (440);
    row1.removeFromLeft (gap);
    groups[1].area = row1;

    b.removeFromTop (gap);
    auto row2 = b.removeFromTop (rowH);
    groups[2].area = row2.removeFromLeft (360);
    row2.removeFromLeft (gap);
    groups[3].area = row2;

    b.removeFromTop (gap);
    groups[4].area = b.removeFromTop (rowH);

    for (auto& grp : groups)
    {
        auto inner = grp.area.reduced (10);
        inner.removeFromTop (24); // group title
        const int n = (int) grp.items.size();
        if (n == 0) continue;
        const int cellW = inner.getWidth() / n;

        for (int i = 0; i < n; ++i)
        {
            auto cell = (i == n - 1) ? inner : inner.removeFromLeft (cellW);
            auto& item = grp.items[(size_t) i];
            item.label->setBounds (cell.removeFromTop (18));

            if (item.isChoice)
                item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 8, 120), 28));
            else
                item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 90), cell.getHeight()));
        }
    }
}
