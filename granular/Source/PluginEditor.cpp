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

juce::Font GrainLookAndFeel::getComboBoxFont (juce::ComboBox&) { return makeFont (13.5f, false); }
juce::Font GrainLookAndFeel::getPopupMenuFont()                { return makeFont (14.0f, false); }

void GrainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
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

    // Where an LFO or the XY pad is pushing this control right now
    const float mod = (float) (double) slider.getProperties().getWithDefault ("mod", -1.0);
    if (mod >= 0.0f)
    {
        const auto p = centre.getPointOnCircumference (arcR, startAngle + mod * (endAngle - startAngle));
        g.setColour (kPaper);
        g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour (kInk);
        g.drawEllipse (p.x - 3.6f, p.y - 3.6f, 7.2f, 7.2f, 1.6f);
    }
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
    setInterceptsMouseClicks (false, false);
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
    auto top = inner.removeFromTop (inner.getHeight() * 0.62f);
    inner.removeFromTop (10.0f);
    auto bottom = inner;

    const juce::Colour white = juce::Colours::white;

    // ---- Recording (left = 4 s ago, right = now) ----
    g.setFont (makeFont (11.5f, false));
    g.setColour (white.withAlpha (0.55f));
    g.drawText ("4 s ago", labelRow, juce::Justification::centredLeft);
    g.drawText (proc.apvts.getRawParameterValue ("freeze")->load() > 0.5f ? "Frozen" : "Now",
                labelRow, juce::Justification::centredRight);
    g.drawText ("Higher dots are higher pitched. Hollow dots play backwards.", labelRow, juce::Justification::centred);

    const float bufSec = (float) GrainProcessor::bufferSeconds;
    auto xForSeconds = [&] (float secAgo) { return top.getRight() - (juce::jlimit (0.0f, bufSec, secAgo) / bufSec) * top.getWidth(); };

    // Where new grains can start: Position (plus Scan), extended by Spray
    {
        const float startSec = proc.visStartSec.load (std::memory_order_relaxed);
        const float spraySec = proc.visSpraySec.load (std::memory_order_relaxed);
        const float x1 = xForSeconds (startSec + spraySec);
        const float x2 = xForSeconds (startSec);
        g.setColour (white.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float> (x1, top.getY(), juce::jmax (2.0f, x2 - x1), top.getHeight()));
        g.setColour (white.withAlpha (0.35f));
        g.drawVerticalLine ((int) x2, top.getY(), top.getBottom());
    }

    const float mid = top.getCentreY();
    const int head = proc.headColumn.load (std::memory_order_relaxed);
    const int n = GrainProcessor::numColumns;
    const float colW = top.getWidth() / (float) n;

    g.setColour (white.withAlpha (0.45f));
    for (int j = 0; j < n; ++j)
    {
        const int idx = (head + 1 + j) % n;
        const float v = juce::jmin (1.0f, std::sqrt (proc.columnPeaks[(size_t) idx].load (std::memory_order_relaxed)));
        const float h = juce::jmax (0.5f, v * top.getHeight() * 0.42f);
        g.fillRect (top.getX() + (float) j * colW, mid - h, juce::jmax (1.0f, colW - 0.6f), h * 2.0f);
    }

    // Playing grains: across = where in the recording, up/down = pitch, size = loudness
    for (int k = 0; k < GrainProcessor::maxGrains; ++k)
    {
        const float p = proc.grainPos[(size_t) k].load (std::memory_order_relaxed);
        if (p < 0.0f) continue;
        const float lvlSigned = proc.grainLevel[(size_t) k].load (std::memory_order_relaxed);
        const float lvl = std::abs (lvlSigned);
        const float semis = proc.grainPitch[(size_t) k].load (std::memory_order_relaxed);
        const float x = top.getRight() - p * top.getWidth();
        const float y = mid - juce::jlimit (-1.0f, 1.0f, semis / 36.0f) * top.getHeight() * 0.44f;

        g.setColour (white.withAlpha (0.12f + 0.4f * lvl));
        g.drawLine (x, top.getY() + 4.0f, x, top.getBottom() - 4.0f, 1.0f);

        const float r = 2.5f + 4.5f * lvl;
        if (lvlSigned >= 0.0f)
        {
            g.setColour (white);
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
            g.setColour (kInk);
            g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.0f);
        }
        else
        {
            g.setColour (kInk);
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
            g.setColour (white);
            g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.5f);
        }
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
XYPad::XYPad (GrainProcessor& p)
    : proc (p), px (p.apvts.getParameter ("padx")), py (p.apvts.getParameter ("pady"))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

