#pragma once

// TALKFORFEIT speech engine: text to phonemes, and a real-time formant speech synthesizer.
// Plain C++17 with no JUCE dependency, so it can be tested on its own.

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace talk
{
// Tuning hooks: fixed values in the plugin, overridable from the environment in test builds
#ifdef TALK_TUNING
#include <cstdlib>
inline float tune (const char* name, float fallback) { const char* v = std::getenv (name); return v ? (float) std::atof (v) : fallback; }
#else
inline float tune (const char*, float fallback) { return fallback; }
#endif

//==============================================================================
// Phonemes (CMU / ARPAbet set) plus a silence
enum class Kind : uint8_t { Vowel, Diphthong, Stop, Fricative, Affricate, Nasal, Liquid, Glide, Aspirate, Pause };

struct PhonemeDef
{
    const char* name;
    Kind kind;
    bool voiced;
    float f1, f2, f3;          // formant targets (Hz, adult male)
    float e1, e2, e3;          // end targets for diphthongs (0 = none)
    float b1, b2, b3;          // formant bandwidths (Hz)
    float fricF, fricQ, af;    // frication noise band and level (fricatives, affricates, stop bursts)
    float dur;                 // inherent duration (ms)
    float closure;             // stops and affricates: silent or voice-bar closure (ms)
};

constexpr int numPhonemes = 40;   // 39 CMU phonemes + PAUSE
constexpr int PAUSE = 39;

const PhonemeDef& phoneme (int id);
int phonemeId (const std::string& name);   // "AA".."ZH" or "PAUSE"; -1 if unknown

//==============================================================================
struct Phone
{
    int id = PAUSE;
    float dur = 0.0f;          // ms at normal speed
    int stress = 0;            // 0 none, 1 primary, 2 secondary (vowels)
    int word = -1;             // index into Utterance::words (-1 for pauses)
    float pitchStart = 0.0f;   // natural intonation, as a relative offset (0.2 = 20% higher)
    float pitchEnd = 0.0f;
};

struct Range
{
    int first = 0, last = 0;   // phone indexes, inclusive
    int sustain = -1;          // phone held while a key is down (the last vowel), or -1
};

struct Utterance
{
    std::vector<Phone> phones;
    std::vector<Range> words;
    std::vector<Range> syllables;
    std::vector<std::string> wordText;   // as typed, for the display
    std::string phonemeText;             // e.g. "HH AH0 L OW1  W ER1 L D"
};

//==============================================================================
// Pronunciation dictionary (CMU Pronouncing Dictionary format: "WORD  P H O N E S")
class Dictionary
{
public:
    void load (const std::string& text);
    bool isLoaded() const { return ! entries.empty(); }
    const std::string* find (const std::string& upperWord) const;
    size_t size() const { return entries.size(); }

private:
    std::unordered_map<std::string, std::string> entries;
};

// Turns typed text into a timed, stressed phoneme sequence with words, syllables and intonation
std::unique_ptr<Utterance> buildUtterance (const std::string& text, const Dictionary& dict);

//==============================================================================
struct VoiceParams
{
    float basePitch = 110.0f;   // Hz, used by the Talk button and when not playing MIDI
    float speed = 1.0f;         // 0.4 .. 2.5
    float formant = 1.0f;       // formant scale ("head size"), 0.7 .. 1.5
    int intonation = 0;         // 0 Natural, 1 Monotone, 2 Sung
    int effort = 0;             // 0 Normal, 1 Breathy, 2 Whispered
    float inflection = 0.6f;    // how much natural intonation moves (0 .. 1)
    float vibrato = 0.0f;       // 0 .. 1
    float glideMs = 40.0f;      // pitch glide between notes
    bool sustain = true;        // hold the last vowel while a key is down
    float lofi = 0.0f;          // 0 .. 1: lower sample rate and bit depth
    float gain = 1.0f;          // linear output gain
};

// Real-time synthesizer. Not thread safe: call everything from the audio thread.
class Speaker
{
public:
    void prepare (double sampleRate);
    void setUtterance (const Utterance* u);   // caller keeps ownership
    void setParams (const VoiceParams& p) { params = p; }

    // Speak phones [first, last]. noteHz <= 0 uses the base pitch.
    void trigger (int first, int last, int sustainPhone, float noteHz, float velocity);
    void release();             // key up: let a held vowel finish
    void stop();                // fade out now
    void setPitchBend (float semitones) { bendSemis = semitones; }
    void setNote (float hz) { if (hz > 0.0f) noteHz = hz; }   // change pitch without restarting

    void render (float* out, int numSamples);   // mono, adds nothing: overwrites out

    bool isSpeaking() const { return active; }
    int currentPhone() const { return active ? phoneIndex : -1; }
    float currentF (int k) const { return curF[(size_t) k]; }
    float currentLevel() const { return level; }

private:
    struct Resonator
    {
        double a = 0, b = 0, c = 0, y1 = 0, y2 = 0;
        void set (double f, double bw, double sr);
        double process (double x) { const double y = a * x + b * y1 + c * y2; y2 = y1; y1 = y; return y; }
        void reset() { y1 = y2 = 0; }
    };

    struct AntiResonator   // Klatt antiresonator: a spectral zero (used for nasal murmur)
    {
        double a = 1, b = 0, c = 0, x1 = 0, x2 = 0;
        void set (double f, double bw, double sr);
        double process (double x) { const double y = a * x + b * x1 + c * x2; x2 = x1; x1 = x; return y; }
        void reset() { x1 = x2 = 0; }
    };

    struct Biquad   // fixed EQ stage (RBJ cookbook)
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void peaking (double f, double q, double gainDb, double sr);
        void lowpass (double f, double q, double sr);
        double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void reset() { z1 = z2 = 0; }
    };

    struct Bandpass   // state-variable band-pass with unity peak gain
    {
        double g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        void set (double f, double q, double sr);
        double process (double x);
        void reset() { ic1 = ic2 = 0; }
    };

    struct Targets
    {
        float f[3] {};
        float bw[3] {};
        float av = 0, ah = 0, af = 0, an = 0;   // voicing, aspiration, frication, nasal murmur
        float fricF = 4000, fricQ = 1;
        float nz = 270;                            // nasal zero frequency (270 = no nasal colouring)
        float pitchRel = 0;
    };

    Targets computeTargets() const;
    void endPhoneFormants (int index, float out[3]) const;
    void advance (double ms);
    float noise();

    const Utterance* utt = nullptr;
    VoiceParams params;
    double sr = 44100.0;

    bool active = false;
    bool keyDown = false;
    int firstPhone = 0, lastPhone = -1, sustainPhone = -1, phoneIndex = 0;
    double phoneTime = 0.0;          // ms into the current phone
    double tailMs = 0.0;             // fade-out after the last phone

    float noteHz = 110.0f, glidedHz = 110.0f, bendSemis = 0.0f, velocity = 1.0f;
    double glottalPhase = 0.0, vibPhase = 0.0;

    std::array<float, 3> curF { 500, 1500, 2500 };
    std::array<float, 3> curB { 70, 90, 150 };
    float curAV = 0, curAH = 0, curAF = 0, curAN = 0, curPitchRel = 0;
    float fricF = 4000, fricQ = 1;
    float level = 0;

    Resonator r1, r2, r3, r4, r5, nasal, nasalPole;
    AntiResonator nasalZero;
    Biquad presence, airCut, airCut2;   // shapes the voice towards the average spectrum of real speech
    float curNZ = 270;
    Bandpass fric;
    double tilt = 0, dcX = 0, dcY = 0, holdValue = 0, holdCount = 0, lastY = 0;
    uint32_t rng = 0x12345678u;
    int tick = 0;
};
} // namespace talk
