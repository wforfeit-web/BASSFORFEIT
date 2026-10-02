#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Parameter order. Presets below list their values in this same order.
enum ParamIndex
{
    pOsc1, pOsc2, pDetune, pSub, pCutoff, pRes, pDrive, pRate, pDepth, pShape,
    pAttack, pRelease, pGlide, pVolume, pScream, pVowel, pTalk, pBite
};

const char* const kIds[BassforfeitProcessor::numParams] = {
    "osc1", "osc2", "detune", "sub", "cutoff", "res", "drive", "rate", "depth", "shape",
    "attack", "release", "glide", "volume", "scream", "vowel", "talk", "bite"
};

// Choice indexes:
//   waves:  0 Saw, 1 Square, 2 Triangle, 3 Sine
//   rates:  0 1/1, 1 1/2, 2 1/4, 3 1/8, 4 1/8T, 5 1/16, 6 1/16T, 7 1/32
//   shapes: 0 Sine, 1 Triangle, 2 Square, 3 Saw down
struct Preset
{
    const char* name;
    float v[BassforfeitProcessor::numParams];
};

const Preset kPresets[] = {
    //                  osc1 osc2 det  sub    cutoff res drive  rate depth shape attack  release glide  vol    scream vowel talk  bite
    { "Classic wobble", { 0,   1,   12, 0.6f,  300,  8,  0.4f,  3,   0.8f, 0,    0.005f, 0.08f,  0.04f, 0.7f,  0.0f,  0.5f, 0.5f, 0.5f } },
    { "Talking growl",  { 0,   0,   25, 0.4f,  500,  14, 0.8f,  5,   0.7f, 1,    0.003f, 0.06f,  0.03f, 0.65f, 0.0f,  0.5f, 0.5f, 0.5f } },
    { "Reese",          { 0,   0,   32, 0.5f,  900,  3,  0.3f,  1,   0.2f, 0,    0.01f,  0.2f,   0.08f, 0.7f,  0.0f,  0.5f, 0.5f, 0.5f } },
    { "Square stab",    { 1,   1,   8,  0.5f,  700,  10, 0.6f,  4,   0.6f, 2,    0.002f, 0.05f,  0.0f,  0.65f, 0.0f,  0.5f, 0.5f, 0.5f } },
    { "Deep sub",       { 3,   2,   0,  0.9f,  1200, 1,  0.15f, 2,   0.0f, 0,    0.005f, 0.15f,  0.06f, 0.8f,  0.0f,  0.5f, 0.5f, 0.5f } },
    { "Brostep scream", { 0,   0,   18, 0.5f,  1400, 6,  0.7f,  5,   0.45f,3,    0.003f, 0.07f,  0.05f, 0.6f,  0.9f,  0.55f,0.7f, 0.75f } },
    { "Yoi scream",     { 0,   1,   14, 0.55f, 900,  8,  0.6f,  3,   0.5f, 0,    0.004f, 0.08f,  0.06f, 0.6f,  0.75f, 0.2f, 1.0f, 0.6f } },
};
constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));

// Wobble cycles per beat for each rate choice
const float kRateCpb[8] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };

// Vowel formants F1, F2, F3 (Hz), morphing OO -> OH -> AH -> EH -> EE
const float kVowels[5][3] = {
    { 300.0f,  870.0f, 2240.0f },
    { 450.0f,  800.0f, 2830.0f },
    { 730.0f, 1090.0f, 2440.0f },
    { 530.0f, 1840.0f, 2480.0f },
    { 270.0f, 2290.0f, 3010.0f },
};
const char* const kVowelNames[5] = { "OO", "OH", "AH", "EH", "EE" };
const float kFormantGain[3] = { 3.0f, 2.4f, 1.5f };

constexpr double kTwoPi = juce::MathConstants<double>::twoPi;

inline float polyBlep (double t, double dt)
{
    if (dt <= 0.0) return 0.0f;
    if (t < dt)
    {
        t /= dt;
        return (float) (t + t - t * t - 1.0);
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return (float) (t * t + t + t + 1.0);
    }
    return 0.0f;
}

