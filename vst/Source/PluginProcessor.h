#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

namespace bff
{
// Topology-preserving state variable filter (Zavalishin). Gives low, band and high outputs.
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

    void process (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = v0 - k * v1 - v2;
    }
};
} // namespace bff

class BassforfeitProcessor : public juce::AudioProcessor
{
public:
    BassforfeitProcessor();
    ~BassforfeitProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BASSFORFEIT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::StringArray presetNames();

    juce::AudioProcessorValueTreeState apvts;

    static constexpr int numParams = 18;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void handleMidi (const juce::MidiMessage& msg);
    void renderSamples (float* left, float* right, int start, int num);
    void addHeld (int note);
    void removeHeld (int note);

    std::array<std::atomic<float>*, numParams> params {};

    double sr = 44100.0;
    double bpm = 140.0;

    // Voice state (mono, last-note priority)
    float curFreq = 43.65f, targetFreq = 43.65f;
    float env = 0.0f, envTarget = 0.0f;
    int held[16] {};
    int numHeld = 0;

    double ph1 = 0.0, ph2 = 0.0, phSub = 0.0, phScream = 0.0, phFm = 0.0, lfoPhase = 0.0;

    bff::SVF filter, screamHP;
    bff::SVF formants[3];
    int formantCounter = 0;

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassforfeitProcessor)
};
