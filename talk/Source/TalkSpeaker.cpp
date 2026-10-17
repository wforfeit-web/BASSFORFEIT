#include "TalkEngine.h"

#include <algorithm>
#include <cmath>

namespace talk
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kControlSamples = 32;   // targets update every 32 samples (under 1 ms)

inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float smooth01 (float t) { t = std::clamp (t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

inline double polyBlep (double t, double dt)
{
    if (dt <= 0.0) return 0.0;
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

bool isVowelKind (Kind k) { return k == Kind::Vowel || k == Kind::Diphthong; }

const int kAH = 2;   // index of AH in the phoneme table
} // namespace

//==============================================================================
void Speaker::Resonator::set (double f, double bw, double rate)
{
    f = std::clamp (f, 20.0, rate * 0.45);
    c = -std::exp (-2.0 * kPi * bw / rate);
    b = 2.0 * std::exp (-kPi * bw / rate) * std::cos (2.0 * kPi * f / rate);
    a = 1.0 - b - c;
}

void Speaker::Biquad::peaking (double f, double q, double gainDb, double rate)
{
    f = std::min (f, rate * 0.45);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * f / rate, alpha = std::sin (w) / (2.0 * q), cw = std::cos (w);
    const double a0 = 1.0 + alpha / A;
    b0 = (1.0 + alpha * A) / a0; b1 = -2.0 * cw / a0; b2 = (1.0 - alpha * A) / a0;
    a1 = -2.0 * cw / a0; a2 = (1.0 - alpha / A) / a0;
}

void Speaker::Biquad::lowpass (double f, double q, double rate)
{
    f = std::min (f, rate * 0.45);
    const double w = 2.0 * kPi * f / rate, alpha = std::sin (w) / (2.0 * q), cw = std::cos (w);
    const double a0 = 1.0 + alpha;
    b0 = (1.0 - cw) * 0.5 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
    a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
}

void Speaker::AntiResonator::set (double f, double bw, double rate)
{
    Resonator r;
    r.set (f, bw, rate);
    a = 1.0 / r.a;
    b = -r.b / r.a;
    c = -r.c / r.a;
}

void Speaker::Bandpass::set (double f, double q, double rate)
{
    f = std::clamp (f, 20.0, rate * 0.45);
    g = std::tan (kPi * f / rate);
    k = 1.0 / std::max (0.1, q);
    a1 = 1.0 / (1.0 + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

double Speaker::Bandpass::process (double v0)
{
    const double v3 = v0 - ic2;
    const double v1 = a1 * ic1 + a2 * v3;
    const double v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0 * v1 - ic1;
    ic2 = 2.0 * v2 - ic2;
    return v1 * k;
}

//==============================================================================
void Speaker::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto* r : { &r1, &r2, &r3, &r4, &r5, &nasal, &nasalPole }) r->reset();
    nasalZero.reset();
    presence.peaking (3800.0, 0.9, tune ("PRES", 14.0f), sr);
    airCut.lowpass (tune ("LPF", 5200.0f), 0.7, sr);
    airCut2.lowpass (tune ("LPF", 5200.0f), 0.7, sr);
    presence.reset();
    airCut.reset();
    airCut2.reset();
    fric.reset();
    active = false;
    tick = 0;
    tilt = dcX = dcY = holdValue = holdCount = 0.0;
    curAV = curAH = curAF = curAN = 0.0f;
    level = 0.0f;
}

void Speaker::setUtterance (const Utterance* u)
{
    utt = u;
    active = false;
}

void Speaker::trigger (int first, int last, int sustain, float hz, float vel)
{
    if (utt == nullptr || utt->phones.empty()) return;
    first = std::clamp (first, 0, (int) utt->phones.size() - 1);
    last  = std::clamp (last, first, (int) utt->phones.size() - 1);

    const bool wasSounding = active && curAV + curAH + curAN > 0.05f;
    firstPhone = first;
    lastPhone = last;
    sustainPhone = sustain;
    phoneIndex = first;
    phoneTime = 0.0;
    tailMs = 0.0;
    active = true;
    keyDown = true;
    noteHz = hz > 0.0f ? hz : params.basePitch;
    if (! wasSounding) glidedHz = noteHz;   // glide only between connected notes
    velocity = std::clamp (vel, 0.05f, 1.0f);
}

void Speaker::release() { keyDown = false; }

void Speaker::stop()
{
    if (! active) return;
    phoneIndex = lastPhone + 1;
    tailMs = 0.0;
    keyDown = false;
}

float Speaker::noise()
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (float) ((double) rng / 4294967295.0 * 2.0 - 1.0);
}

//==============================================================================
void Speaker::endPhoneFormants (int index, float out[3]) const
{
    const auto& ph = utt->phones[(size_t) index];
    const auto& d = phoneme (ph.id);

    if (d.kind == Kind::Aspirate || d.kind == Kind::Pause)
    {
        // Takes the shape of the next sound
        if (index + 1 < (int) utt->phones.size() && phoneme (utt->phones[(size_t) index + 1].id).kind != Kind::Pause)
        {
            const auto& n = phoneme (utt->phones[(size_t) index + 1].id);
            out[0] = n.f1; out[1] = n.f2; out[2] = n.f3;
            return;
        }
        out[0] = 500; out[1] = 1500; out[2] = 2500;
        return;
    }
    if (ph.id == kAH && ph.stress == 0) { out[0] = 500; out[1] = 1450; out[2] = 2450; return; }   // schwa
    if (d.kind == Kind::Diphthong) { out[0] = d.e1; out[1] = d.e2; out[2] = d.e3; return; }
    out[0] = d.f1; out[1] = d.f2; out[2] = d.f3;
}

Speaker::Targets Speaker::computeTargets() const
{
    Targets t;
    for (int k = 0; k < 3; ++k) { t.f[k] = curF[(size_t) k]; t.bw[k] = curB[(size_t) k]; }
    t.pitchRel = curPitchRel;
    t.fricF = fricF;
    t.fricQ = fricQ;

    if (! active || utt == nullptr || phoneIndex > lastPhone) return t;   // silence; keep the last shape

    const auto& ph = utt->phones[(size_t) phoneIndex];
    const auto& d = phoneme (ph.id);
    const float dur = std::max (1.0f, ph.dur);
    const float time = (float) phoneTime;
    const bool hasPrev = phoneIndex > firstPhone && phoneme (utt->phones[(size_t) phoneIndex - 1].id).kind != Kind::Pause;
    const bool hasNext = phoneIndex < lastPhone && phoneme (utt->phones[(size_t) phoneIndex + 1].id).kind != Kind::Pause;

    if (d.kind == Kind::Pause) return t;

    // ---- Formant targets ----
    float start[3], end[3];
    if (d.kind == Kind::Aspirate)
    {
        endPhoneFormants (phoneIndex, start);
        for (int k = 0; k < 3; ++k) end[k] = start[k];
    }
    else if (ph.id == kAH && ph.stress == 0)
    {
        start[0] = end[0] = 500; start[1] = end[1] = 1450; start[2] = end[2] = 2450;
    }
    else
    {
        start[0] = d.f1; start[1] = d.f2; start[2] = d.f3;
        endPhoneFormants (phoneIndex, end);
    }

    float f[3];
    const float glide = d.kind == Kind::Diphthong ? smooth01 ((time / dur - 0.2f) / 0.7f) : 0.0f;
    for (int k = 0; k < 3; ++k) f[k] = lerp (start[k], end[k], glide);

    // Move in from the previous sound...
    if (hasPrev)
    {
        float prev[3];
        endPhoneFormants (phoneIndex - 1, prev);
        const auto prevKind = phoneme (utt->phones[(size_t) phoneIndex - 1].id).kind;
        float transMs = 40.0f;
        if (prevKind == Kind::Stop || prevKind == Kind::Affricate) transMs = 55.0f;
        if (prevKind == Kind::Glide || prevKind == Kind::Liquid || prevKind == Kind::Nasal) transMs = 65.0f;
        const float trans = std::min (transMs, 0.6f * dur);
        if (time < trans)
        {
            const float s = smooth01 (time / trans);
            for (int k = 0; k < 3; ++k) f[k] = lerp (prev[k], f[k], s);
        }
    }
    // ...and lean towards the next consonant near the end of a vowel
    if (hasNext && isVowelKind (d.kind))
    {
        const auto& n = phoneme (utt->phones[(size_t) phoneIndex + 1].id);
        if (! isVowelKind (n.kind) && n.kind != Kind::Aspirate)
        {
            const float out = std::min (35.0f, 0.35f * dur);
            if (time > dur - out)
            {
                const float s = 0.6f * smooth01 ((time - (dur - out)) / out);
                const float nf[3] = { n.f1, n.f2, n.f3 };
                for (int k = 0; k < 3; ++k) f[k] = lerp (f[k], nf[k], s);
            }
        }
    }

    for (int k = 0; k < 3; ++k) t.f[k] = f[k];
    t.bw[0] = d.b1; t.bw[1] = d.b2; t.bw[2] = d.b3;

    // ---- Sources ----
    const bool stressedVowel = ph.stress > 0;
    switch (d.kind)
    {
        case Kind::Vowel:
        case Kind::Diphthong:
            t.av = stressedVowel ? 1.0f : 0.8f;
            break;
        case Kind::Liquid:
        case Kind::Glide:
            t.av = 0.85f;
            break;
        case Kind::Nasal:
            t.av = tune ("NAV", 0.35f);
            t.an = tune ("NAN", 1.4f);
            t.nz = tune ("NZ", 0.0f) > 0.5f ? d.fricF : 270.0f;   // optional nasal zero (fricF column); off sounds clearer
            t.f[0] = 250.0f;
            break;
        case Kind::Aspirate:
            t.ah = tune ("HH", 0.7f);
            break;
        case Kind::Fricative:
            t.af = d.af;
            t.av = d.voiced ? 0.6f : 0.0f;
            t.fricF = d.fricF;
            t.fricQ = d.fricQ;
            break;
        case Kind::Stop:
        {
            t.fricF = d.fricF;
            t.fricQ = d.fricQ;
            const float burst = d.voiced ? 10.0f : 14.0f;
            const float minAfter = burst + (d.voiced ? 8.0f : 22.0f);
            const float closure = std::max (10.0f, std::min (d.closure * dur / d.dur, dur - minAfter));
            if (time < closure)
            {
                t.av = d.voiced ? 0.12f : 0.0f;   // voice bar
                t.f[0] = 150.0f;
            }
            else if (time < closure + burst)
            {
                t.af = d.af * tune ("BURST", 1.0f);
                t.av = d.voiced ? 0.25f : 0.0f;
            }
            else
            {
                if (d.voiced) t.av = 0.6f;
                else t.ah = tune ("ASP", 0.5f);   // aspiration, shaped like the next vowel
                if (hasNext)
                {
                    float nf[3];
                    const auto& n = phoneme (utt->phones[(size_t) phoneIndex + 1].id);
                    nf[0] = n.f1 > 0 ? n.f1 : 500; nf[1] = n.f2 > 0 ? n.f2 : 1500; nf[2] = n.f3 > 0 ? n.f3 : 2500;
                    for (int k = 0; k < 3; ++k) t.f[k] = lerp (t.f[k], nf[k], 0.6f);
                }
            }
            break;
        }
        case Kind::Affricate:
        {
            t.fricF = d.fricF;
            t.fricQ = d.fricQ;
            const float closure = std::max (10.0f, std::min (d.closure * dur / d.dur, dur - 30.0f));
            if (time < closure) { t.av = d.voiced ? 0.12f : 0.0f; t.f[0] = 150.0f; }
            else { t.af = d.af; t.av = d.voiced ? 0.35f : 0.0f; }
            break;
        }
        case Kind::Pause:
            break;
    }

    // Soft start for the first sound of a phrase
    if (phoneIndex == firstPhone && time < 8.0f)
    {
        const float s = time / 8.0f;
        t.av *= s; t.an *= s;
    }

    t.pitchRel = lerp (ph.pitchStart, ph.pitchEnd, std::clamp (time / dur, 0.0f, 1.0f));
    return t;
}

void Speaker::advance (double ms)
{
    if (! active || utt == nullptr) return;

    if (phoneIndex > lastPhone)
    {
        tailMs += ms;
        if (tailMs > 80.0) active = false;
        return;
    }

    // Hold the chosen vowel while the key is down
    if (params.sustain && keyDown && phoneIndex == sustainPhone)
    {
        const double holdAt = utt->phones[(size_t) phoneIndex].dur * 0.6;
        if (phoneTime >= holdAt) return;
        phoneTime = std::min (holdAt, phoneTime + ms * params.speed);
        return;
    }

    phoneTime += ms * params.speed;
    while (phoneIndex <= lastPhone && phoneTime >= utt->phones[(size_t) phoneIndex].dur)
    {
        phoneTime -= utt->phones[(size_t) phoneIndex].dur;
        ++phoneIndex;
        if (params.sustain && keyDown && phoneIndex == sustainPhone) phoneTime = std::min (phoneTime, 1.0);
    }
}

//==============================================================================
void Speaker::render (float* out, int numSamples)
{
    const double ctrlMs = 1000.0 * kControlSamples / sr;
    const double ampCoef = 1.0 - std::exp (-ctrlMs / 3.0);
    const double frmCoef = 1.0 - std::exp (-ctrlMs / 2.5);
    const double glideCoef = params.glideMs > 1.0f ? 1.0 - std::exp (-1000.0 / (sr * params.glideMs)) : 1.0;
    const float fs = params.formant;
    const int effort = params.effort;
    const double openQ = effort == 1 ? 0.75 : 0.6;
    const double tiltCoef = effort == 1 ? 0.35 : 1.0;
    const float vibDepth = (params.intonation == 2 ? std::max (params.vibrato, 0.35f) : params.vibrato) * 0.03f;
    const double lofiRate = sr + (5500.0 - sr) * std::pow ((double) params.lofi, 0.6);
    const double bits = 16.0 - 11.0 * params.lofi;
    const double quant = std::pow (2.0, bits - 1.0);

    for (int i = 0; i < numSamples; ++i)
    {
        if (--tick < 0)
        {
            tick = kControlSamples - 1;
            Targets t = computeTargets();

            // Voice effort
            if (effort == 0)      { t.ah += t.av * 0.03f; }
            else if (effort == 1) { t.ah += t.av * 0.35f; t.av *= 0.65f; }
            else                  { t.ah += t.av * 1.7f + t.an * 0.8f; t.av = 0.0f; t.an = 0.0f; }   // whisper

            for (int k = 0; k < 3; ++k)
            {
                curF[(size_t) k] += (float) ((t.f[k] - curF[(size_t) k]) * frmCoef);
                curB[(size_t) k] += (float) ((t.bw[k] - curB[(size_t) k]) * frmCoef);
            }
            curAV += (float) ((t.av - curAV) * ampCoef);
            curAH += (float) ((t.ah - curAH) * ampCoef);
            curAF += (float) ((t.af - curAF) * std::min (1.0, ampCoef * 2.0));
            curAN += (float) ((t.an - curAN) * ampCoef);
            curNZ += (float) ((t.nz - curNZ) * frmCoef);
            curPitchRel += (float) ((t.pitchRel - curPitchRel) * (1.0 - std::exp (-ctrlMs / 25.0)));
            fricF = t.fricF;
            fricQ = t.fricQ;

            r1.set (curF[0] * fs, curB[0], sr);
            r2.set (curF[1] * fs, curB[1], sr);
            r3.set (curF[2] * fs, curB[2], sr);
            r4.set (3500.0 * fs, 250.0, sr);
            r5.set (4500.0 * fs, 300.0, sr);
            nasal.set (270.0 * std::sqrt (fs), 100.0, sr);
            nasalPole.set (270.0 * std::sqrt (fs), 100.0, sr);
            nasalZero.set (curNZ * std::sqrt (fs), curNZ > 300.0f ? 150.0 : 100.0, sr);
            fric.set (fricF * std::min (1.25f, fs), fricQ, sr);

            advance (ctrlMs);
        }

        // ---- Pitch ----
        glidedHz += (float) ((noteHz - glidedHz) * glideCoef);
        double f0 = glidedHz * std::pow (2.0, bendSemis / 12.0);
        if (params.intonation == 0) f0 *= 1.0 + params.inflection * curPitchRel;
        vibPhase += 5.5 / sr;
        if (vibPhase >= 1.0) vibPhase -= 1.0;
        f0 *= 1.0 + vibDepth * std::sin (2.0 * kPi * vibPhase);
        f0 = std::clamp (f0, 25.0, 1500.0);

        // ---- Glottal source: derivative of a smooth flow pulse, band-limited at closure ----
        const double dt = f0 / sr;
        glottalPhase += dt;
        if (glottalPhase >= 1.0) glottalPhase -= 1.0;
        double src = 0.0;
        if (glottalPhase < openQ)
        {
            const double x = glottalPhase / openQ;
            src = 2.0 * x - 3.0 * x * x;          // rises, then drops sharply at closure
        }
        double p = glottalPhase - openQ;
        if (p < 0.0) p += 1.0;
        src += 1.0 * polyBlep (p, dt);           // smooth the jump back to zero
        tilt += (src - tilt) * tiltCoef;

        const float n1 = noise();
        const float n2 = noise();
        const double breathGate = effort == 1 ? (glottalPhase < openQ ? 1.0 : 0.5) : 1.0;
        const double asp = n1 * curAH * 0.35 * breathGate;

        double y = nasalZero.process (tilt * curAV + asp);
        y = nasalPole.process (y);
        y = r1.process (y);
        y = r2.process (y);
        y = r3.process (y);
        y = r4.process (y);
        y = r5.process (y);

        const double nas = nasal.process (tilt * curAN) * 0.9;
        const double fr = fric.process (n2 * curAF) * tune ("FRIC", 0.85f);

        if (tune ("EQ", 1.0f) > 0.5f) y = airCut2.process (airCut.process (presence.process (y)));
        const double emphAmt = tune ("EMPH", 0.0f);
        const double emph = y - emphAmt * lastY;
        lastY = y;
        double x = emph * tune ("VGAIN", 0.7f) + nas + fr;

        // DC blocker
        const double dc = x - dcX + 0.995 * dcY;
        dcX = x;
        dcY = dc;
        x = dc;

        // Lo-fi: lower sample rate and bit depth
        if (params.lofi > 0.001f)
        {
            holdCount += lofiRate / sr;
            if (holdCount >= 1.0)
            {
                holdCount -= 1.0;
                holdValue = std::round (x * quant) / quant;
            }
            x = holdValue;
        }

        x *= params.gain * velocity;
        x = std::tanh (x);
        level = std::max (level * 0.9995f, (float) std::abs (x));
        out[i] = (float) x;
    }
}
} // namespace talk
