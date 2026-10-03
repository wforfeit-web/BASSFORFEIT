#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

class GrainProcessor : public juce::AudioProcessor
{
public:
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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- Visualizer feed: written by the audio thread, only read by the editor ----
    static constexpr double bufferSeconds = 4.0;
    static constexpr int numColumns = 384;
    static constexpr int maxGrains = 64;
    static constexpr int scopeSize = 8192;

    std::array<std::atomic<float>, numColumns> columnPeaks;  // loudness of each slice of the grain buffer
    std::atomic<int> headColumn { 0 };                       // slice currently being recorded
    std::array<std::atomic<float>, maxGrains> grainPos;      // 0 = now, 1 = 4 s ago, < 0 = not playing
    std::array<std::atomic<float>, maxGrains> grainLevel;    // current window level of each grain
    juce::AbstractFifo scopeFifo { scopeSize };              // output audio for the spectrum
    std::array<float, scopeSize> scopeData {};

private:
    enum ParamIndex
    {
        pSize, pDensity, pPosition, pSpray, pFeedback,
        pPitch, pPitchRand, pReverse, pSpread,
        pType, pDrive, pTone, pMix, pOutput, pFreeze,
        numParams
    };

    struct Grain
    {
        bool active = false;
        double pos = 0.0;   // read position in the buffer (samples)
        double inc = 1.0;   // read speed; negative plays backwards
        int length = 0;
        int age = 0;
        float gainL = 1.0f, gainR = 1.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void spawnGrain (float sizeMs, float positionMs, float sprayMs, float pitch,
                     float pitchRand, float reverse, float spread, bool frozen);
    float readBuffer (const std::vector<float>& buf, double pos) const noexcept;

    std::array<std::atomic<float>*, numParams> params {};

    double sr = 44100.0;
    std::vector<float> bufL, bufR;
    int bufLen = 0;
    int writePos = 0;

    std::array<Grain, maxGrains> grains;
    double spawnCounter = 0.0;

    float colPeak = 0.0f;
    int lastCol = 0;

    float toneL = 0.0f, toneR = 0.0f;
    float crushL = 0.0f, crushR = 0.0f;
    int crushCount = 0;

    juce::SmoothedValue<float> mixSmooth, outSmooth, driveSmooth;
    std::array<float, 2048> hann {};
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainProcessor)
};
