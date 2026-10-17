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

float textWidth (const juce::Font& f, const juce::String& s)
{
   #if JUCE_MAJOR_VERSION >= 8
    return juce::GlyphArrangement::getStringWidth (f, s);
   #else
    return f.getStringWidthFloat (s);
   #endif
}

constexpr float kChoiceWeight = 1.35f;
const char* const kHint = "Tip: type phonemes in brackets for exact sounds, like [HH AH0 L OW1]. Press Talk, or play notes from your DAW.";
} // namespace

//==============================================================================
TalkLookAndFeel::TalkLookAndFeel()
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
    setColour (juce::TextEditor::highlightedTextColourId, kInk);
    setColour (juce::TextEditor::outlineColourId, kInk);
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

juce::Font TalkLookAndFeel::getComboBoxFont (juce::ComboBox&) { return makeFont (13.5f, false); }
juce::Font TalkLookAndFeel::getPopupMenuFont()                { return makeFont (14.0f, false); }
juce::Font TalkLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return makeFont (juce::jmin (15.0f, (float) h * 0.48f), false); }

void TalkLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider&)
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
}

void TalkLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool isHighlighted, bool isDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;
    const bool primary = (bool) button.getProperties().getWithDefault ("primary", false);

    if (button.getToggleState() || primary)
    {
        g.setColour (isDown ? kGrey : (isHighlighted ? juce::Colour (0xff2a2a2a) : kInk));
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
TalkDisplay::TalkDisplay (TalkProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void TalkDisplay::refresh()
{
    int s1, n1, s2, n2;
    const int ready = proc.scopeFifo.getNumReady();
    proc.scopeFifo.prepareToRead (ready, s1, n1, s2, n2);
    auto push = [this] (float v) { ring[(size_t) ringPos] = v; ringPos = (ringPos + 1) % (int) ring.size(); };
    for (int i = 0; i < n1; ++i) push (proc.scopeData[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) push (proc.scopeData[(size_t) (s2 + i)]);
    proc.scopeFifo.finishedRead (n1 + n2);

    const float level = proc.liveLevel.load();
    trail.push_back ({ proc.liveF1.load(), proc.liveF2.load(), level });
    if (trail.size() > 45) trail.erase (trail.begin());
}

void TalkDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const juce::Colour white = juce::Colours::white;
    g.setColour (kInk);
    g.fillRoundedRectangle (bounds, 14.0f);

    auto inner = bounds.reduced (18.0f, 14.0f);
    auto wordsArea = inner.removeFromTop (64.0f);
    inner.removeFromTop (8.0f);
    auto phonArea = inner.removeFromTop (24.0f);
    inner.removeFromTop (14.0f);
    auto scopeArea = inner.removeFromLeft (inner.getWidth() * 0.64f);
    inner.removeFromLeft (18.0f);
    auto vowelArea = inner;

    const auto* u = proc.getDisplayUtterance();
    const bool inSync = proc.textVersionPlaying.load() == proc.getTextVersion();
    const int phone = inSync ? proc.currentPhone.load() : -1;
    int currentWord = -1;
    if (u != nullptr && phone >= 0 && phone < (int) u->phones.size())
    {
        currentWord = u->phones[(size_t) phone].word;
        if (currentWord < 0)   // in a pause: keep the previous word lit
            for (int k = phone; k >= 0 && currentWord < 0; --k) currentWord = u->phones[(size_t) k].word;
    }

    // ---- Words ----
    if (u == nullptr || u->words.empty())
    {
        g.setColour (white.withAlpha (0.5f));
        g.setFont (makeFont (15.0f, false));
        g.drawText ("Type something above, then press Talk.", wordsArea, juce::Justification::centredLeft);
    }
    else
    {
        const auto font = makeFont (16.0f, false);
        g.setFont (font);
        const float lineH = 30.0f, padX = 7.0f, gap = 4.0f;
        struct Placed { int word; float x; int line; float w; };
        std::vector<Placed> placed;
        float x = 0.0f;
        int line = 0;
        for (int w = 0; w < (int) u->wordText.size(); ++w)
        {
            const float tw = textWidth (font, juce::String (u->wordText[(size_t) w])) + padX * 2.0f;
            if (x > 0.0f && x + tw > wordsArea.getWidth()) { x = 0.0f; ++line; }
            placed.push_back ({ w, x, line, tw });
            x += tw + gap;
        }
        int currentLine = 0;
        for (const auto& p : placed) if (p.word == currentWord) currentLine = p.line;
        const int firstLine = juce::jmax (0, currentLine - 1);

        for (const auto& p : placed)
        {
            const int visibleLine = p.line - firstLine;
            if (visibleLine < 0 || visibleLine > 1) continue;
            const juce::Rectangle<float> r (wordsArea.getX() + p.x, wordsArea.getY() + (float) visibleLine * (lineH + 4.0f), p.w, lineH);
            if (p.word == currentWord)
            {
                g.setColour (white);
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (kInk);
            }
            else
            {
                g.setColour (white.withAlpha (currentWord >= 0 && p.word < currentWord ? 0.35f : 0.85f));
            }
            g.drawText (juce::String (u->wordText[(size_t) p.word]), r, juce::Justification::centred);
        }
    }

    // ---- Phonemes of the current word ----
    if (u != nullptr && ! u->words.empty())
    {
        const int w = juce::jlimit (0, (int) u->words.size() - 1, currentWord >= 0 ? currentWord : 0);
        const auto& range = u->words[(size_t) w];
        const auto font = makeFont (13.5f, false);
        g.setFont (font);
        float x = phonArea.getX();
        g.setColour (white.withAlpha (0.45f));
        g.drawText ("Phonemes", juce::Rectangle<float> (x, phonArea.getY(), 74.0f, phonArea.getHeight()), juce::Justification::centredLeft);
        x += 78.0f;
        for (int k = range.first; k <= range.last; ++k)
        {
            juce::String name (talk::phoneme (u->phones[(size_t) k].id).name);
            const float tw = textWidth (font, name) + 12.0f;
            const juce::Rectangle<float> r (x, phonArea.getY() + 1.0f, tw, phonArea.getHeight() - 2.0f);
            if (k == phone)
            {
                g.setColour (white);
                g.fillRoundedRectangle (r, 5.0f);
                g.setColour (kInk);
            }
            else
            {
                g.setColour (white.withAlpha (0.75f));
                g.drawRoundedRectangle (r, 5.0f, 1.0f);
            }
            g.drawText (name, r, juce::Justification::centred);
            x += tw + 5.0f;
            if (x > phonArea.getRight() - 40.0f) break;
        }
    }

    // ---- Waveform ----
    {
        g.setColour (white.withAlpha (0.15f));
        g.drawHorizontalLine ((int) scopeArea.getCentreY(), scopeArea.getX(), scopeArea.getRight());
        const int n = (int) ring.size();
        const int show = 1024;
        int start = (ringPos - show - 512 + n) % n;
        for (int k = 0; k < 512; ++k)   // find an upward zero crossing so the trace holds still
        {
            const int a = (start + k) % n, b = (start + k + 1) % n;
            if (ring[(size_t) a] <= 0.0f && ring[(size_t) b] > 0.0f) { start = b; break; }
        }
        juce::Path wave;
        for (int k = 0; k < show; ++k)
        {
            const float v = ring[(size_t) ((start + k) % n)];
            const float px = scopeArea.getX() + scopeArea.getWidth() * (float) k / (float) (show - 1);
            const float py = scopeArea.getCentreY() - juce::jlimit (-1.0f, 1.0f, v) * scopeArea.getHeight() * 0.48f;
            if (k == 0) wave.startNewSubPath (px, py); else wave.lineTo (px, py);
        }
        g.setColour (white);
        g.strokePath (wave, juce::PathStrokeType (1.4f));
    }

    // ---- Vowel space: F2 across (front vowels on the left), F1 down ----
    {
        g.setColour (white.withAlpha (0.18f));
        g.drawRoundedRectangle (vowelArea, 8.0f, 1.0f);
        auto plot = vowelArea.reduced (14.0f, 12.0f);
        auto xFor = [&] (float f2) { return juce::jmap (juce::jlimit (600.0f, 2600.0f, f2), 2600.0f, 600.0f, plot.getX(), plot.getRight()); };
        auto yFor = [&] (float f1) { return juce::jmap (juce::jlimit (200.0f, 850.0f, f1), 200.0f, 850.0f, plot.getY(), plot.getBottom()); };

        struct Ref { const char* label; float f1, f2; };
        static const Ref refs[] = { { "ee", 290, 2250 }, { "i", 400, 1800 }, { "e", 530, 1680 }, { "a", 660, 1720 },
                                    { "ah", 700, 1220 }, { "aw", 600, 990 }, { "u", 450, 1100 }, { "oo", 320, 1000 },
                                    { "er", 470, 1270 } };
        g.setFont (makeFont (11.5f, false));
        g.setColour (white.withAlpha (0.45f));
        for (const auto& r : refs)
            g.drawText (r.label, juce::Rectangle<float> (xFor (r.f2) - 14.0f, yFor (r.f1) - 8.0f, 28.0f, 16.0f), juce::Justification::centred);
        g.drawText ("Vowel space", vowelArea.reduced (10.0f, 6.0f), juce::Justification::bottomLeft);

        for (size_t k = 0; k < trail.size(); ++k)
        {
            const auto& t = trail[k];
            if (t.level < 0.02f) continue;
            const float age = (float) (k + 1) / (float) trail.size();
            const float r = 1.5f + 2.5f * age;
            g.setColour (white.withAlpha (0.12f + 0.5f * age));
            g.fillEllipse (xFor (t.f2) - r, yFor (t.f1) - r, r * 2.0f, r * 2.0f);
        }
        if (! trail.empty() && trail.back().level >= 0.02f)
        {
            const auto& t = trail.back();
            g.setColour (white);
            g.fillEllipse (xFor (t.f2) - 6.0f, yFor (t.f1) - 6.0f, 12.0f, 12.0f);
        }
    }
}

//==============================================================================
TalkEditor::TalkEditor (TalkProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);

    canvas.painter = [this] (juce::Graphics& g) { paintCanvas (g); };
    addAndMakeVisible (canvas);

    textBox.setColour (juce::TextEditor::textColourId, kInk);
    textBox.setColour (juce::TextEditor::backgroundColourId, kPaper);
    textBox.setColour (juce::TextEditor::outlineColourId, kInk);
    textBox.setColour (juce::TextEditor::focusedOutlineColourId, kInk);
    textBox.setColour (juce::TextEditor::highlightColourId, kLine);
    textBox.setColour (juce::CaretComponent::caretColourId, kInk);
    textBox.setMultiLine (true, true);
    textBox.setReturnKeyStartsNewLine (true);
    textBox.setFont (makeFont (18.0f, false));
    textBox.setIndents (12, 10);
    textBox.setTextToShowWhenEmpty ("Type what it should say...", kGrey);
    textBox.setText (proc.getText(), juce::dontSendNotification);
    textBox.onTextChange = [this] { textDirty = true; lastEdit = juce::Time::getMillisecondCounter(); };
    canvas.addAndMakeVisible (textBox);

    hint.setText (kHint, juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, kGrey);
    hint.setFont (makeFont (12.5f, false));
    canvas.addAndMakeVisible (hint);

    display.setComponentID ("display");
    canvas.addAndMakeVisible (display);

    voiceBox.addItemList (TalkProcessor::voiceNames(), 1);
    voiceBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    voiceBox.onChange = [this]
    {
        const int index = voiceBox.getSelectedId() - 1;
        if (index >= 0) proc.setCurrentProgram (index);
    };
    canvas.addAndMakeVisible (voiceBox);

    talkButton.getProperties().set ("primary", true);
    talkButton.setColour (juce::TextButton::textColourOffId, kPaper);
    talkButton.setColour (juce::TextButton::textColourOnId, kPaper);
    talkButton.onClick = [this]
    {
        if (textDirty) { proc.setText (textBox.getText()); textDirty = false; }
        if (proc.currentPhone.load() >= 0) proc.requestStop();
        else proc.requestTalk();
    };
    canvas.addAndMakeVisible (talkButton);

    exportButton.onClick = [this] { exportWav(); };
    canvas.addAndMakeVisible (exportButton);

    Group voice   { "Voice", {}, {} };
    addKnob   (voice, "pitch", "Pitch");
    addKnob   (voice, "speed", "Speed");
    addKnob   (voice, "formant", "Formant");
    addKnob   (voice, "inflection", "Inflection");
    addChoice (voice, "intonation", "Intonation");
    addChoice (voice, "effort", "Effort");

    Group play    { "MIDI performance", {}, {} };
    addChoice (play, "mode", "Each note plays");
    holdLabel.setText ("Hold vowels", juce::dontSendNotification);
    holdLabel.setJustificationType (juce::Justification::centred);
    holdLabel.setFont (makeFont (13.0f, true));
    holdButton.setClickingTogglesState (true);
    canvas.addAndMakeVisible (holdButton);
    canvas.addAndMakeVisible (holdLabel);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "hold", holdButton);
    play.items.push_back ({ &holdButton, &holdLabel, 2 });
    addKnob (play, "glide", "Glide");
    addKnob (play, "vibrato", "Vibrato");

    Group output  { "Output", {}, {} };
    addKnob (output, "lofi", "Lo-fi");
    addKnob (output, "volume", "Volume");

    groups.push_back (std::move (voice));
    groups.push_back (std::move (play));
    groups.push_back (std::move (output));

    sendLookAndFeelChange();   // push the black and white colours into every control
    textBox.applyColourToAllText (kInk);

    setResizable (true, true);
    setResizeLimits ((int) (baseWidth * 0.6), (int) (baseHeight * 0.6), (int) (baseWidth * 1.5), (int) (baseHeight * 1.5));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (juce::roundToInt (baseWidth * 0.85), juce::roundToInt (baseHeight * 0.85));

    startTimerHz (30);
}

