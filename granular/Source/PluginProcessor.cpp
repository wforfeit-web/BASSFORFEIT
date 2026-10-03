#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* const kIds[GrainProcessor::numParams] = {
    "size", "density", "rhythm", "position", "spray", "scan", "shape", "feedback",
    "pitch", "pitchrand", "scale", "reverse", "spread",
    "type", "drive",
    "fmode", "cutoff", "reso",
    "dtime", "dfeedback", "echo", "room", "reverb",
    "l1shape", "l1rate", "l1depth", "l1target",
    "l2shape", "l2rate", "l2depth", "l2target",
    "padx", "pady", "padxtarget", "padytarget",
    "mix", "output", "freeze"
};

const int kDestParam[GrainProcessor::numDest] = {
    -1,
    GrainProcessor::pPosition, GrainProcessor::pSpray, GrainProcessor::pSize, GrainProcessor::pDensity,
    GrainProcessor::pScan, GrainProcessor::pShape, GrainProcessor::pPitch, GrainProcessor::pRandom,
    GrainProcessor::pSpread, GrainProcessor::pDrive, GrainProcessor::pCutoff, GrainProcessor::pEcho,
    GrainProcessor::pReverb, GrainProcessor::pMix
};

// Rhythm choices, in beats: Free, 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
const double kDivBeats[] = { 0.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

// Scale degrees used to snap random pitch: Free, Octaves, Fifths, Major, Minor, Penta
const std::vector<int> kScales[] = {
    {}, { 0 }, { 0, 7 }, { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 7, 9 }
};

float quantizeToScale (float semis, int scale)
{
    if (scale <= 0 || scale > 5) return semis;
    float best = 0.0f, bestDist = 1.0e9f;
    for (int octave = -4; octave <= 4; ++octave)
        for (int degree : kScales[scale])
        {
            const float cand = (float) (octave * 12 + degree);
            const float d = std::abs (cand - semis);
            if (d < bestDist) { bestDist = d; best = cand; }
        }
    return best;
}

// Transparent below 0.8, then rounds peaks off so the effect never goes past full scale
inline float safetyLimit (float x) noexcept
{
    const float a = std::abs (x);
    if (a <= 0.8f) return x;
    return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x);
}

inline float shapeSample (int type, float x) noexcept
{
    switch (type)
    {
        case 0:  return std::tanh (x);                    // Soft
        case 1:  return juce::jlimit (-1.0f, 1.0f, x);    // Hard
        default: return std::sin (x);                     // Fold
    }
}

