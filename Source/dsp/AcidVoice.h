// 303-style monophonic voice: saw/square oscillator -> resonant 4-pole ladder
// filter with envelope, accent and slide -> drive. No JUCE dependency.
#pragma once
#include <algorithm>
#include <cmath>

namespace acid
{
class AcidVoice
{
public:
    struct Params
    {
        float wave     = 0.0f;    // 0 = saw, 1 = square
        float cutoffHz = 500.0f;
        float reso     = 0.75f;   // 0..1
        float envMod   = 0.55f;   // 0..1
        float decayS   = 0.35f;   // filter envelope decay (seconds)
        float accent   = 0.6f;    // 0..1
        float slideS   = 0.06f;   // glide time (seconds)
        float drive    = 0.3f;    // 0..1

        // Optional sample source. When set, it replaces the oscillator.
        const float* sample = nullptr;
        int   sampleLen   = 0;
        float sampleRatio = 1.0f;    // sample's rate / engine rate
        float refNote     = 60.0f;   // note at which the sample plays at its original pitch
    };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        atkCoef     = 1.0f - std::exp (-1.0f / (0.0015f * sr));
        relCoef     = std::exp (-1.0f / (0.012f * sr));
        susCoef     = std::exp (-1.0f / (2.5f * sr));
        accDecay    = std::exp (-1.0f / (0.07f * sr));
        accAtk      = 1.0f - std::exp (-1.0f / (0.012f * sr));
        accRel      = 1.0f - std::exp (-1.0f / (0.16f * sr));
        gainSmooth  = 1.0f - std::exp (-1.0f / (0.003f * sr));
        cutSmooth   = 1.0f - std::exp (-1.0f / (0.01f * sr));
        setParams (p);
        reset();
    }

    void reset()
    {
        phase = 0.0f; s1 = s2 = s3 = s4 = 0.0f;
        fEnv = 0.0f; amp = 0.0f; accState = 0.0f; accGain = 1.0f;
        gate = false; attack = false; gliding = false; accented = false;
        cutSm = p.cutoffHz; dcX = dcY = 0.0f;
    }

    void setParams (const Params& np)
    {
        p = np;
        glideCoef = 1.0f - std::exp (-1.0f / (std::max (0.002f, p.slideS) * 0.33f * sr));
        decayCoef = std::exp (-1.0f / (std::max (0.02f, p.decayS) * 0.4f * sr));
    }

    // tie = true keeps the envelopes running and glides to the new pitch.
    void noteOn (float midiNote, bool accent, bool tie)
    {
        targetNote = midiNote;
        accented = accent;
        if (tie && gate)
        {
            gliding = true;
        }
        else
        {
            gliding = false;
            curNote = midiNote;
            fEnv = 1.0f;
            attack = true;
            samplePos = 0.0;
        }
        gate = true;
    }

    void noteOff()       { gate = false; }
    bool gateOn() const  { return gate; }

    float tick()
    {
        // pitch
        if (gliding) curNote += (targetNote - curNote) * glideCoef;
        float osc = 0.0f;
        if (p.sample != nullptr)
        {
            // sample playback, pitched relative to refNote (one-shot)
            const int i0 = (int) samplePos;
            if (i0 >= 0 && i0 + 1 < p.sampleLen)
            {
                const float fr = (float) (samplePos - (double) i0);
                osc = p.sample[i0] + (p.sample[i0 + 1] - p.sample[i0]) * fr;
            }
            samplePos += (double) (std::exp2 ((curNote - p.refNote) * (1.0f / 12.0f)) * p.sampleRatio);
        }
        else
        {
            const float freq = 440.0f * std::exp2 ((curNote - 69.0f) * (1.0f / 12.0f));
            const float dt = std::min (0.45f, freq / sr);
            phase += dt;
            if (phase >= 1.0f) phase -= 1.0f;

            // oscillator (band-limited saw / square)
            const float saw = 2.0f * phase - 1.0f - blep (phase, dt);
            float ph2 = phase + 0.5f; if (ph2 >= 1.0f) ph2 -= 1.0f;
            const float sq = (phase < 0.5f ? 1.0f : -1.0f) + blep (phase, dt) - blep (ph2, dt);
            osc = saw + (sq * 0.8f - saw) * p.wave;
        }

        // envelopes
        fEnv *= accented ? accDecay : decayCoef;
        const float accIn = accented ? fEnv : 0.0f;
        accState += (accIn - accState) * (accIn > accState ? accAtk : accRel);

        if (gate)
        {
            if (attack) { amp += (1.03f - amp) * atkCoef; if (amp >= 1.0f) { amp = 1.0f; attack = false; } }
            else        { amp *= susCoef; }
        }
        else
        {
            amp *= relCoef;
        }
        const float accTarget = accented ? 1.0f + p.accent * 0.8f : 1.0f;
        accGain += (accTarget - accGain) * gainSmooth;

        // filter cutoff: knob + envelope + accent sweep
        cutSm += (p.cutoffHz - cutSm) * cutSmooth;
        const float oct = p.envMod * (6.5f * fEnv - 1.0f) + p.accent * 2.0f * accState;
        const float fc = std::clamp (cutSm * std::exp2 (oct), 20.0f, std::min (19000.0f, 0.45f * sr));

        // zero-delay-feedback 4-pole ladder with a saturating input
        const float g = std::tan (3.14159265f * fc / sr);
        const float G = g / (1.0f + g);
        const float k = 3.9f * p.reso;
        const float S = (1.0f - G) * (G * G * G * s1 + G * G * s2 + G * s3 + s4);
        const float x = osc * 0.55f * (1.0f + 0.4f * k);
        float u = (x - k * S) / (1.0f + k * G * G * G * G);
        u = std::tanh (u);

        float v;
        v = (u  - s1) * G; const float y1 = v + s1; s1 = y1 + v;
        v = (y1 - s2) * G; const float y2 = v + s2; s2 = y2 + v;
        v = (y2 - s3) * G; const float y3 = v + s3; s3 = y3 + v;
        v = (y3 - s4) * G; const float y4 = v + s4; s4 = y4 + v;

        // amp + drive
        float out = y4 * amp * accGain;
        const float dg = 1.0f + p.drive * p.drive * 14.0f;
        out = std::tanh (out * dg) / (1.0f + p.drive * 0.9f);

        // DC blocker
        const float y = out - dcX + 0.9995f * dcY;
        dcX = out; dcY = y;
        return y;
    }

private:
    static float blep (float t, float dt)
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    Params p;
    float sr = 44100.0f;
    double samplePos = 0.0;
    float phase = 0.0f, curNote = 36.0f, targetNote = 36.0f;
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    float fEnv = 0, amp = 0, accState = 0, accGain = 1, cutSm = 500, dcX = 0, dcY = 0;
    float atkCoef = 0, relCoef = 0, susCoef = 0, accDecay = 0, accAtk = 0, accRel = 0;
    float gainSmooth = 0, cutSmooth = 0, glideCoef = 0, decayCoef = 0;
    bool gate = false, attack = false, gliding = false, accented = false;
};
} // namespace acid
