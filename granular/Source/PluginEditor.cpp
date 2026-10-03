#include "PluginEditor.h"

namespace
{
const juce::Colour kInk   (0xff000000);
const juce::Colour kGrey  (0xff6b6b6b);
const juce::Colour kLine  (0xffe2e2e2);
const juce::Colour kPaper (0xffffffff);

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
GrainLookAndFeel::GrainLookAndFeel()
{
   #if JUCE_MAC || JUCE_IOS
    setDefaultSansSerifTypefaceName ("Helvetica Neue");
   #else
    setDefaultSansSerifTypefaceName ("Arial");   // Windows has no Helvetica; Arial matches its shapes
   #endif

    setColour (juce::ResizableWindow::backgroundColourId, kPaper);
    setColour (juce::Label::textColourId, kInk);

    setColour (juce::Slider::textBoxTextColourId, kGrey);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentWhite);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentWhite);
    setColour (juce::Slider::textBoxHighlightColourId, kLine);

    setColour (juce::TextEditor::backgroundColourId, kPaper);
    setColour (juce::TextEditor::textColourId, kInk);
    setColour (juce::TextEditor::highlightColourId, kLine);
    setColour (juce::TextEditor::outlineColourId, kLine);
    setColour (juce::TextEditor::focusedOutlineColourId, kInk);
    setColour (juce::CaretComponent::caretColourId, kInk);

    setColour (juce::ComboBox::backgroundColourId, kPaper);
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::ComboBox::outlineColourId, kInk);
    setColour (juce::ComboBox::arrowColourId, kInk);
    setColour (juce::ComboBox::buttonColourId, kPaper);
    setColour (juce::ComboBox::focusedOutlineColourId, kInk);

    setColour (juce::PopupMenu::backgroundColourId, kPaper);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kInk);
    setColour (juce::PopupMenu::highlightedTextColourId, kPaper);

    setColour (juce::TextButton::buttonColourId, kPaper);
    setColour (juce::TextButton::buttonOnColourId, kInk);
    setColour (juce::TextButton::textColourOffId, kInk);
    setColour (juce::TextButton::textColourOnId, kPaper);
}

void GrainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = 3.5f;
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
        g.setColour (kInk);
        g.strokePath (value, stroke);
    }

    const float knobR = arcR * 0.64f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);
    g.setColour (kPaper);
    g.fillEllipse (knob);
    g.setColour (kInk);
    g.drawEllipse (knob, 1.2f);
    g.drawLine (juce::Line<float> (centre.getPointOnCircumference (knobR * 0.25f, angle),
                                   centre.getPointOnCircumference (knobR * 0.8f, angle)), 2.2f);
}

void GrainLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool isHighlighted, bool isDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;

    if (button.getToggleState())
    {
        g.setColour (kInk);
        g.fillRoundedRectangle (r, corner);
    }
    else
    {
        g.setColour (isDown ? kLine : (isHighlighted ? juce::Colour (0xfff4f4f4) : kPaper));
        g.fillRoundedRectangle (r, corner);
        g.setColour (kInk);
        g.drawRoundedRectangle (r, corner, 1.2f);
    }
}

//==============================================================================
Visualizer::Visualizer (GrainProcessor& p) : proc (p)
{
    spectrum.fill (-100.0f);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
    setOpaque (false);
    startTimerHz (30);
}

void Visualizer::refresh()
{
    int start1, size1, start2, size2;
    const int ready = proc.scopeFifo.getNumReady();
    proc.scopeFifo.prepareToRead (ready, start1, size1, start2, size2);

    auto push = [this] (float v)
    {
        ring[(size_t) ringPos] = v;
        if (++ringPos >= fftSize) ringPos = 0;
    };
    for (int i = 0; i < size1; ++i) push (proc.scopeData[(size_t) (start1 + i)]);
    for (int i = 0; i < size2; ++i) push (proc.scopeData[(size_t) (start2 + i)]);
    proc.scopeFifo.finishedRead (size1 + size2);

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)] * window[(size_t) i];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float scale = 4.0f / (float) fftSize;   // a full-scale sine reads 0 dB
    for (int b = 0; b < fftSize / 2; ++b)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * scale, -100.0f);
        auto& s = spectrum[(size_t) b];
        s = juce::jmax (db, s - 2.5f);   // fall back slowly so peaks are readable
    }
}