TalkEditor::~TalkEditor()
{
    stopTimer();
    if (textDirty) proc.setText (textBox.getText());
    setLookAndFeel (nullptr);
}

void TalkEditor::addKnob (Group& group, const juce::String& paramId, const juce::String& name)
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
    group.items.push_back ({ &k->slider, &k->label, 0 });
    knobs.push_back (std::move (k));
}

void TalkEditor::addChoice (Group& group, const juce::String& paramId, const juce::String& name)
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
    group.items.push_back ({ &c->box, &c->label, 1 });
    choices.push_back (std::move (c));
}

//==============================================================================
void TalkEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (textDirty && now - lastEdit > 300)
    {
        proc.setText (textBox.getText());
        textDirty = false;
    }

    talkButton.setButtonText (proc.currentPhone.load() >= 0 ? "Stop" : "Talk");
    holdButton.setButtonText (holdButton.getToggleState() ? "On" : "Off");

    if (statusUntil != 0 && now > statusUntil)
    {
        statusUntil = 0;
        hint.setText (kHint, juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, kGrey);
    }

    const int program = proc.getCurrentProgram() + 1;
    if (voiceBox.getSelectedId() != program)
        voiceBox.setSelectedId (program, juce::dontSendNotification);
}

void TalkEditor::showStatus (const juce::String& message)
{
    hint.setText (message, juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, kInk);
    statusUntil = juce::Time::getMillisecondCounter() + 6000;
}