// ---- Factory presets: every parameter starts at its default, then these overrides apply ----
struct PresetDef
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<PresetDef>& factoryPresets()
{
    // Choice values are indexes. Targets: 0 None, 1 Position, 2 Spray, 3 Size, 4 Density, 5 Scan,
    // 6 Shape, 7 Pitch, 8 Random, 9 Spread, 10 Drive, 11 Cutoff, 12 Echo, 13 Reverb, 14 Mix
    static const std::vector<PresetDef> presets = {
        { "Init", {} },
        { "Glass Cloud", { { "size", 180 }, { "density", 22 }, { "position", 300 }, { "spray", 600 },
                           { "pitchrand", 12 }, { "scale", 1 }, { "reverse", 0.3f }, { "drive", 0.15f },
                           { "cutoff", 9000 }, { "echo", 0.25f }, { "room", 0.85f }, { "reverb", 0.55f },
                           { "l1shape", 0 }, { "l1rate", 0.08f }, { "l1depth", 0.4f }, { "l1target", 1 },
                           { "mix", 0.6f } } },
        { "Stutter Grid", { { "rhythm", 4 }, { "size", 70 }, { "shape", 0.85f }, { "position", 250 },
                            { "spray", 0 }, { "reverse", 0.5f }, { "pitchrand", 7 }, { "scale", 2 },
                            { "type", 1 }, { "drive", 0.55f }, { "echo", 0.15f }, { "reverb", 0.1f },
                            { "l1depth", 0 }, { "l2shape", 2 }, { "l2rate", 0.5f }, { "l2depth", 0.25f },
                            { "l2target", 7 }, { "mix", 0.7f } } },
        { "Reverse Bloom", { { "reverse", 1 }, { "size", 400 }, { "density", 10 }, { "position", 900 },
                             { "spray", 400 }, { "feedback", 0.35f }, { "dtime", 500 }, { "echo", 0.3f },
                             { "room", 0.9f }, { "reverb", 0.5f }, { "mix", 0.65f } } },
        { "Octave Rain", { { "rhythm", 5 }, { "size", 60 }, { "shape", 0.7f }, { "pitch", 12 },
                           { "pitchrand", 24 }, { "scale", 1 }, { "spread", 1 }, { "drive", 0.2f },
                           { "dtime", 250 }, { "echo", 0.3f }, { "reverb", 0.4f }, { "l1depth", 0 },
                           { "mix", 0.6f } } },
        { "Broken Radio", { { "type", 2 }, { "drive", 0.75f }, { "fmode", 1 }, { "cutoff", 1400 },
                            { "reso", 4 }, { "size", 50 }, { "density", 30 }, { "spray", 200 },
                            { "l1shape", 3 }, { "l1rate", 3 }, { "l1depth", 0.6f }, { "l1target", 11 },
                            { "reverb", 0.15f }, { "mix", 0.7f }, { "output", 4 } } },
        { "Bitcrushed Swarm", { { "type", 3 }, { "drive", 0.6f }, { "density", 45 }, { "size", 35 },
                                { "spray", 900 }, { "pitchrand", 5 }, { "spread", 1 },
                                { "l1shape", 1 }, { "l1rate", 0.3f }, { "l1depth", 0.5f }, { "l1target", 4 },
                                { "mix", 0.6f } } },
        { "Frozen Choir", { { "size", 450 }, { "density", 30 }, { "position", 500 }, { "spray", 900 },
                            { "scan", 0.15f }, { "pitchrand", 7 }, { "scale", 3 }, { "reverse", 0.2f },
                            { "cutoff", 7000 }, { "room", 0.95f }, { "reverb", 0.6f },
                            { "l1rate", 0.05f }, { "l1depth", 0.5f }, { "l1target", 1 }, { "mix", 0.8f } } },
        { "Sub Smear", { { "pitch", -12 }, { "size", 300 }, { "density", 16 }, { "spray", 300 },
                         { "cutoff", 900 }, { "reso", 1.2f }, { "drive", 0.45f }, { "reverb", 0.2f },
                         { "l1depth", 0 }, { "mix", 0.5f } } },
        { "Tape Warp", { { "scan", -0.5f }, { "size", 220 }, { "density", 14 }, { "spray", 120 },
                         { "feedback", 0.3f }, { "drive", 0.35f }, { "cutoff", 6000 }, { "echo", 0.2f },
                         { "l1shape", 0 }, { "l1rate", 0.6f }, { "l1depth", 0.12f }, { "l1target", 7 },
                         { "mix", 0.6f } } },
        { "Minor Halo", { { "size", 260 }, { "density", 18 }, { "position", 400 }, { "spray", 500 },
                          { "pitch", 12 }, { "pitchrand", 12 }, { "scale", 4 }, { "reverse", 0.4f },
                          { "dtime", 600 }, { "dfeedback", 0.5f }, { "echo", 0.35f }, { "room", 0.9f },
                          { "reverb", 0.5f }, { "l2shape", 3 }, { "l2rate", 0.25f }, { "l2depth", 0.3f },
                          { "l2target", 9 }, { "mix", 0.7f } } },
    };
    return presets;
}
} // namespace

//==============================================================================
const char* GrainProcessor::paramId (int index)         { return kIds[index]; }
int GrainProcessor::destParam (int dest)                { return (dest > 0 && dest < numDest) ? kDestParam[dest] : -1; }

juce::StringArray GrainProcessor::destNames()
{
    return { "None", "Position", "Spray", "Size", "Density", "Scan", "Shape", "Pitch", "Random",
             "Spread", "Drive", "Cutoff", "Echo", "Reverb", "Mix" };
}

