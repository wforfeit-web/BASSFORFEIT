#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* const kIds[] = {
    "size", "density", "position", "spray", "feedback",
    "pitch", "pitchrand", "reverse", "spread",
    "type", "drive", "tone", "mix", "output", "freeze"
};

inline float shapeSample (int type, float x) noexcept
{
    switch (type)
    {
        case 0:  return std::tanh (x);                    // Soft: smooth saturation
        case 1:  return juce::jlimit (-1.0f, 1.0f, x);    // Hard: flat clipping
        default: return std::sin (x);                     // Fold: wavefolding
    }
}

inline float toneHz (float tone) noexcept
{
    return 300.0f * std::pow (20000.0f / 300.0f, tone);
}
} // namespace

//==============================================================================
GrainProcessor::GrainProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < numParams; ++i)
        params[(size_t) i] = apvts.getRawParameterValue (kIds[i]);

    for (auto& c : columnPeaks) c.store (0.0f);
    for (auto& g : grainPos)    g.store (-1.0f);
    for (auto& g : grainLevel)  g.store (0.0f);

    for (size_t i = 0; i < hann.size(); ++i)
        hann[i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (hann.size() - 1));
}

juce::AudioProcessorValueTreeState::ParameterLayout GrainProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto ms    = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; };
    auto pct   = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto perS  = [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " /s"; };
    auto semis = [] (float v, int) { const int s = roundToInt (v); return (s > 0 ? "+" : "") + String (s) + " st"; };
    auto range = [] (float v, int) { return String::charToString ((juce_wchar) 0x00B1) + String (v, 1) + " st"; };
    auto db    = [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; };
    auto hz    = [] (float v, int)
    {
        if (v > 0.995f) return String ("Open");
        const float f = toneHz (v);
        return f >= 1000.0f ? String (f / 1000.0f, 1) + " kHz" : String (roundToInt (f)) + " Hz";
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

    addFloat ("size",      "Grain size", skewed (10.0f, 500.0f, 90.0f), 80.0f, ms);
    addFloat ("density",   "Density",    skewed (1.0f, 60.0f, 12.0f), 12.0f, perS);
    addFloat ("position",  "Position",   skewed (0.0f, 2000.0f, 300.0f), 150.0f, ms);
    addFloat ("spray",     "Spray",      skewed (0.0f, 1000.0f, 150.0f), 120.0f, ms);
    addFloat ("feedback",  "Feedback",   NormalisableRange<float> (0.0f, 0.9f), 0.2f, pct);
    addFloat ("pitch",     "Pitch",      NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, semis);
    addFloat ("pitchrand", "Random",     NormalisableRange<float> (0.0f, 12.0f), 0.0f, range);
    addFloat ("reverse",   "Reverse",    NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct);
    addFloat ("spread",    "Spread",     NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);

    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "type", 1 }, "Type",
                                                          StringArray { "Soft", "Hard", "Fold", "Crush" }, 0));

    addFloat ("drive",  "Drive",  NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct);
    addFloat ("tone",   "Tone",   NormalisableRange<float> (0.0f, 1.0f), 1.0f, hz);
    addFloat ("mix",    "Mix",    NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);
    addFloat ("output", "Output", NormalisableRange<float> (-24.0f, 12.0f), 0.0f, db);

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

    for (auto& g : grains) g.active = false;
    spawnCounter = 0.0;
    colPeak = 0.0f;
    lastCol = 0;
    toneL = toneR = crushL = crushR = 0.0f;
    crushCount = 0;

    mixSmooth.reset (sr, 0.05);
    outSmooth.reset (sr, 0.05);
    driveSmooth.reset (sr, 0.05);
    mixSmooth.setCurrentAndTargetValue (params[pMix]->load());
    outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params[pOutput]->load()));
    driveSmooth.setCurrentAndTargetValue (params[pDrive]->load());

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
float GrainProcessor::readBuffer (const std::vector<float>& buf, double pos) const noexcept
{
    const int i0 = (int) pos;
    const int i1 = (i0 + 1 >= bufLen) ? 0 : i0 + 1;
    const float frac = (float) (pos - (double) i0);
    return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void GrainProcessor::spawnGrain (float sizeMs, float positionMs, float sprayMs, float pitch,
                                 float pitchRand, float reverse, float spread, bool frozen)
{
    Grain* g = nullptr;
    for (auto& candidate : grains)
        if (! candidate.active) { g = &candidate; break; }
    if (g == nullptr) return;

    const float semis = juce::jlimit (-24.0f, 24.0f, pitch + pitchRand * (rng.nextFloat() * 2.0f - 1.0f));
    const double rate = std::exp2 ((double) semis / 12.0);
    const double inc = (rng.nextFloat() < reverse) ? -rate : rate;

    // How fast the grain's read point moves towards the record head (negative = away from it).
    // The grain must stay inside recorded audio for its whole life.
    const double drift = inc - (frozen ? 0.0 : 1.0);

    double len = juce::jmax (32.0, (double) sizeMs * 0.001 * sr);
    double lo = 0.0, hi = 0.0;
    for (int tries = 0; tries < 10; ++tries)
    {
        lo = 4.0 + juce::jmax (0.0, drift) * len;
        hi = (double) bufLen - 4.0 + juce::jmin (0.0, drift) * len;
        if (hi > lo) break;
        len *= 0.5;
    }
    if (hi <= lo) return;

    const double distance = juce::jlimit (lo, hi, ((double) positionMs + (double) sprayMs * rng.nextDouble()) * 0.001 * sr);

    double p = (double) writePos - distance;
    while (p < 0.0) p += bufLen;

    const float pan = spread * (rng.nextFloat() * 2.0f - 1.0f);
    g->pos = p;
    g->inc = inc;
    g->length = (int) len;
    g->age = 0;
    g->gainL = juce::jmin (1.0f, 1.0f - pan);
    g->gainR = juce::jmin (1.0f, 1.0f + pan);
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

    const float sizeMs     = params[pSize]->load();
    const float density    = juce::jmax (0.5f, params[pDensity]->load());
    const float positionMs = params[pPosition]->load();
    const float sprayMs    = params[pSpray]->load();
    const float feedback   = params[pFeedback]->load();
    const float pitch      = params[pPitch]->load();
    const float pitchRand  = params[pPitchRand]->load();
    const float reverse    = params[pReverse]->load();
    const float spread     = params[pSpread]->load();
    const int   type       = juce::jlimit (0, 3, (int) params[pType]->load());
    const float tone       = params[pTone]->load();
    const bool  frozen     = params[pFreeze]->load() > 0.5f;

    mixSmooth.setTargetValue (params[pMix]->load());
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (params[pOutput]->load()));
    driveSmooth.setTargetValue (params[pDrive]->load());

    // Keep the level steady however many grains overlap
    const double overlap = (double) density * (double) sizeMs * 0.001;
    const float norm = (float) std::pow (juce::jmax (1.0, overlap * 0.5), -0.65);
    const double interval = sr / (double) density;

    const bool toneOn = tone < 0.995f;
    const float toneA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                         * juce::jmin (toneHz (tone), (float) sr * 0.45f) / (float) sr);

    const int hannMax = (int) hann.size() - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = right != nullptr ? right[i] : inL;

        // Start new grains at the chosen density, with a little timing jitter
        spawnCounter -= 1.0;
        if (spawnCounter <= 0.0)
        {
            spawnGrain (sizeMs, positionMs, sprayMs, pitch, pitchRand, reverse, spread, frozen);
            spawnCounter += interval * (0.75 + 0.5 * rng.nextDouble());
        }

        // Play every active grain through its window
        float gL = 0.0f, gR = 0.0f;
        for (auto& g : grains)
        {
            if (! g.active) continue;

            const float w = hann[(size_t) (((juce::int64) g.age * hannMax) / g.length)];
            gL += readBuffer (bufL, g.pos) * w * g.gainL;
            gR += readBuffer (bufR, g.pos) * w * g.gainR;

            g.pos += g.inc;
            if (g.pos >= bufLen) g.pos -= bufLen;
            else if (g.pos < 0.0) g.pos += bufLen;

            if (++g.age >= g.length) g.active = false;
        }
        gL *= norm;
        gR *= norm;

        // Record the input (plus feedback) unless frozen
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

        // Distort the grains
        const float drive = driveSmooth.getNextValue();
        float yL, yR;
        if (type == 3)
        {
            // Crush: fewer bits and a lower sample rate as Drive goes up
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
            const float gain = std::pow (10.0f, drive * 1.5f);   // up to +30 dB into the shaper
            const float comp = 1.0f / std::sqrt (gain);
            yL = shapeSample (type, gL * gain) * comp;
            yR = shapeSample (type, gR * gain) * comp;
        }

        if (toneOn)
        {
            toneL += toneA * (yL - toneL);
            toneR += toneA * (yR - toneR);
            yL = toneL;
            yR = toneR;
        }

        const float mix = mixSmooth.getNextValue();
        const float out = outSmooth.getNextValue();
        left[i] = (inL * (1.0f - mix) + yL * mix) * out;
        if (right != nullptr)
            right[i] = (inR * (1.0f - mix) + yR * mix) * out;
    }

    for (int ch = 2; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Publish grain positions for the display
    for (size_t k = 0; k < grains.size(); ++k)
    {
        const auto& g = grains[k];
        if (g.active)
        {
            double dist = (double) writePos - g.pos;
            if (dist < 0.0) dist += bufLen;
            grainPos[k].store ((float) (dist / bufLen), std::memory_order_relaxed);
            grainLevel[k].store (hann[(size_t) (((juce::int64) g.age * hannMax) / juce::jmax (1, g.length))],
                                 std::memory_order_relaxed);
        }
        else
        {
            grainPos[k].store (-1.0f, std::memory_order_relaxed);
        }
    }

    // Send the output to the spectrum analyser
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
void GrainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void GrainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* GrainProcessor::createEditor()
{
    return new GrainEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainProcessor();
}