juce::Rectangle<float> XYPad::padArea() const
{
    return getLocalBounds().toFloat().reduced (16.0f);
}

void XYPad::timerCallback()
{
    const float x = px->getValue(), y = py->getValue();
    const auto labels = proc.apvts.getParameter ("padxtarget")->getCurrentValueAsText()
                      + "|" + proc.apvts.getParameter ("padytarget")->getCurrentValueAsText();
    if (std::abs (x - lastX) > 0.0005f || std::abs (y - lastY) > 0.0005f || labels != lastLabels)
    {
        lastX = x;
        lastY = y;
        lastLabels = labels;
        repaint();
    }
}

void XYPad::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const juce::Colour white = juce::Colours::white;
    g.setColour (kInk);
    g.fillRoundedRectangle (bounds, 14.0f);

    const auto area = padArea();

    g.setColour (white.withAlpha (0.10f));
    for (int i = 1; i < 4; ++i)
    {
        const float fx = area.getX() + area.getWidth() * (float) i / 4.0f;
        const float fy = area.getY() + area.getHeight() * (float) i / 4.0f;
        g.drawVerticalLine ((int) fx, area.getY(), area.getBottom());
        g.drawHorizontalLine ((int) fy, area.getX(), area.getRight());
    }

    const float x = area.getX() + px->getValue() * area.getWidth();
    const float y = area.getBottom() - py->getValue() * area.getHeight();

    g.setColour (white.withAlpha (0.3f));
    g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    g.drawHorizontalLine ((int) y, area.getX(), area.getRight());

    g.setColour (white.withAlpha (0.18f));
    g.fillEllipse (x - 16.0f, y - 16.0f, 32.0f, 32.0f);
    g.setColour (white);
    g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);

    const auto xName = proc.apvts.getParameter ("padxtarget")->getCurrentValueAsText();
    const auto yName = proc.apvts.getParameter ("padytarget")->getCurrentValueAsText();
    g.setFont (makeFont (11.5f, false));
    g.setColour (white.withAlpha (0.6f));
    g.drawText ("X  " + xName, bounds.reduced (14.0f, 10.0f), juce::Justification::bottomRight);
    g.drawText ("Y  " + yName, bounds.reduced (14.0f, 10.0f), juce::Justification::topLeft);
}

void XYPad::setFromMouse (juce::Point<float> p)
{
    const auto area = padArea();
    const float nx = juce::jlimit (0.0f, 1.0f, (p.x - area.getX()) / area.getWidth());
    const float ny = juce::jlimit (0.0f, 1.0f, (area.getBottom() - p.y) / area.getHeight());
    px->setValueNotifyingHost (nx);
    py->setValueNotifyingHost (ny);
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    px->beginChangeGesture();
    py->beginChangeGesture();
    setFromMouse (e.position);
}

void XYPad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void XYPad::mouseUp (const juce::MouseEvent&)
{
    px->endChangeGesture();
    py->endChangeGesture();
}

void XYPad::mouseDoubleClick (const juce::MouseEvent&)
{
    px->beginChangeGesture();
    py->beginChangeGesture();
    px->setValueNotifyingHost (0.5f);
    py->setValueNotifyingHost (0.5f);
    px->endChangeGesture();
    py->endChangeGesture();
}