juce::StringArray GrainProcessor::presetNames()
{
    juce::StringArray names;
    for (const auto& p : factoryPresets()) names.add (p.name);
    return names;
}

//==============================================================================
GrainProcessor::GrainProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < numParams; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (kIds[i]);
        prm[(size_t) i] = apvts.getParameter (kIds[i]);
    }

    for (auto& c : columnPeaks) c.store (0.0f);
    for (auto& g : grainPos)    g.store (-1.0f);
    for (auto& g : grainLevel)  g.store (0.0f);
    for (auto& g : grainPitch)  g.store (0.0f);
    for (auto& m : modDisplay)  m.store (-1.0f);

    rebuildWindow (0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout GrainProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto ms    = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; };
    auto pct   = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto perS  = [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " /s"; };
    auto semis = [] (float v, int) { const int s = roundToInt (v); return (s > 0 ? "+" : "") + String (s) + " st"; };
    auto range = [] (float v, int) { return String::charToString ((juce_wchar) 0x00B1) + String (roundToInt (v)) + " st"; };
    auto db    = [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; };
    auto hz    = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto lfoHz = [] (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; };
    auto one   = [] (float v, int) { return String (v, 1); };
    auto scan  = [] (float v, int)
    {
        if (std::abs (v) < 0.005f) return String ("Still");
        return String (v > 0.0f ? "Fwd " : "Back ") + String (std::abs (v), 2) + "x";
    };
    auto shape = [] (float v, int)
    {
        if (v < 0.25f) return String ("Smooth");
        if (v < 0.6f)  return String ("Flat");
        return String ("Pluck");
    };

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                         std::function<String (float, int)> fmt)
    {
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                          AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt))));
    };
    auto addChoice = [&] (const char* id, const char* name, const StringArray& items, int def)
    {
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    const StringArray targets = destNames();
    const StringArray lfoShapes { "Sine", "Triangle", "Square", "Random" };

    // Grains
    addFloat  ("size",      "Grain size", skewed (10.0f, 500.0f, 90.0f), 120.0f, ms);
    addFloat  ("density",   "Density",    skewed (1.0f, 60.0f, 12.0f), 14.0f, perS);
    addChoice ("rhythm",    "Rhythm",     { "Free", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 0);
    addFloat  ("position",  "Position",   skewed (0.0f, 2000.0f, 300.0f), 200.0f, ms);
    addFloat  ("spray",     "Spray",      skewed (0.0f, 1000.0f, 150.0f), 250.0f, ms);
    addFloat  ("scan",      "Scan",       NormalisableRange<float> (-1.0f, 1.0f), 0.0f, scan);
    addFloat  ("shape",     "Shape",      NormalisableRange<float> (0.0f, 1.0f), 0.0f, shape);
    addFloat  ("feedback",  "Feedback",   NormalisableRange<float> (0.0f, 0.9f), 0.2f, pct);

    // Pitch
    addFloat  ("pitch",     "Pitch",      NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, semis);
    addFloat  ("pitchrand", "Random",     NormalisableRange<float> (0.0f, 24.0f), 0.0f, range);
    addChoice ("scale",     "Scale",      { "Free", "Octaves", "Fifths", "Major", "Minor", "Penta" }, 0);
    addFloat  ("reverse",   "Reverse",    NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct);
    addFloat  ("spread",    "Spread",     NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);

    // Distortion
    addChoice ("type",      "Type",       { "Soft", "Hard", "Fold", "Crush" }, 0);
    addFloat  ("drive",     "Drive",      NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);

    // Filter
    addChoice ("fmode",     "Filter",     { "Low", "Band", "High" }, 0);
    addFloat  ("cutoff",    "Cutoff",     skewed (40.0f, 20000.0f, 1000.0f), 20000.0f, hz);
    addFloat  ("reso",      "Resonance",  skewed (0.5f, 10.0f, 2.0f), 0.7f, one);

    // Space
    addFloat  ("dtime",     "Delay",      skewed (20.0f, 1500.0f, 300.0f), 375.0f, ms);
    addFloat  ("dfeedback", "Echo feedback", NormalisableRange<float> (0.0f, 0.9f), 0.35f, pct);
    addFloat  ("echo",      "Echo",       NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct);
    addFloat  ("room",      "Room",       NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct);
    addFloat  ("reverb",    "Reverb",     NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct);

    // Motion
    addChoice ("l1shape",   "LFO 1 shape",  lfoShapes, 0);
    addFloat  ("l1rate",    "LFO 1 rate",   skewed (0.02f, 20.0f, 1.0f), 0.2f, lfoHz);
    addFloat  ("l1depth",   "LFO 1 depth",  NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
    addChoice ("l1target",  "LFO 1 target", targets, dPosition);
    addChoice ("l2shape",   "LFO 2 shape",  lfoShapes, 3);
    addFloat  ("l2rate",    "LFO 2 rate",   skewed (0.02f, 20.0f, 1.0f), 2.0f, lfoHz);
    addFloat  ("l2depth",   "LFO 2 depth",  NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    addChoice ("l2target",  "LFO 2 target", targets, dPitch);

    // XY pad
    addFloat  ("padx",      "Pad X",      NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    addFloat  ("pady",      "Pad Y",      NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    addChoice ("padxtarget", "Pad X target", targets, dSpray);
    addChoice ("padytarget", "Pad Y target", targets, dSize);

    // Output
    addFloat  ("mix",       "Mix",        NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
    addFloat  ("output",    "Output",     NormalisableRange<float> (-24.0f, 12.0f), 0.0f, db);
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));

    return { ps.begin(), ps.end() };
}

//==============================================================================
void GrainProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    bufLen = juce::jmax (1024, (int) (sr * bufferSeconds));
    bufL.assign ((size_t) bufLen, 0.0f);
    bufR.assign ((size_t) bufLen, 0.0f);
    writePos = 0;

    dlyLen = juce::jmax (1024, (int) (sr * 1.6));
    dlyL.assign ((size_t) dlyLen, 0.0f);
    dlyR.assign ((size_t) dlyLen, 0.0f);
    dlyWrite = 0;

    for (auto& g : grains) g.active = false;
    spawnCounter = 0.0;
    internalBeat = 0.0;
    lastGrid = -1;
    scanOffset = 0.0;
    colPeak = 0.0f;
    lastCol = 0;
    crushL = crushR = 0.0f;
    crushCount = 0;
    filtL.reset();
    filtR.reset();
    lfoPhase[0] = lfoPhase[1] = 0.0;

    reverb.setSampleRate (sr);
    reverb.reset();

    mixSmooth.reset (sr, 0.05);
    outSmooth.reset (sr, 0.05);
    driveSmooth.reset (sr, 0.05);
    echoSmooth.reset (sr, 0.05);
    dlyTimeSmooth.reset (sr, 0.25);
    mixSmooth.setCurrentAndTargetValue (raw[pMix]->load());
    outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw[pOutput]->load()));
    driveSmooth.setCurrentAndTargetValue (raw[pDrive]->load());
    echoSmooth.setCurrentAndTargetValue (raw[pEcho]->load());
    dlyTimeSmooth.setCurrentAndTargetValue (raw[pDelayTime]->load() * 0.001f * (float) sr);

    for (auto& c : columnPeaks) c.store (0.0f);
    for (auto& g : grainPos)    g.store (-1.0f);
    headColumn.store (0);
}

bool GrainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
float GrainProcessor::readInterp (const std::vector<float>& buf, int len, double pos) noexcept
{
    const int i0 = (int) pos;
    const int i1 = (i0 + 1 >= len) ? 0 : i0 + 1;
    const float frac = (float) (pos - (double) i0);
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void GrainProcessor::rebuildWindow (float s)
{
    // 0 = smooth bell, 0.5 = flat top with short fades, 1 = sharp pluck with a fast decay
    const int n = (int) window.size();
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) (n - 1);
        float w;
        if (s <= 0.5f)
        {
            const float edge = 0.5f * (1.0f - 1.8f * s);
            if (t < edge)             w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t / edge);
            else if (t > 1.0f - edge) w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (1.0f - t) / edge);
            else                      w = 1.0f;
        }
        else
        {
            const float k = (s - 0.5f) * 18.0f;
            const float attack = 0.015f;
            const float env = t < attack ? t / attack : std::exp (-k * (t - attack) / (1.0f - attack));
            w = env * juce::jmin (1.0f, (1.0f - t) / 0.05f);
        }
        window[(size_t) i] = w;
        sum += w;
    }
    const float mean = (float) (sum / n);
    windowComp = std::sqrt (0.5f / juce::jmax (0.05f, mean));   // keep plucky grains as loud as smooth ones
    windowShape = s;
}