void Visualizer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (kInk);
    g.fillRoundedRectangle (bounds, 14.0f);

    auto inner = bounds.reduced (18.0f, 14.0f);
    auto labelRow = inner.removeFromTop (16.0f);
    auto top = inner.removeFromTop (inner.getHeight() * 0.6f);
    inner.removeFromTop (10.0f);
    auto bottom = inner;

    const juce::Colour white = juce::Colours::white;

    // ---- Grain buffer (left = 4 s ago, right = now) ----
    g.setFont (makeFont (11.5f, false));
    g.setColour (white.withAlpha (0.55f));
    g.drawText ("4 s ago", labelRow, juce::Justification::centredLeft);
    g.drawText (proc.apvts.getRawParameterValue ("freeze")->load() > 0.5f ? "Frozen" : "Now",
                labelRow, juce::Justification::centredRight);

    const float bufSec = (float) GrainProcessor::bufferSeconds;
    auto xForSeconds = [&] (float secAgo) { return top.getRight() - (secAgo / bufSec) * top.getWidth(); };

    // Where new grains can start: Position, extended by Spray
    {
        const float posSec   = proc.apvts.getRawParameterValue ("position")->load() * 0.001f;
        const float spraySec = proc.apvts.getRawParameterValue ("spray")->load() * 0.001f;
        const float x1 = xForSeconds (juce::jmin (bufSec, posSec + spraySec));
        const float x2 = xForSeconds (posSec);
        g.setColour (white.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (x1, top.getY(), juce::jmax (2.0f, x2 - x1), top.getHeight()));
    }

    const float mid = top.getCentreY();
    const int head = proc.headColumn.load (std::memory_order_relaxed);
    const int n = GrainProcessor::numColumns;
    const float colW = top.getWidth() / (float) n;

    g.setColour (white.withAlpha (0.6f));
    for (int j = 0; j < n; ++j)
    {
        const int idx = (head + 1 + j) % n;
        const float v = juce::jmin (1.0f, std::sqrt (proc.columnPeaks[(size_t) idx].load (std::memory_order_relaxed)));
        const float h = juce::jmax (0.5f, v * top.getHeight() * 0.48f);
        g.fillRect (top.getX() + (float) j * colW, mid - h, juce::jmax (1.0f, colW - 0.6f), h * 2.0f);
    }

    // Playing grains
    for (int k = 0; k < GrainProcessor::maxGrains; ++k)
    {
        const float p = proc.grainPos[(size_t) k].load (std::memory_order_relaxed);
        if (p < 0.0f) continue;
        const float lvl = proc.grainLevel[(size_t) k].load (std::memory_order_relaxed);
        const float x = top.getRight() - p * top.getWidth();

        g.setColour (white.withAlpha (0.15f + 0.5f * lvl));
        g.drawLine (x, top.getY() + 4.0f, x, top.getBottom() - 4.0f, 1.0f);

        const float r = 2.5f + 4.5f * lvl;
        g.setColour (white);
        g.fillEllipse (x - r, mid - r, r * 2.0f, r * 2.0f);
        g.setColour (kInk);
        g.drawEllipse (x - r, mid - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    // ---- Output spectrum (20 Hz to 20 kHz, -90 to 0 dB) ----
    g.setColour (white.withAlpha (0.18f));
    g.drawHorizontalLine ((int) bottom.getY() - 5, bottom.getX(), bottom.getRight());

    const double sampleRate = juce::jmax (8000.0, proc.getSampleRate());
    auto yForDb = [&] (float db) { return juce::jmap (juce::jlimit (-90.0f, 0.0f, db), -90.0f, 0.0f, bottom.getBottom(), bottom.getY()); };
    auto freqForX = [&] (float x) { return 20.0f * std::pow (1000.0f, (x - bottom.getX()) / bottom.getWidth()); };

    juce::Path curve;
    bool firstPoint = true;
    const float step = 2.0f;
    for (float x = bottom.getX(); x <= bottom.getRight(); x += step)
    {
        const float fb1 = (float) (freqForX (x) * fftSize / sampleRate);
        const float fb2 = (float) (freqForX (x + step) * fftSize / sampleRate);
        float db;
        if (fb2 - fb1 < 1.0f)
        {
            // Low end: less than one FFT bin per pixel, so blend neighbouring bins for a smooth curve
            const int b = juce::jlimit (1, fftSize / 2 - 2, (int) fb1);
            const float t = juce::jlimit (0.0f, 1.0f, fb1 - (float) b);
            db = spectrum[(size_t) b] + t * (spectrum[(size_t) b + 1] - spectrum[(size_t) b]);
        }
        else
        {
            // High end: many bins per pixel, so show the loudest
            const int b1 = juce::jlimit (1, fftSize / 2 - 1, (int) fb1);
            const int b2 = juce::jlimit (b1, fftSize / 2 - 1, (int) fb2);
            db = -100.0f;
            for (int b = b1; b <= b2; ++b) db = juce::jmax (db, spectrum[(size_t) b]);
        }

        const float y = yForDb (db);
        if (firstPoint) { curve.startNewSubPath (x, y); firstPoint = false; }
        else curve.lineTo (x, y);
    }

    juce::Path fill (curve);
    fill.lineTo (bottom.getRight(), bottom.getBottom());
    fill.lineTo (bottom.getX(), bottom.getBottom());
    fill.closeSubPath();
    g.setColour (white.withAlpha (0.14f));
    g.fillPath (fill);
    g.setColour (white);
    g.strokePath (curve, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));

    g.setFont (makeFont (10.5f, false));
    g.setColour (white.withAlpha (0.45f));
    for (const float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = bottom.getX() + bottom.getWidth() * std::log10 (f / 20.0f) / 3.0f;
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x - 20.0f, bottom.getY(), 40.0f, 14.0f), juce::Justification::centred);
    }
}

