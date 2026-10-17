#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
const char* const kIds[TalkProcessor::numParams] = {
    "pitch", "speed", "formant", "inflection", "intonation", "effort",
    "mode", "hold", "glide", "vibrato", "lofi", "volume"
};

// Voice personalities: pitch (Hz), speed, formant, inflection, intonation, effort, vibrato, lo-fi
struct Voice
{
    const char* name;
    float pitch, speed, formant, inflection;
    int intonation, effort;
    float vibrato, lofi;
};

const Voice kVoices[] = {
    { "Standard",      110.0f, 1.00f, 1.00f, 0.60f, 0, 0, 0.00f, 0.00f },
    { "Deep",           80.0f, 0.92f, 0.94f, 0.50f, 0, 0, 0.00f, 0.00f },
    { "Bright",        205.0f, 1.05f, 1.15f, 0.70f, 0, 0, 0.00f, 0.00f },
    { "Kid",           290.0f, 1.12f, 1.30f, 0.80f, 0, 0, 0.00f, 0.00f },
    { "Giant",          58.0f, 0.75f, 0.82f, 0.40f, 0, 0, 0.00f, 0.00f },
    { "Robot",         120.0f, 1.00f, 1.00f, 0.00f, 1, 0, 0.00f, 0.35f },
    { "Old Computer",  100.0f, 1.00f, 1.00f, 0.00f, 1, 0, 0.00f, 0.75f },
    { "Whisper",       110.0f, 0.92f, 1.05f, 0.50f, 0, 2, 0.00f, 0.00f },
    { "Breathy",       175.0f, 0.90f, 1.10f, 0.60f, 0, 1, 0.00f, 0.00f },
    { "Fast Talker",   135.0f, 1.60f, 1.02f, 0.50f, 0, 0, 0.00f, 0.00f },
    { "Singer",        150.0f, 0.90f, 1.00f, 0.00f, 2, 0, 0.45f, 0.00f },
    { "Choir Kid",     260.0f, 0.95f, 1.22f, 0.00f, 2, 1, 0.40f, 0.00f },
    { "Alien",         240.0f, 1.00f, 1.42f, 0.00f, 1, 0, 0.00f, 0.50f },
    { "Ghost",          95.0f, 0.80f, 0.88f, 0.40f, 0, 2, 0.00f, 0.20f },
};
constexpr int kNumVoices = (int) (sizeof (kVoices) / sizeof (kVoices[0]));

const char* const kDefaultText = "Hello. I am a talking computer. Type anything here, then press Talk.";

void writeWav16 (juce::OutputStream& out, const std::vector<float>& samples, int sampleRate)
{
    const auto dataBytes = (uint32_t) (samples.size() * 2);
    out.write ("RIFF", 4); out.writeInt ((int) (36 + dataBytes)); out.write ("WAVEfmt ", 8);
    out.writeInt (16); out.writeShort (1); out.writeShort (1);
    out.writeInt (sampleRate); out.writeInt (sampleRate * 2); out.writeShort (2); out.writeShort (16);
    out.write ("data", 4); out.writeInt ((int) dataBytes);
    for (float v : samples)
        out.writeShort ((short) juce::roundToInt (juce::jlimit (-1.0f, 1.0f, v) * 32767.0f));
}
} // namespace

//==============================================================================
DictionaryHolder::DictionaryHolder()
{
    juce::MemoryInputStream compressed (BinaryData::cmudict_txt_gz, (size_t) BinaryData::cmudict_txt_gzSize, false);
    juce::GZIPDecompressorInputStream gz (&compressed, false, juce::GZIPDecompressorInputStream::gzipFormat);
    juce::MemoryBlock block;
    gz.readIntoMemoryBlock (block);
    dict.load (std::string (static_cast<const char*> (block.getData()), block.getSize()));
}

//==============================================================================
const char* TalkProcessor::paramId (int index) { return kIds[index]; }

juce::StringArray TalkProcessor::voiceNames()
{
    juce::StringArray names;
    for (const auto& v : kVoices) names.add (v.name);
    return names;
}

TalkProcessor::TalkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < numParams; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (kIds[i]);

    setText (kDefaultText);
}