float GrainProcessor::lfoValue (int k, int shape) const noexcept
{
    const double p = lfoPhase[k];
    switch (shape)
    {
        case 0:  return (float) std::sin (juce::MathConstants<double>::twoPi * p);
        case 1:  return (float) (1.0 - 4.0 * std::abs (p - 0.5));
        case 2:  return p < 0.5 ? 1.0f : -1.0f;
        default: return lfoRand[k];
    }
}

void GrainProcessor::spawnGrain (const Live& v, int scale, float reverse, bool frozen)
{
    Grain* g = nullptr;
    for (auto& candidate : grains)
        if (! candidate.active) { g = &candidate; break; }
    if (g == nullptr) return;

    const float offset = quantizeToScale (v.random * (rng.nextFloat() * 2.0f - 1.0f), scale);
    const float semis = juce::jlimit (-36.0f, 36.0f, v.pitch + offset);
    const double rate = std::exp2 ((double) semis / 12.0);
    const double inc = (rng.nextFloat() < reverse) ? -rate : rate;

    // How fast the read point moves towards the record head (negative = away from it).
    // The grain must stay inside recorded audio for its whole life.
    const double drift = inc - (frozen ? 0.0 : 1.0);

    double len = juce::jmax (32.0, (double) v.sizeMs * 0.001 * sr);
    double lo = 0.0, hi = 0.0;
    for (int tries = 0; tries < 10; ++tries)
    {
        lo = 4.0 + juce::jmax (0.0, drift) * len;
        hi = (double) bufLen - 4.0 + juce::jmin (0.0, drift) * len;
        if (hi > lo) break;
        len *= 0.5;
    }
    if (hi <= lo) return;

    const double distance = juce::jlimit (lo, hi, (double) v.positionMs * 0.001 * sr + scanOffset
                                                     + (double) v.sprayMs * 0.001 * sr * rng.nextDouble());

    double p = (double) writePos - distance;
    while (p < 0.0) p += bufLen;

    const float pan = v.spread * (rng.nextFloat() * 2.0f - 1.0f);
    g->pos = p;
    g->inc = inc;
    g->length = (int) len;
    g->age = 0;
    g->gainL = juce::jmin (1.0f, 1.0f - pan);
    g->gainR = juce::jmin (1.0f, 1.0f + pan);
    g->semis = semis;
    g->active = true;
}

