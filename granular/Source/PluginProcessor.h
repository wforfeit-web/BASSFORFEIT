#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

namespace gf
{
// Topology-preserving state variable filter (Zavalishin): low, band and high outputs
struct SVF
{
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;

    void set (float cutoffHz, float q, float sampleRate)
    {
        cutoffHz = juce::jlimit (10.0f, sampleRate * 0.45f, cutoffHz);
        g  = std::tan (juce::MathConstants<float>::pi * cutoffHz / sampleRate);
        k  = 1.0f / juce::jmax (0.1f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() { ic1 = ic2 = 0.0f; }

    float process (float v0, int mode)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        switch (mode)
        {
            case 1:  return v1 * k * 1.6f;          // band-pass, with makeup gain
            case 2:  return v0 - k * v1 - v2;       // high-pass
            default: return v2;                     // low-pass
        }
    }
};
} // namespace gf

class GrainProcessor : public juce::AudioProcessor
{
public:
    enum ParamIndex
    {
        pSize, pDensity, pRhythm, pPosition, pSpray, pScan, pShape, pFeedback,
        pPitch, pRandom, pScale, pReverse, pSpread,
        pType, pDrive,
        pFilterMode, pCutoff, pReso,
        pDelayTime, pDelayFb, pEcho, pRoom, pReverb,
        pLfo1Shape, pLfo1Rate, pLfo1Depth, pLfo1Target,
        pLfo2Shape, pLfo2Rate, pLfo2Depth, pLfo2Target,
        pPadX, pPadY, pPadXTarget, pPadYTarget,
        pMix, pOutput, pFreeze,
        numParams
    };

    // Things the LFOs and the XY pad can move
    enum Dest
    {
        dNone, dPosition, dSpray, dSize, dDensity, dScan, dShape, dPitch, dRandom,
        dSpread, dDrive, dCutoff, dEcho, dReverb, dMix,
        numDest
    };

    static const char* paramId (int index);
    static int destParam (int dest);            // ParamIndex moved by a destination, or -1
    static juce::StringArray destNames();
    static juce::StringArray presetNames();

    GrainProcessor();
    ~GrainProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRAINFORFEIT"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- Visualizer feed: written by the audio thread, only read by the editor ----
    static constexpr double bufferSeconds = 4.0;
    static constexpr int numColumns = 384;
    static constexpr int maxGrains = 96;
    static constexpr int scopeSize = 8192;

    std::array<std::atomic<float>, numColumns> columnPeaks;  // loudness of each slice of the recording
    std::atomic<int> headColumn { 0 };                       // slice currently being recorded
    std::array<std::atomic<float>, maxGrains> grainPos;      // 0 = now, 1 = 4 s ago, < 0 = not playing
    std::array<std::atomic<float>, maxGrains> grainLevel;    // window level; negative = playing backwards
    std::array<std::atomic<float>, maxGrains> grainPitch;    // semitones
    std::atomic<float> visStartSec { 0.2f }, visSpraySec { 0.25f };
    std::array<std::atomic<float>, numDest> modDisplay;      // modulated position 0..1, < 0 = not modulated
    juce::AbstractFifo scopeFifo { scopeSize };
    std::array<float, scopeSize> scopeData {};

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0;    // read position in the recording (samples)
        double inc = 1.0;    // read speed; negative plays backwards
        int length = 0;
        int age = 0;
        float gainL = 1.0f, gainR = 1.0f;
        float semis = 0.0f;
    };

    // Parameter values after modulation, refreshed every few milliseconds
    struct Live
    {
        float sizeMs, density, positionMs, sprayMs, scan, shape, pitch, random,
              spread, drive, cutoff, echo, reverb, mix;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void spawnGrain (const Live& v, int scale, float reverse, bool frozen);
    void rebuildWindow (float shape);
    float lfoValue (int k, int shape) const noexcept;
    static float readInterp (const std::vector<float>& buf, int len, double pos) noexcept;

    std::array<std::atomic<float>*, numParams> raw {};
    std::array<juce::RangedAudioParameter*, numParams> prm {};

    double sr = 44100.0;

    std::vector<float> bufL, bufR;
    int bufLen = 0, writePos = 0;
    std::array<Grain, maxGrains> grains;

    double spawnCounter = 0.0;
    double internalBeat = 0.0;
    juce::int64 lastGrid = -1;
    double scanOffset = 0.0;   // samples, added to Position by Scan

    float colPeak = 0.0f;
    int lastCol = 0;

    std::array<float, 1024> window {};
    float windowShape = -1.0f;
    float windowComp = 1.0f;

    gf::SVF filtL, filtR;
    float crushL = 0.0f, crushR = 0.0f;
    int crushCount = 0;

    std::vector<float> dlyL, dlyR;
    int dlyLen = 0, dlyWrite = 0;

    juce::Reverb reverb;

    juce::SmoothedValue<float> mixSmooth, outSmooth, driveSmooth, echoSmooth, dlyTimeSmooth;

    double lfoPhase[2] { 0.0, 0.0 };
    float lfoRand[2] { 0.0f, 0.0f }, lfoRandTarget[2] { 0.0f, 0.0f };

    juce::Random rng;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainProcessor)
};