juce::AudioProcessorValueTreeState::ParameterLayout TalkProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto hz    = [] (float v, int) { return String (roundToInt (v)) + " Hz"; };
    auto times = [] (float v, int) { return String (v, 2) + "x"; };
    auto pct   = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto ms    = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
    auto db    = [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; };

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

    addFloat ("pitch",      "Pitch",      skewed (40.0f, 500.0f, 140.0f), 110.0f, hz);
    addFloat ("speed",      "Speed",      skewed (0.4f, 2.5f, 1.0f), 1.0f, times);
    addFloat ("formant",    "Formant",    skewed (0.7f, 1.5f, 1.0f), 1.0f, times);
    addFloat ("inflection", "Inflection", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "intonation", 1 }, "Intonation",
                                                          StringArray { "Natural", "Monotone", "Sung" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "effort", 1 }, "Effort",
                                                          StringArray { "Normal", "Breathy", "Whispered" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Play mode",
                                                          StringArray { "Phrase", "Word", "Syllable" }, 0));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 }, "Hold vowels", true));
    addFloat ("glide",      "Glide",      skewed (0.0f, 500.0f, 80.0f), 40.0f, ms);
    addFloat ("vibrato",    "Vibrato",    NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    addFloat ("lofi",       "Lo-fi",      NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
    addFloat ("volume",     "Volume",     NormalisableRange<float> (-24.0f, 6.0f), 0.0f, db);

    return { ps.begin(), ps.end() };
}

talk::VoiceParams TalkProcessor::currentVoiceParams() const
{
    talk::VoiceParams p;
    p.basePitch  = raw[pPitch]->load();
    p.speed      = raw[pSpeed]->load();
    p.formant    = raw[pFormant]->load();
    p.inflection = raw[pInflection]->load();
    p.intonation = juce::jlimit (0, 2, (int) raw[pIntonation]->load());
    p.effort     = juce::jlimit (0, 2, (int) raw[pEffort]->load());
    p.sustain    = raw[pHold]->load() > 0.5f;
    p.glideMs    = raw[pGlide]->load();
    p.vibrato    = raw[pVibrato]->load();
    p.lofi       = raw[pLofi]->load();
    p.gain       = juce::Decibels::decibelsToGain (raw[pVolume]->load());
    return p;
}

//==============================================================================
void TalkProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    speaker.prepare (sr);
    mono.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
    numHeld = 0;
    unitCursor = 0;

    const juce::SpinLock::ScopedLockType lock (utteranceLock);
    speaker.setUtterance (audioUtterance.get());
}

bool TalkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
void TalkProcessor::setText (const juce::String& newText)
{
    text = newText;
    ++textVersion;
    const auto s = text.toStdString();
    displayUtterance = talk::buildUtterance (s, dictionary->dict);
    auto forAudio = talk::buildUtterance (s, dictionary->dict);

    std::unique_ptr<talk::Utterance> toFree1, toFree2;   // freed here, never on the audio thread
    {
        const juce::SpinLock::ScopedLockType lock (utteranceLock);
        toFree1 = std::move (retiredUtterance);
        toFree2 = std::move (pendingUtterance);
        pendingUtterance = std::move (forAudio);
        pendingVersion = textVersion;
    }
}

void TalkProcessor::startUnit (float noteHz, float velocity)
{
    const auto* u = audioUtterance.get();
    if (u == nullptr || u->phones.empty() || u->words.empty()) return;

    const int mode = juce::jlimit (0, 2, (int) raw[pMode]->load());
    if (mode == 0)
    {
        speaker.trigger (0, (int) u->phones.size() - 1, u->words.back().sustain, noteHz, velocity);
    }
    else
    {
        const auto& units = mode == 1 ? u->words : u->syllables;
        if (units.empty()) return;
        const int idx = unitCursor % (int) units.size();
        unitCursor = idx + 1;
        speaker.trigger (units[(size_t) idx].first, units[(size_t) idx].last, units[(size_t) idx].sustain, noteHz, velocity);
    }
}

void TalkProcessor::handleMidi (const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        const int note = msg.getNoteNumber();
        const float hz = (float) juce::MidiMessage::getMidiNoteInHertz (note);
        const bool phraseMode = (int) raw[pMode]->load() == 0;
        const bool legato = phraseMode && numHeld > 0 && speaker.isSpeaking();

        for (int i = 0; i < numHeld; ++i)
            if (heldNotes[i] == note) { for (int j = i + 1; j < numHeld; ++j) heldNotes[j - 1] = heldNotes[j]; --numHeld; break; }
        if (numHeld < 16) heldNotes[numHeld++] = note;

        if (legato) speaker.setNote (hz);   // sing the phrase to a new note without restarting it
        else startUnit (hz, msg.getFloatVelocity());
    }
    else if (msg.isNoteOff())
    {
        const int note = msg.getNoteNumber();
        for (int i = 0; i < numHeld; ++i)
            if (heldNotes[i] == note) { for (int j = i + 1; j < numHeld; ++j) heldNotes[j - 1] = heldNotes[j]; --numHeld; break; }

        if (numHeld == 0) speaker.release();
        else if ((int) raw[pMode]->load() == 0)
            speaker.setNote ((float) juce::MidiMessage::getMidiNoteInHertz (heldNotes[numHeld - 1]));
    }
    else if (msg.isPitchWheel())
    {
        bendSemis = (float) (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f;
        speaker.setPitchBend (bendSemis);
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        numHeld = 0;
        speaker.stop();
    }
}