//==============================================================================
GrainEditor::GrainEditor (GrainProcessor& p)
    : AudioProcessorEditor (&p), proc (p), visualizer (p), pad (p)
{
    setLookAndFeel (&lnf);

    canvas.painter = [this] (juce::Graphics& g) { paintCanvas (g); };
    addAndMakeVisible (canvas);

    visualizer.setComponentID ("visualizer");
    canvas.addAndMakeVisible (visualizer);
    canvas.addAndMakeVisible (pad);

    for (auto* l : { &padXLabel, &padYLabel })
    {
        l->setFont (makeFont (13.0f, true));
        l->setJustificationType (juce::Justification::centredRight);
        canvas.addAndMakeVisible (*l);
    }
    padXLabel.setText ("X", juce::dontSendNotification);
    padYLabel.setText ("Y", juce::dontSendNotification);
    for (auto* b : { &padXBox, &padYBox })
    {
        b->addItemList (GrainProcessor::destNames(), 1);
        canvas.addAndMakeVisible (*b);
    }
    padXAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "padxtarget", padXBox);
    padYAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "padytarget", padYBox);

    presetBox.addItemList (GrainProcessor::presetNames(), 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0) proc.setCurrentProgram (index);
    };
    canvas.addAndMakeVisible (presetBox);

    freezeButton.setClickingTogglesState (true);
    canvas.addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "freeze", freezeButton);

    Group grains   { "Grains", {}, {} };
    addKnob   (grains, "size", "Size");
    addKnob   (grains, "density", "Density");
    addChoice (grains, "rhythm", "Rhythm");
    addKnob   (grains, "position", "Position");
    addKnob   (grains, "spray", "Spray");
    addKnob   (grains, "scan", "Scan");
    addKnob   (grains, "shape", "Shape");
    addKnob   (grains, "feedback", "Feedback");

    Group pitch    { "Pitch", {}, {} };
    addKnob   (pitch, "pitch", "Pitch");
    addKnob   (pitch, "pitchrand", "Random");
    addChoice (pitch, "scale", "Scale");
    addKnob   (pitch, "reverse", "Reverse");
    addKnob   (pitch, "spread", "Spread");

    Group distort  { "Distortion", {}, {} };
    addChoice (distort, "type", "Type");
    addKnob   (distort, "drive", "Drive");

    Group filter   { "Filter", {}, {} };
    addChoice (filter, "fmode", "Mode");
    addKnob   (filter, "cutoff", "Cutoff");
    addKnob   (filter, "reso", "Resonance");

    Group space    { "Space", {}, {} };
    addKnob (space, "dtime", "Delay");
    addKnob (space, "dfeedback", "Repeats");
    addKnob (space, "echo", "Echo");
    addKnob (space, "room", "Room");
    addKnob (space, "reverb", "Reverb");

    Group output   { "Output", {}, {} };
    addKnob (output, "mix", "Mix");
    addKnob (output, "output", "Gain");

    Group lfo1     { "LFO 1", {}, {} };
    addChoice (lfo1, "l1shape", "Wave");
    addKnob   (lfo1, "l1rate", "Rate");
    addKnob   (lfo1, "l1depth", "Depth");
    addChoice (lfo1, "l1target", "Target");

    Group lfo2     { "LFO 2", {}, {} };
    addChoice (lfo2, "l2shape", "Wave");
    addKnob   (lfo2, "l2rate", "Rate");
    addKnob   (lfo2, "l2depth", "Depth");
    addChoice (lfo2, "l2target", "Target");

    for (auto* grp : { &grains, &pitch, &distort, &filter, &space, &output, &lfo1, &lfo2 })
        groups.push_back (std::move (*grp));

    sendLookAndFeelChange();   // push the black and white colours into every control

    setResizable (true, true);
    setResizeLimits ((int) (baseWidth * 0.6), (int) (baseHeight * 0.6), (int) (baseWidth * 1.5), (int) (baseHeight * 1.5));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (juce::roundToInt (baseWidth * 0.85), juce::roundToInt (baseHeight * 0.85));

    startTimerHz (30);
}

GrainEditor::~GrainEditor()
{
    stopTimer();
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
    canvas.addAndMakeVisible (k->slider);
    canvas.addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);

    for (int d = 1; d < GrainProcessor::numDest; ++d)
        if (paramId == GrainProcessor::paramId (GrainProcessor::destParam (d)))
            k->dest = d;

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
    canvas.addAndMakeVisible (c->box);
    canvas.addAndMakeVisible (c->label);
    c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c->box);

    group.items.push_back ({ &c->box, &c->label, true });
    choices.push_back (std::move (c));
}

void GrainEditor::timerCallback()
{
    updateModulationDots();
}

void GrainEditor::updateModulationDots()
{
    // Show live modulation as a dot on each moved knob's ring
    for (auto& k : knobs)
    {
        if (k->dest < 0) continue;
        const float v = proc.modDisplay[(size_t) k->dest].load (std::memory_order_relaxed);
        const float prev = (float) (double) k->slider.getProperties().getWithDefault ("mod", -1.0);
        if (std::abs (v - prev) > 0.002f)
        {
            k->slider.getProperties().set ("mod", (double) v);
            k->slider.repaint();
        }
    }

    const int program = proc.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != program)
        presetBox.setSelectedId (program, juce::dontSendNotification);
}

