#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TalkEngine.h"
#include <array>
#include <atomic>
#include <memory>

// Shared, loaded once per process: the pronunciation dictionary
struct DictionaryHolder
{
    DictionaryHolder();
    talk::Dictionary dict;
};

class TalkProcessor : public juce::AudioProcessor
{
public:
    enum ParamIndex
    {
        pPitch, pSpeed, pFormant, pInflection, pIntonation, pEffort,
        pMode, pHold, pGlide, pVibrato, pLofi, pVolume,
        numParams
    };

    static const char* paramId (int index);
    static juce::StringArray voiceNames();

    TalkProcessor();
    ~TalkProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TALKFORFEIT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    // Programs are the voice personalities
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentVoice; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Called from the editor (message thread) ----
    void setText (const juce::String& newText);
    juce::String getText() const { return text; }
    const talk::Utterance* getDisplayUtterance() const { return displayUtterance.get(); }
    void requestTalk()  { talkRequested.store (true); }
    void requestStop()  { stopRequested.store (true); }
    bool renderToFile (const juce::File& file);   // speaks the whole text into a WAV file

    juce::AudioProcessorValueTreeState apvts;

    // ---- Live feedback for the display ----
    std::atomic<int> currentPhone { -1 };
    std::atomic<int> textVersionPlaying { 0 };
    std::atomic<float> liveF1 { 500 }, liveF2 { 1500 }, liveLevel { 0 };
    static constexpr int scopeSize = 4096;
    juce::AbstractFifo scopeFifo { scopeSize };
    std::array<float, scopeSize> scopeData {};
    int getTextVersion() const { return textVersion; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    talk::VoiceParams currentVoiceParams() const;
    void handleMidi (const juce::MidiMessage& msg);
    void startUnit (float noteHz, float velocity);
    void render (float* const* channels, int numChannels, int start, int num);

    juce::SharedResourcePointer<DictionaryHolder> dictionary;
    std::array<std::atomic<float>*, numParams> raw {};

    juce::String text;
    int textVersion = 0;
    std::unique_ptr<talk::Utterance> displayUtterance;   // message thread only

    // Hand-over of new text to the audio thread
    juce::SpinLock utteranceLock;
    std::unique_ptr<talk::Utterance> pendingUtterance, audioUtterance, retiredUtterance;
    int pendingVersion = 0;

    talk::Speaker speaker;
    std::vector<float> mono;
    double sr = 44100.0;

    int heldNotes[16] {};
    int numHeld = 0;
    int unitCursor = 0;
    bool wasPlaying = false;
    float bendSemis = 0.0f;

    std::atomic<bool> talkRequested { false }, stopRequested { false };
    int currentVoice = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TalkProcessor)
};