inline float oscSample (int wave, double p, double dt)
{
    switch (wave)
    {
        case 0: // saw
            return (float) (2.0 * p - 1.0) - polyBlep (p, dt);
        case 1: // square
        {
            double p2 = p + 0.5;
            if (p2 >= 1.0) p2 -= 1.0;
            return (p < 0.5 ? 1.0f : -1.0f) + polyBlep (p, dt) - polyBlep (p2, dt);
        }
        case 2: // triangle
            return (float) (1.0 - 4.0 * std::abs (p - 0.5));
        default: // sine
            return (float) std::sin (kTwoPi * p);
    }
}

inline float lfoSample (int shape, double p)
{
    switch (shape)
    {
        case 0:  return (float) std::sin (kTwoPi * p);           // sine
        case 1:  return (float) (1.0 - 4.0 * std::abs (p - 0.5)); // triangle
        case 2:  return p < 0.5 ? 1.0f : -1.0f;                   // square
        default: return (float) (1.0 - 2.0 * p);                  // saw down
    }
}

inline void vowelFormants (float v, float out[3])
{
    const float x = juce::jlimit (0.0f, 1.0f, v) * 4.0f;
    const int i = juce::jmin (3, (int) x);
    const float t = x - (float) i;
    for (int k = 0; k < 3; ++k)
        out[k] = kVowels[i][k] * std::pow (kVowels[i + 1][k] / kVowels[i][k], t);
}

inline void wrap (double& p)
{
    if (p >= 1.0) p -= std::floor (p);
}

inline float shaper (float x, float k, float norm)
{
    return std::tanh (k * juce::jlimit (-1.0f, 1.0f, x)) / norm;
}
} // namespace

//==============================================================================
BassforfeitProcessor::BassforfeitProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < numParams; ++i)
        params[(size_t) i] = apvts.getRawParameterValue (kIds[i]);
}

juce::AudioProcessorValueTreeState::ParameterLayout BassforfeitProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    const StringArray waves  { "Saw", "Square", "Triangle", "Sine" };
    const StringArray rates  { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    const StringArray shapes { "Sine", "Triangle", "Square", "Saw down" };

    auto pct   = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto hz    = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto ms    = [] (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; };
    auto cents = [] (float v, int) { return String (roundToInt (v)) + " ct"; };
    auto one   = [] (float v, int) { return String (v, 1); };
    auto vowel = [] (float v, int) { return String (kVowelNames[jlimit (0, 4, roundToInt (v * 4.0f))]); };

    auto logRange = [] (float lo, float hi)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (std::sqrt (lo * hi));
        return r;
    };

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                         std::function<String (float, int)> fmt)
    {
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                          AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt))));
    };
    auto addChoice = [&] (const char* id, const char* name, const StringArray& items, int def)
    {
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    addChoice ("osc1",    "Osc 1",     waves, 0);
    addChoice ("osc2",    "Osc 2",     waves, 1);
    addFloat  ("detune",  "Detune",    NormalisableRange<float> (0.0f, 50.0f), 12.0f, cents);
    addFloat  ("sub",     "Sub",       NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);
    addFloat  ("cutoff",  "Cutoff",    logRange (40.0f, 8000.0f), 300.0f, hz);
    addFloat  ("res",     "Resonance", logRange (0.5f, 20.0f), 8.0f, one);
    addFloat  ("drive",   "Drive",     NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct);
    addChoice ("rate",    "Rate",      rates, 3);
    addFloat  ("depth",   "Depth",     NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct);
    addChoice ("shape",   "Shape",     shapes, 0);
    addFloat  ("attack",  "Attack",    logRange (0.001f, 1.0f), 0.005f, ms);
    addFloat  ("release", "Release",   logRange (0.01f, 2.0f), 0.08f, ms);
    addFloat  ("glide",   "Glide",     NormalisableRange<float> (0.0f, 0.5f), 0.04f, ms);
    addFloat  ("volume",  "Volume",    NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct);
    addFloat  ("scream",  "Scream",    NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    addFloat  ("vowel",   "Vowel",     NormalisableRange<float> (0.0f, 1.0f), 0.5f, vowel);
    addFloat  ("talk",    "Talk",      NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    addFloat  ("bite",    "Bite",      NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);

    return { ps.begin(), ps.end() };
}

//==============================================================================
void BassforfeitProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    filter.reset();
    screamHP.reset();
    for (auto& f : formants) f.reset();
    screamHP.set (180.0f, 0.707f, (float) sr);
    env = envTarget = 0.0f;
    numHeld = 0;
    formantCounter = 0;
}

bool BassforfeitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
void BassforfeitProcessor::addHeld (int note)
{
    removeHeld (note);
    if (numHeld == 16)
    {
        for (int i = 1; i < 16; ++i) held[i - 1] = held[i];
        --numHeld;
    }
    held[numHeld++] = note;
}

void BassforfeitProcessor::removeHeld (int note)
{
    for (int i = 0; i < numHeld; ++i)
    {
        if (held[i] == note)
        {
            for (int j = i + 1; j < numHeld; ++j) held[j - 1] = held[j];
            --numHeld;
            return;
        }
    }
}

void BassforfeitProcessor::handleMidi (const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        const bool legato = numHeld > 0;
        addHeld (msg.getNoteNumber());
        targetFreq = (float) juce::MidiMessage::getMidiNoteInHertz (msg.getNoteNumber());
        if (! legato)
        {
            curFreq = targetFreq; // fresh note: jump straight to pitch
            envTarget = 1.0f;
        }
    }
    else if (msg.isNoteOff())
    {
        removeHeld (msg.getNoteNumber());
        if (numHeld > 0)
            targetFreq = (float) juce::MidiMessage::getMidiNoteInHertz (held[numHeld - 1]); // glide back
        else
            envTarget = 0.0f;
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        numHeld = 0;
        envTarget = 0.0f;
    }
}