//==============================================================================
void GrainEditor::paint (juce::Graphics& g)
{
    g.fillAll (kPaper);
}

void GrainEditor::paintCanvas (juce::Graphics& g)
{
    g.fillAll (kPaper);

    g.setColour (kInk);
    g.setFont (makeFont (28.0f, true));
    g.drawText ("GRAINFORFEIT", juce::Rectangle<int> (20, 18, 400, 34), juce::Justification::centredLeft);

    g.setColour (kGrey);
    g.setFont (makeFont (13.0f, false));
    g.drawText ("Granular texture engine", juce::Rectangle<int> (21, 50, 400, 18), juce::Justification::centredLeft);

    const auto presetArea = presetBox.getBounds();
    g.drawText ("Preset", presetArea.withX (presetArea.getX() - 62).withWidth (54), juce::Justification::centredRight);

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

namespace
{
// Menus get a little more width than knobs so their text fits
constexpr float kChoiceWeight = 1.35f;
}

void GrainEditor::layoutGroup (Group& grp)
{
    auto inner = grp.area;
    inner.removeFromTop (34);   // title and rule
    const int n = (int) grp.items.size();
    if (n == 0) return;

    float totalWeight = 0.0f;
    for (const auto& item : grp.items) totalWeight += item.isChoice ? kChoiceWeight : 1.0f;
    const float unit = (float) inner.getWidth() / totalWeight;

    for (int i = 0; i < n; ++i)
    {
        const int cellW = juce::roundToInt (unit * (grp.items[(size_t) i].isChoice ? kChoiceWeight : 1.0f));
        auto cell = (i == n - 1) ? inner : inner.removeFromLeft (cellW);
        auto& item = grp.items[(size_t) i];
        item.label->setBounds (cell.removeFromTop (18));

        if (item.isChoice)
            item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 6, 112), 30));
        else
            item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 92), cell.getHeight()));
    }
}

void GrainEditor::layoutRow (juce::Rectangle<int> row, std::initializer_list<int> groupIndexes)
{
    const int gap = 28;
    auto weightOf = [] (const Group& grp)
    {
        float w = 0.0f;
        for (const auto& item : grp.items) w += item.isChoice ? kChoiceWeight : 1.0f;
        return w;
    };
    float totalWeight = 0.0f;
    for (int gi : groupIndexes) totalWeight += weightOf (groups[(size_t) gi]);

    const int usable = row.getWidth() - gap * ((int) groupIndexes.size() - 1);
    int count = 0;
    for (int gi : groupIndexes)
    {
        auto& grp = groups[(size_t) gi];
        const bool last = ++count == (int) groupIndexes.size();
        const int w = last ? row.getWidth() : juce::roundToInt ((float) usable * weightOf (grp) / totalWeight);
        grp.area = row.removeFromLeft (w);
        if (! last) row.removeFromLeft (gap);
        layoutGroup (grp);
    }
}

void GrainEditor::resized()
{
    canvas.setBounds (0, 0, baseWidth, baseHeight);
    canvas.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));

    auto b = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (20);

    auto header = b.removeFromTop (52);
    freezeButton.setBounds (header.removeFromRight (130).withSizeKeepingCentre (130, 34));
    header.removeFromRight (18);
    presetBox.setBounds (header.removeFromRight (210).withSizeKeepingCentre (210, 32));

    b.removeFromTop (14);
    auto rowA = b.removeFromTop (260);
    auto padColumn = rowA.removeFromRight (300);
    rowA.removeFromRight (20);
    visualizer.setBounds (rowA);

    pad.setBounds (padColumn.removeFromTop (218));
    padColumn.removeFromTop (12);
    auto targets = padColumn.removeFromTop (30);
    padXLabel.setBounds (targets.removeFromLeft (20));
    targets.removeFromLeft (6);
    padXBox.setBounds (targets.removeFromLeft (122));
    targets.removeFromLeft (20);
    padYLabel.setBounds (targets.removeFromLeft (20));
    targets.removeFromLeft (6);
    padYBox.setBounds (targets);

    if (groups.size() < 8) return;

    b.removeFromTop (22);
    layoutRow (b.removeFromTop (150), { 0, 1 });
    b.removeFromTop (16);
    layoutRow (b.removeFromTop (150), { 2, 3, 4, 5 });
    b.removeFromTop (16);
    layoutRow (b.removeFromTop (150), { 6, 7 });
}