void TalkEditor::exportWav()
{
    if (textDirty) { proc.setText (textBox.getText()); textDirty = false; }

    chooser = std::make_unique<juce::FileChooser> ("Export speech as a WAV file",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("TALKFORFEIT.wav"), "*.wav");

    const auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;
        if (! file.hasFileExtension ("wav")) file = file.withFileExtension ("wav");
        if (proc.renderToFile (file)) showStatus ("Saved " + file.getFullPathName());
        else showStatus ("Could not save the WAV file. Try a different folder.");
    });
}

//==============================================================================
void TalkEditor::paint (juce::Graphics& g)
{
    g.fillAll (kPaper);
}

void TalkEditor::paintCanvas (juce::Graphics& g)
{
    g.fillAll (kPaper);

    g.setColour (kInk);
    g.setFont (makeFont (28.0f, true));
    g.drawText ("TALKFORFEIT", juce::Rectangle<int> (20, 18, 400, 34), juce::Justification::centredLeft);
    g.setColour (kGrey);
    g.setFont (makeFont (13.0f, false));
    g.drawText ("Retro speech synth", juce::Rectangle<int> (21, 50, 400, 18), juce::Justification::centredLeft);

    const auto vb = voiceBox.getBounds();
    g.drawText ("Voice", vb.withX (vb.getX() - 56).withWidth (48), juce::Justification::centredRight);

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

void TalkEditor::layoutGroup (Group& grp)
{
    auto inner = grp.area;
    inner.removeFromTop (34);
    const int n = (int) grp.items.size();
    if (n == 0) return;

    float totalWeight = 0.0f;
    for (const auto& item : grp.items) totalWeight += item.kind == 1 ? kChoiceWeight : 1.0f;
    const float unit = (float) inner.getWidth() / totalWeight;

    for (int i = 0; i < n; ++i)
    {
        auto& item = grp.items[(size_t) i];
        const int cellW = juce::roundToInt (unit * (item.kind == 1 ? kChoiceWeight : 1.0f));
        auto cell = (i == n - 1) ? inner : inner.removeFromLeft (cellW);
        item.label->setBounds (cell.removeFromTop (18));

        if (item.kind == 1)      item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 6, 120), 30));
        else if (item.kind == 2) item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 14, 76), 30));
        else                     item.control->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 92), cell.getHeight()));
    }
}