//==============================================================================
GrainEditor::GrainEditor (GrainProcessor& p)
    : AudioProcessorEditor (&p), proc (p), visualizer (p)
{
    setLookAndFeel (&lnf);

    visualizer.setComponentID ("visualizer");
    addAndMakeVisible (visualizer);

    freezeButton.setClickingTogglesState (true);
    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "freeze", freezeButton);

    Group grains   { "Grains", {}, {} };
    addKnob (grains, "size", "Size");
    addKnob (grains, "density", "Density");
    addKnob (grains, "position", "Position");
    addKnob (grains, "spray", "Spray");
    addKnob (grains, "feedback", "Feedback");

    Group pitch    { "Pitch", {}, {} };
    addKnob (pitch, "pitch", "Pitch");
    addKnob (pitch, "pitchrand", "Random");
    addKnob (pitch, "reverse", "Reverse");
    addKnob (pitch, "spread", "Spread");

    Group distort  { "Distortion", {}, {} };
    addChoice (distort, "type", "Type");
    addKnob (distort, "drive", "Drive");
    addKnob (distort, "tone", "Tone");

    Group output   { "Output", {}, {} };
    addKnob (output, "mix", "Mix");
    addKnob (output, "output", "Gain");

    groups.push_back (std::move (grains));
    groups.push_back (std::move (pitch));
    groups.push_back (std::move (distort));
    groups.push_back (std::move (output));

    sendLookAndFeelChange();   // push the black and white colours into every control's value box
    setSize (880, 660);
}

GrainEditor::~GrainEditor()
{
    setLookAndFeel (nullptr);
}

void GrainEditor::addKnob (Group& group, const juce::String& paramId, const juce::String& name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 18);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (makeFont (13.0f, true));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);

    group.items.push_back ({ &k->slider, &k->label, false });
    knobs.push_back (std::move (k));
}

void GrainEditor::addChoice (Group& group, const juce::String& paramId, const juce::String& name)
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
void GrainEditor::paint (juce::Graphics& g)
{
    g.fillAll (kPaper);

    g.setColour (kInk);
    g.setFont (makeFont (28.0f, true));
    g.drawText ("GRAINFORFEIT", juce::Rectangle<int> (20, 18, 400, 34), juce::Justification::centredLeft);

    g.setColour (kGrey);
    g.setFont (makeFont (13.0f, false));
    g.drawText ("Granular sample distortion", juce::Rectangle<int> (21, 50, 400, 18), juce::Justification::centredLeft);

    for (const auto& grp : groups)
    {
        auto title = grp.area.withHeight (20);
        g.setColour (kInk);
        g.setFont (makeFont (13.0f, true));
        g.drawText (grp.title, title, juce::Justification::centredLeft);
        g.setColour (kLine);
        g.fillRect (grp.area.getX(), title.getBottom() + 4, grp.area.getWidth(), 1);
    }
}

void GrainEditor::layoutGroup (Group& grp)
{
    auto inner = grp.area;
    inner.removeFromTop (34);   // title and rule
    const int n = (int) grp.items.size();
    if (n == 0) return;
    const int cellW = inner.getWidth() / n;

    for (int i = 0; i < n; ++i)
    {
        auto cell = (i == n - 1) ? inner : inner.removeFromLeft (cellW);
        auto& item = grp.items[(size_t) i];
        item.label->setBounds (cell.removeFromTop (18));

        if (item.isChoice)
            item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 12, 120), 30));
        else
            item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 92), cell.getHeight()));
    }
}

void GrainEditor::resized()
{
    auto b = getLocalBounds().reduced (20);

    auto header = b.removeFromTop (52);
    freezeButton.setBounds (header.removeFromRight (120).withSizeKeepingCentre (120, 34));

    b.removeFromTop (12);
    visualizer.setBounds (b.removeFromTop (232));
    b.removeFromTop (20);

    if (groups.size() < 4) return;

    const int gap = 28, rowH = 146;

    auto row1 = b.removeFromTop (rowH);
    groups[0].area = row1.removeFromLeft ((row1.getWidth() - gap) * 5 / 9);
    row1.removeFromLeft (gap);
    groups[1].area = row1;

    b.removeFromTop (16);
    auto row2 = b.removeFromTop (rowH);
    groups[2].area = row2.removeFromLeft ((row2.getWidth() - gap) * 3 / 5);
    row2.removeFromLeft (gap);
    groups[3].area = row2;

    for (auto& grp : groups)
        layoutGroup (grp);
}