void TalkProcessor::render (float* const* channels, int numChannels, int start, int num)
{
    int done = 0;
    while (done < num)
    {
        const int n = juce::jmin (num - done, (int) mono.size());
        speaker.render (mono.data(), n);
        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::copy (channels[ch] + start + done, mono.data(), n);

        int s1, n1, s2, n2;
        scopeFifo.prepareToWrite (n, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) scopeData[(size_t) (s1 + i)] = mono[(size_t) i];
        for (int i = 0; i < n2; ++i) scopeData[(size_t) (s2 + i)] = mono[(size_t) (n1 + i)];
        scopeFifo.finishedWrite (n1 + n2);

        done += n;
    }
}

void TalkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Pick up newly typed text
    {
        const juce::SpinLock::ScopedTryLockType lock (utteranceLock);
        if (lock.isLocked() && pendingUtterance != nullptr)
        {
            retiredUtterance = std::move (audioUtterance);
            audioUtterance = std::move (pendingUtterance);
            speaker.setUtterance (audioUtterance.get());
            unitCursor = 0;
            textVersionPlaying.store (pendingVersion);
        }
    }

    speaker.setParams (currentVoiceParams());

    // Word and syllable modes start from the top whenever the host starts playing
    if (auto* hostPlayHead = getPlayHead())
        if (auto pos = hostPlayHead->getPosition())
        {
            const bool playing = pos->getIsPlaying();
            if (playing && ! wasPlaying) unitCursor = 0;
            wasPlaying = playing;
        }

    if (stopRequested.exchange (false)) speaker.stop();
    if (talkRequested.exchange (false) && audioUtterance != nullptr && ! audioUtterance->phones.empty())
    {
        speaker.trigger (0, (int) audioUtterance->phones.size() - 1, -1, 0.0f, 1.0f);
        speaker.release();
    }

    float* const* channels = buffer.getArrayOfWritePointers();
    const int numChannels = buffer.getNumChannels();

    int pos = 0;
    for (const auto meta : midi)
    {
        const int s = juce::jlimit (0, numSamples, meta.samplePosition);
        if (s > pos) render (channels, numChannels, pos, s - pos);
        handleMidi (meta.getMessage());
        pos = juce::jmax (pos, s);
    }
    if (pos < numSamples) render (channels, numChannels, pos, numSamples - pos);

    currentPhone.store (speaker.currentPhone());
    liveF1.store (speaker.currentF (0));
    liveF2.store (speaker.currentF (1));
    liveLevel.store (speaker.currentLevel());
}

//==============================================================================
bool TalkProcessor::renderToFile (const juce::File& file)
{
    auto u = talk::buildUtterance (text.toStdString(), dictionary->dict);
    if (u == nullptr || u->phones.empty()) return false;

    constexpr int rate = 44100;
    talk::Speaker offline;
    offline.prepare (rate);
    auto params = currentVoiceParams();
    params.sustain = false;
    offline.setParams (params);
    offline.setUtterance (u.get());
    offline.trigger (0, (int) u->phones.size() - 1, -1, 0.0f, 1.0f);
    offline.release();

    std::vector<float> samples ((size_t) rate / 10, 0.0f);
    std::vector<float> block (512);
    while (offline.isSpeaking() && samples.size() < (size_t) rate * 600)
    {
        offline.render (block.data(), (int) block.size());
        samples.insert (samples.end(), block.begin(), block.end());
    }
    samples.resize (samples.size() + rate / 10, 0.0f);

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk()) return false;
    writeWav16 (out, samples, rate);
    out.flush();
    return ! out.getStatus().failed();
}

//==============================================================================
int TalkProcessor::getNumPrograms() { return kNumVoices; }

const juce::String TalkProcessor::getProgramName (int index)
{
    return (index >= 0 && index < kNumVoices) ? juce::String (kVoices[index].name) : juce::String();
}

void TalkProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= kNumVoices) return;
    currentVoice = index;
    const auto& v = kVoices[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set ("pitch", v.pitch);
    set ("speed", v.speed);
    set ("formant", v.formant);
    set ("inflection", v.inflection);
    set ("intonation", (float) v.intonation);
    set ("effort", (float) v.effort);
    set ("vibrato", v.vibrato);
    set ("lofi", v.lofi);
}

void TalkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("text", text, nullptr);
    state.setProperty ("voice", currentVoice, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TalkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentVoice = juce::jlimit (0, kNumVoices - 1, (int) tree.getProperty ("voice", 0));
            const juce::String savedText = tree.getProperty ("text", kDefaultText).toString();
            apvts.replaceState (tree);
            setText (savedText);
        }
    }
}

juce::AudioProcessorEditor* TalkProcessor::createEditor()
{
    return new TalkEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TalkProcessor();
}