//==============================================================================
void BassforfeitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Sync the wobble to the host tempo and bar position
    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto pos = hostPlayHead->getPosition())
        {
            if (auto hostBpm = pos->getBpm())
                bpm = juce::jlimit (20.0, 400.0, *hostBpm);

            if (pos->getIsPlaying())
            {
                if (auto ppq = pos->getPpqPosition())
                {
                    const int rateIdx = juce::jlimit (0, 7, (int) params[pRate]->load());
                    const double cycles = *ppq * kRateCpb[rateIdx];
                    lfoPhase = cycles - std::floor (cycles);
                }
            }
        }
    }

    float* left  = buffer.getNumChannels() > 0 ? buffer.getWritePointer (0) : nullptr;
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (left == nullptr) return;

    int pos = 0;
    for (const auto meta : midi)
    {
        const int s = juce::jlimit (0, numSamples, meta.samplePosition);
        if (s > pos) renderSamples (left, right, pos, s - pos);
        handleMidi (meta.getMessage());
        pos = juce::jmax (pos, s);
    }
    if (pos < numSamples)
        renderSamples (left, right, pos, numSamples - pos);
}

void BassforfeitProcessor::renderSamples (float* left, float* right, int start, int num)
{
    const float fsr = (float) sr;

    const int   wave1   = (int) params[pOsc1]->load();
    const int   wave2   = (int) params[pOsc2]->load();
    const float detune  = params[pDetune]->load();
    const float subLvl  = params[pSub]->load();
    const float cutoff  = params[pCutoff]->load();
    const float res     = params[pRes]->load();
    const float drive   = params[pDrive]->load();
    const int   rateIdx = juce::jlimit (0, 7, (int) params[pRate]->load());
    const float depth   = params[pDepth]->load();
    const int   shape   = (int) params[pShape]->load();
    const float attack  = params[pAttack]->load();
    const float release = params[pRelease]->load();
    const float glide   = params[pGlide]->load();
    const float volume  = params[pVolume]->load();
    const float scream  = params[pScream]->load();
    const float vowel   = params[pVowel]->load();
    const float talk    = params[pTalk]->load();
    const float bite    = params[pBite]->load();

    const float aCoef = 1.0f - std::exp (-1.0f / (juce::jmax (attack / 3.0f, 0.002f) * fsr));
    const float rCoef = 1.0f - std::exp (-1.0f / (juce::jmax (release / 4.0f, 0.004f) * fsr));
    const float gCoef = glide > 0.0f ? 1.0f - std::exp (-1.0f / ((glide / 3.0f) * fsr)) : 1.0f;

    const double det1 = std::pow (2.0, -detune / 2400.0);
    const double det2 = std::pow (2.0,  detune / 2400.0);
    const double lfoInc = bpm / 60.0 * kRateCpb[rateIdx] / sr;

    const float driveK = 1.0f + drive * 24.0f, driveNorm = std::tanh (driveK);
    const float preGain = 1.0f + drive * 4.0f, postGain = 1.0f / (1.0f + drive * 1.5f);

    const float screamK = 6.0f + bite * 40.0f, screamNorm = std::tanh (screamK);
    const float formantQ = 4.0f + bite * 18.0f;
    float vf[3];
    vowelFormants (vowel, vf);

    const float dryLevel = 1.0f - scream * 0.6f;
    const float screamLevel = scream * 0.5f;

    for (int i = start; i < start + num; ++i)
    {
        // Pitch with glide
        curFreq += (targetFreq - curFreq) * gCoef;
        const double f = curFreq;

        // Main oscillators + sub
        const double dt1 = f * det1 / sr, dt2 = f * det2 / sr;
        const float o1 = oscSample (wave1, ph1, dt1);
        const float o2 = oscSample (wave2, ph2, dt2);
        ph1 += dt1; wrap (ph1);
        ph2 += dt2; wrap (ph2);
        const float sub = (float) std::sin (kTwoPi * phSub);
        phSub += f * 0.5 / sr; wrap (phSub);

        // Wobble LFO sweeps the filter up from the cutoff by up to 4 octaves
        const float lfo = lfoSample (shape, lfoPhase);
        lfoPhase += lfoInc; wrap (lfoPhase);

        const float fc = cutoff * std::exp2 (depth * 2.0f * (1.0f + lfo));
        filter.set (fc, res, fsr);

        const float mix = o1 * 0.3f + o2 * 0.3f;
        float lp, bp, hp;
        filter.process (mix, lp, bp, hp);
        const float dry = shaper (lp * preGain, driveK, driveNorm) * postGain;

        // Scream: octave-up FM saw -> vowel formants -> hard clip -> highpass
        float screamOut = 0.0f;
        if (scream > 0.0001f)
        {
            const double fmDepth = bite * f * 2.0 * 1.6;
            const double fm = std::sin (kTwoPi * phFm);
            phFm += f * 2.0 / sr; wrap (phFm);
            const double dtS = juce::jmax (1.0e-6, (f * 2.0 + fmDepth * fm) / sr);
            const float scSaw = (float) (2.0 * phScream - 1.0) - polyBlep (phScream, dtS);
            phScream += dtS; wrap (phScream);

            const float scIn = mix + scSaw * 0.35f;

            if (formantCounter-- <= 0)
            {
                formantCounter = 8;
                const float talkMul = std::exp2 (talk * 1.25f * lfo);
                for (int k = 0; k < 3; ++k)
                    formants[k].set (vf[k] * talkMul, formantQ, fsr);
            }

            float sum = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                float flp, fbp, fhp;
                formants[k].process (scIn, flp, fbp, fhp);
                sum += fbp * formants[k].k * kFormantGain[k]; // k * bp = unity-peak bandpass
            }

            const float clipped = shaper (sum, screamK, screamNorm);
            float hlp, hbp, hhp;
            screamHP.process (clipped, hlp, hbp, hhp);
            screamOut = hhp * screamLevel;
        }

        // Amp envelope
        env += (envTarget - env) * (envTarget > env ? aCoef : rCoef);

        float out = (dry * dryLevel + sub * subLvl * 0.7f + screamOut) * env * volume;
        out = std::tanh (out); // gentle output saturation keeps peaks under 0 dBFS

        left[i] = out;
        if (right != nullptr) right[i] = out;
    }
}

//==============================================================================
int BassforfeitProcessor::getNumPrograms() { return kNumPresets; }

void BassforfeitProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentProgram = index;
    for (int k = 0; k < numParams; ++k)
        if (auto* p = apvts.getParameter (kIds[k]))
            p->setValueNotifyingHost (p->convertTo0to1 (kPresets[index].v[k]));
}

const juce::String BassforfeitProcessor::getProgramName (int index)
{
    return (index >= 0 && index < kNumPresets) ? juce::String (kPresets[index].name) : juce::String();
}

juce::StringArray BassforfeitProcessor::presetNames()
{
    juce::StringArray names;
    for (const auto& p : kPresets) names.add (p.name);
    return names;
}

//==============================================================================
void BassforfeitProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BassforfeitProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, kNumPresets - 1, (int) tree.getProperty ("program", 0));
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessorEditor* BassforfeitProcessor::createEditor()
{
    return new BassforfeitEditor (*this);
}

// Entry point JUCE uses to create the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassforfeitProcessor();
}