void TalkEditor::layoutRow (juce::Rectangle<int> row, std::initializer_list<int> groupIndexes)
{
    const int gap = 28;
    auto weightOf = [] (const Group& grp)
    {
        float w = 0.0f;
        for (const auto& item : grp.items) w += item.kind == 1 ? kChoiceWeight : 1.0f;
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

void TalkEditor::resized()
{
    canvas.setBounds (0, 0, baseWidth, baseHeight);
    canvas.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));

    auto b = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (20);

    auto header = b.removeFromTop (52);
    exportButton.setBounds (header.removeFromRight (130).withSizeKeepingCentre (130, 34));
    header.removeFromRight (12);
    talkButton.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 34));
    header.removeFromRight (18);
    voiceBox.setBounds (header.removeFromRight (200).withSizeKeepingCentre (200, 32));

    b.removeFromTop (12);
    textBox.setBounds (b.removeFromTop (92));
    b.removeFromTop (4);
    hint.setBounds (b.removeFromTop (18));
    b.removeFromTop (10);
    display.setBounds (b.removeFromTop (240));

    if (groups.size() < 3) return;
    b.removeFromTop (20);
    layoutRow (b.removeFromTop (150), { 0 });
    b.removeFromTop (16);
    layoutRow (b.removeFromTop (150), { 1, 2 });
}