//==============================================================================
void GrainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numCh == 0 || bufLen == 0) return;

    float* left  = buffer.getWritePointer (0);
    float* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    // Settings that are not modulated
    const int   rhythm     = juce::jlimit (0, 6, (int) raw[pRhythm]->load());
    const float feedback   = raw[pFeedback]->load();
    const int   scale      = juce::jlimit (0, 5, (int) raw[pScale]->load());
    const float reverse    = raw[pReverse]->load();
    const int   type       = juce::jlimit (0, 3, (int) raw[pType]->load());
    const int   filterMode = juce::jlimit (0, 2, (int) raw[pFilterMode]->load());
    const float reso       = raw[pReso]->load();
    const float delayFb    = raw[pDelayFb]->load();
    const float room       = raw[pRoom]->load();
    const bool  frozen     = raw[pFreeze]->load() > 0.5f;

    const int   lfoShape[2]  = { (int) raw[pLfo1Shape]->load(), (int) raw[pLfo2Shape]->load() };
    const float lfoRate[2]   = { raw[pLfo1Rate]->load(), raw[pLfo2Rate]->load() };
    const float lfoDepth[2]  = { raw[pLfo1Depth]->load(), raw[pLfo2Depth]->load() };
    const int   lfoTarget[2] = { (int) raw[pLfo1Target]->load(), (int) raw[pLfo2Target]->load() };
    const float padX = raw[pPadX]->load(), padY = raw[pPadY]->load();
    const int   padXTarget = (int) raw[pPadXTarget]->load(), padYTarget = (int) raw[pPadYTarget]->load();

    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (raw[pOutput]->load()));
    dlyTimeSmooth.setTargetValue (juce::jmin ((float) dlyLen - 4.0f, raw[pDelayTime]->load() * 0.001f * (float) sr));

    // Host tempo, for Rhythm
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, hasPpq = false;
    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto pos = hostPlayHead->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
        }
    }
    const double samplesPerBeat = 60.0 / bpm * sr;

    const int hannMax = (int) window.size() - 1;
    constexpr int chunk = 32;
    float wetL[chunk], wetR[chunk];

    for (int start = 0; start < numSamples; start += chunk)
    {
        const int n = juce::jmin (chunk, numSamples - start);

        // ---- Modulation: two LFOs plus the XY pad, in normalised parameter units ----
        std::array<float, numDest> offs {};
        for (int k = 0; k < 2; ++k)
        {
            if (lfoTarget[k] > 0 && lfoTarget[k] < numDest && lfoDepth[k] > 0.0f)
                offs[(size_t) lfoTarget[k]] += lfoValue (k, lfoShape[k]) * lfoDepth[k] * 0.5f;

            lfoPhase[k] += (double) lfoRate[k] * n / sr;
            if (lfoPhase[k] >= 1.0)
            {
                lfoPhase[k] -= std::floor (lfoPhase[k]);
                lfoRandTarget[k] = rng.nextFloat() * 2.0f - 1.0f;
            }
            lfoRand[k] += (lfoRandTarget[k] - lfoRand[k]) * juce::jmin (1.0f, (float) n / (0.02f * (float) sr));
        }
        if (padXTarget > 0 && padXTarget < numDest) offs[(size_t) padXTarget] += padX - 0.5f;
        if (padYTarget > 0 && padYTarget < numDest) offs[(size_t) padYTarget] += padY - 0.5f;

        auto eff = [&] (int dest) -> float
        {
            const int p = kDestParam[dest];
            const float base = raw[(size_t) p]->load();
            if (std::abs (offs[(size_t) dest]) < 1.0e-6f)
            {
                modDisplay[(size_t) dest].store (-1.0f, std::memory_order_relaxed);
                return base;
            }
            const float norm = juce::jlimit (0.0f, 1.0f, prm[(size_t) p]->convertTo0to1 (base) + offs[(size_t) dest]);
            modDisplay[(size_t) dest].store (norm, std::memory_order_relaxed);
            return prm[(size_t) p]->convertFrom0to1 (norm);
        };

        Live v;
        v.positionMs = eff (dPosition);
        v.sprayMs    = eff (dSpray);
        v.sizeMs     = eff (dSize);
        v.density    = juce::jmax (0.5f, eff (dDensity));
        v.scan       = eff (dScan);
        v.shape      = eff (dShape);
        v.pitch      = eff (dPitch);
        v.random     = eff (dRandom);
        v.spread     = eff (dSpread);
        v.drive      = eff (dDrive);
        v.cutoff     = eff (dCutoff);
        v.echo       = eff (dEcho);
        v.reverb     = eff (dReverb);
        v.mix        = eff (dMix);

        if (std::abs (v.shape - windowShape) > 0.002f)
            rebuildWindow (v.shape);

        // Scan slowly moves where grains start; at zero it glides back to the Position setting
        const double scanRange = 2.0 * sr;
        if (std::abs (v.scan) > 0.005f)
        {
            scanOffset -= (double) v.scan * n;
            while (scanOffset < 0.0)        scanOffset += scanRange;
            while (scanOffset >= scanRange) scanOffset -= scanRange;
        }
        else
        {
            scanOffset *= std::exp (-(double) n / (0.25 * sr));
        }
        visStartSec.store (v.positionMs * 0.001f + (float) (scanOffset / sr), std::memory_order_relaxed);
        visSpraySec.store (v.sprayMs * 0.001f, std::memory_order_relaxed);

        const double overlap = (double) v.density * (double) v.sizeMs * 0.001;
        const float norm = (float) std::pow (juce::jmax (1.0, overlap * 0.5), -0.65) * windowComp;
        const double interval = sr / (double) v.density;

        filtL.set (v.cutoff, reso, (float) sr);
        filtR.set (v.cutoff, reso, (float) sr);
        const bool filterOn = ! (filterMode == 0 && v.cutoff > 19500.0f);

        driveSmooth.setTargetValue (v.drive);
        mixSmooth.setTargetValue (v.mix);
        echoSmooth.setTargetValue (v.echo);

        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.69f * room;
        rp.damping = 0.45f;
        rp.wetLevel = v.reverb * 0.3f;
        rp.dryLevel = 1.0f - v.reverb * 0.6f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        for (int j = 0; j < n; ++j)
        {
            const int i = start + j;
            const float inL = left[i];
            const float inR = right != nullptr ? right[i] : inL;

            // ---- Start grains: free-running density, or locked to the host tempo ----
            if (rhythm == 0)
            {
                spawnCounter -= 1.0;
                if (spawnCounter <= 0.0)
                {
                    spawnGrain (v, scale, reverse, frozen);
                    spawnCounter += interval * (0.75 + 0.5 * rng.nextDouble());
                }
            }
            else
            {
                const double beat = (playing && hasPpq) ? ppq + (double) i / samplesPerBeat : internalBeat;
                const auto grid = (juce::int64) std::floor (beat / kDivBeats[rhythm] + 1.0e-9);
                if (grid != lastGrid)
                {
                    spawnGrain (v, scale, reverse, frozen);
                    lastGrid = grid;
                }
            }
            internalBeat += 1.0 / samplesPerBeat;

            // ---- Play every active grain through its window ----
            float gL = 0.0f, gR = 0.0f;
            for (auto& g : grains)
            {
                if (! g.active) continue;

                const float w = window[(size_t) (((juce::int64) g.age * hannMax) / g.length)];
                gL += readInterp (bufL, bufLen, g.pos) * w * g.gainL;
                gR += readInterp (bufR, bufLen, g.pos) * w * g.gainR;

                g.pos += g.inc;
                if (g.pos >= bufLen) g.pos -= bufLen;
                else if (g.pos < 0.0) g.pos += bufLen;

                if (++g.age >= g.length) g.active = false;
            }
            gL *= norm;
            gR *= norm;

            // ---- Record the input (plus feedback) unless frozen ----
            if (! frozen)
            {
                float wL = inL + feedback * gL;
                float wR = inR + feedback * gR;
                if (feedback > 0.001f) { wL = std::tanh (wL); wR = std::tanh (wR); }

                bufL[(size_t) writePos] = wL;
                bufR[(size_t) writePos] = wR;
                colPeak = juce::jmax (colPeak, 0.5f * (std::abs (wL) + std::abs (wR)));

                if (++writePos >= bufLen) writePos = 0;

                const int col = (int) (((juce::int64) writePos * numColumns) / bufLen);
                if (col != lastCol)
                {
                    columnPeaks[(size_t) lastCol].store (colPeak, std::memory_order_relaxed);
                    colPeak = 0.0f;
                    lastCol = col;
                    headColumn.store (col, std::memory_order_relaxed);
                }
            }

            // ---- Distortion ----
            const float drive = driveSmooth.getNextValue();
            float yL, yR;
            if (type == 3)
            {
                const float levels = std::exp2 (15.0f - 13.0f * drive);
                const int hold = 1 + (int) (drive * drive * 40.0f);
                if (++crushCount >= hold)
                {
                    crushCount = 0;
                    crushL = std::round (gL * levels) / levels;
                    crushR = std::round (gR * levels) / levels;
                }
                yL = crushL;
                yR = crushR;
            }
            else
            {
                const float gain = std::pow (10.0f, drive * 1.5f);
                const float comp = 1.0f / std::sqrt (gain);
                yL = shapeSample (type, gL * gain) * comp;
                yR = shapeSample (type, gR * gain) * comp;
            }

            // ---- Filter ----
            if (filterOn)
            {
                yL = filtL.process (yL, filterMode);
                yR = filtR.process (yR, filterMode);
            }

            // ---- Echo: ping-pong delay ----
            const float dt = dlyTimeSmooth.getNextValue();
            double rpos = (double) dlyWrite - (double) dt;
            if (rpos < 0.0) rpos += dlyLen;
            const float dL = readInterp (dlyL, dlyLen, rpos);
            const float dR = readInterp (dlyR, dlyLen, rpos);
            dlyL[(size_t) dlyWrite] = std::tanh (yL + delayFb * dR);
            dlyR[(size_t) dlyWrite] = std::tanh (yR + delayFb * dL);
            if (++dlyWrite >= dlyLen) dlyWrite = 0;

            const float e = echoSmooth.getNextValue();
            wetL[j] = yL * (1.0f - 0.35f * e) + e * 0.7f * dL;
            wetR[j] = yR * (1.0f - 0.35f * e) + e * 0.7f * dR;
        }

        // ---- Reverb on the grains ----
        reverb.processStereo (wetL, wetR, n);

        // ---- Blend with the original ----
        for (int j = 0; j < n; ++j)
        {
            const int i = start + j;
            const float m = mixSmooth.getNextValue();
            const float o = outSmooth.getNextValue();
            const float inL = left[i];
            const float inR = right != nullptr ? right[i] : inL;
            left[i] = (inL * (1.0f - m) + safetyLimit (wetL[j]) * m) * o;
            if (right != nullptr)
                right[i] = (inR * (1.0f - m) + safetyLimit (wetR[j]) * m) * o;
        }
    }

    for (int ch = 2; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    // ---- Publish grains for the display ----
    for (size_t k = 0; k < grains.size(); ++k)
    {
        const auto& g = grains[k];
        if (g.active)
        {
            double dist = (double) writePos - g.pos;
            if (dist < 0.0) dist += bufLen;
            const float level = window[(size_t) (((juce::int64) g.age * hannMax) / juce::jmax (1, g.length))];
            grainPos[k].store ((float) (dist / bufLen), std::memory_order_relaxed);
            grainLevel[k].store (g.inc < 0.0 ? -level : level, std::memory_order_relaxed);
            grainPitch[k].store (g.semis, std::memory_order_relaxed);
        }
        else
        {
            grainPos[k].store (-1.0f, std::memory_order_relaxed);
        }
    }

    // ---- Send the output to the spectrum analyser ----
    int start1, size1, start2, size2;
    scopeFifo.prepareToWrite (numSamples, start1, size1, start2, size2);
    for (int i = 0; i < size1; ++i)
        scopeData[(size_t) (start1 + i)] = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
    for (int i = 0; i < size2; ++i)
    {
        const int s = size1 + i;
        scopeData[(size_t) (start2 + i)] = right != nullptr ? 0.5f * (left[s] + right[s]) : left[s];
    }
    scopeFifo.finishedWrite (size1 + size2);
}

//==============================================================================
int GrainProcessor::getNumPrograms()
{
    return (int) factoryPresets().size();
}

const juce::String GrainProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return (index >= 0 && index < (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void GrainProcessor::setCurrentProgram (int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= (int) presets.size()) return;
    currentProgram = index;

    // Everything except Freeze goes back to its default, then the preset's own settings apply
    for (int i = 0; i < numParams; ++i)
        if (i != pFreeze)
            prm[(size_t) i]->setValueNotifyingHost (prm[(size_t) i]->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void GrainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GrainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessorEditor* GrainProcessor::createEditor()
{
    return new GrainEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainProcessor();
}
