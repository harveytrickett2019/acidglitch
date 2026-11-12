// Ties the sequencer, voice and glitch section together. No JUCE dependency.
#pragma once
#include "AcidVoice.h"
#include "Glitch.h"
#include "Pattern.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

namespace acid
{
struct EngineParams
{
    AcidVoice::Params voice;
    float tune = 0.0f;          // semitones
    float volume = 0.7f;
    bool  seqOn = true;
    int   rate = 1;             // 0 = 1/8, 1 = 1/16, 2 = 1/16T, 3 = 1/32
    float swing = 0.0f;         // 0..0.6
    int   length = 16;          // steps
    int   root = 9;             // 0 = C
    float glitchChance = 0.1f;  // random glitch probability per step
    int   glitchType = 0;       // 0 = random, 1 = stutter, 2 = reverse, 3 = tape stop
    float glitchMix = 1.0f;
    float crush = 0.0f;
    float downsample = 0.0f;

    // Optional sample that replaces the oscillator (owned by the caller).
    const float* sample = nullptr;
    int    sampleLen = 0;
    double sampleRate = 44100.0;
};

struct Transport
{
    bool   playing = false;
    double bpm = 120.0;
    double ppq = 0.0;           // position at the first sample of the block
};

class Engine
{
public:
    Pattern pattern;
    std::atomic<int> uiStep { -1 };   // playhead for the editor

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        voice.prepare (sampleRate);
        glitch.prepare (sampleRate);
        numHeld = 0; lastK = kNone; wasRunning = false; internalRun = false;
        gateOpen = false; gateHold = false; volSm = 0.0f;
        uiStep.store (-1);
    }

    void noteOn (int note, int velocity, const EngineParams& p)
    {
        const bool legato = numHeld > 0;
        removeHeld (note);
        if (numHeld < kMaxHeld) { held[numHeld] = note; heldVel[numHeld] = velocity; ++numHeld; }
        if (! p.seqOn)
            voice.noteOn ((float) note + p.tune, velocity >= 100, legato);
    }

    void noteOff (int note, const EngineParams& p)
    {
        const bool wasTop = numHeld > 0 && held[numHeld - 1] == note;
        removeHeld (note);
        if (p.seqOn) return;
        if (numHeld == 0)      voice.noteOff();
        else if (wasTop)       voice.noteOn ((float) held[numHeld - 1] + p.tune, heldVel[numHeld - 1] >= 100, true);
    }

    void allNotesOff() { numHeld = 0; voice.noteOff(); }

    // Renders n mono samples into out.
    void process (float* out, int n, const Transport& t, const EngineParams& p)
    {
        AcidVoice::Params vp = p.voice;
        vp.sample      = p.sample;
        vp.sampleLen   = p.sampleLen;
        vp.sampleRatio = (float) (p.sampleRate / sr);
        vp.refNote     = p.seqOn ? 36.0f + (float) p.root : 60.0f;   // root step / C5 = original pitch
        voice.setParams (vp);

        static const double rateLen[] = { 0.5, 0.25, 1.0 / 6.0, 0.125 };
        const double L = rateLen[p.rate < 0 ? 0 : (p.rate > 3 ? 3 : p.rate)];
        const double bpm = t.bpm > 20.0 ? t.bpm : 120.0;
        const double inc = bpm / 60.0 / sr;                 // quarter notes per sample
        const double sw = (double) p.swing * 0.5;

        // Host playing -> follow the host. Host stopped -> a held key runs the
        // pattern on an internal clock so sounds can be auditioned.
        const bool hostRun = p.seqOn && t.playing;
        const bool keyRun  = p.seqOn && ! t.playing && numHeld > 0;
        const bool running = hostRun || keyRun;

        double ppq = t.ppq;
        if (keyRun)
        {
            if (! internalRun) { internalPpq = 0.0; lastK = kNone; }
            ppq = internalPpq;
        }
        else if (internalRun)
        {
            lastK = kNone;
        }
        internalRun = keyRun;

        if (wasRunning && ! running)
        {
            voice.noteOff(); glitch.stop();
            gateOpen = false; lastK = kNone; uiStep.store (-1);
        }
        if (p.seqOn != lastSeqOn)
        {
            voice.noteOff(); glitch.stop(); gateOpen = false; lastK = kNone;
            lastSeqOn = p.seqOn;
        }
        wasRunning = running;

        const float volTarget = p.volume * p.volume * 1.4f;
        const float volCoef = 1.0f - std::exp (-1.0f / (0.01f * (float) sr));

        for (int i = 0; i < n; ++i)
        {
            if (running)
            {
                const double pos = ppq / L;
                const double kf = std::floor (pos);
                int64_t k = (int64_t) kf;
                const double frac = pos - kf;
                double ef;
                if (k & 1)
                {
                    if (frac < sw) { k -= 1; ef = (1.0 + frac) / (1.0 + sw); }
                    else           { ef = (frac - sw) / (1.0 - sw); }
                }
                else
                {
                    ef = frac / (1.0 + sw);
                }

                // ignore tiny backwards jitter in the host position
                const bool jitter = (lastK != kNone && k == lastK - 1 && ef > 0.9);
                if (k != lastK && ! jitter)
                {
                    const double stepSamples = (L / inc) * ((k & 1) ? 1.0 - sw : 1.0 + sw);
                    onStep (k, (int) stepSamples, p);
                }

                if (gateOpen && ! gateHold && ef >= 0.55)
                {
                    voice.noteOff();
                    gateOpen = false;
                }
                ppq += inc;
            }

            float s = voice.tick();
            s = glitch.tick (s, p.glitchMix, p.crush, p.downsample);
            volSm += (volTarget - volSm) * volCoef;
            out[i] = s * volSm;
        }

        if (keyRun) internalPpq = ppq;
    }

private:
    static constexpr int64_t kNone = std::numeric_limits<int64_t>::min();
    static constexpr int kMaxHeld = 16;

    void removeHeld (int note)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[i] == note)
            {
                for (int j = i; j < numHeld - 1; ++j) { held[j] = held[j + 1]; heldVel[j] = heldVel[j + 1]; }
                --numHeld;
                return;
            }
    }

    void onStep (int64_t k, int stepSamples, const EngineParams& p)
    {
        const int len = p.length < 1 ? 1 : (p.length > kMaxSteps ? kMaxSteps : p.length);
        const int idx = (int) (((k % len) + len) % len);
        const Step s    = pattern.get (idx);
        const Step prev = pattern.get ((idx + len - 1) % len);
        const Step next = pattern.get ((idx + 1) % len);
        const bool contiguous = (lastK != kNone && k == lastK + 1);
        lastK = k;
        uiStep.store (idx, std::memory_order_relaxed);

        if (s.gate)
        {
            const int transpose = numHeld > 0 ? held[numHeld - 1] - 60 : 0;
            const float note = 36.0f + (float) (p.root + s.semi + transpose) + p.tune;
            const bool tie = contiguous && prev.gate && prev.slide && voice.gateOn();
            voice.noteOn (note, s.accent, tie);
            gateOpen = true;
            gateHold = s.slide && next.gate;
        }
        else
        {
            voice.noteOff();
            gateOpen = false;
        }

        // glitch: flagged steps always fire, others fire by chance. The hash
        // makes it repeat identically each time the same bar plays.
        const uint32_t h = hash32 ((uint32_t) (uint64_t) k * 2654435761u + 0x51ed270bu);
        const bool fire = s.glitch || ((float) (h & 0xffffu) / 65536.0f) < p.glitchChance;
        if (! fire) { glitch.stop(); return; }

        static const int divs[] = { 2, 3, 4, 4, 6, 8 };
        static const float rates[] = { 1.0f, 1.0f, 1.0f, 0.943874f, 1.059463f, 0.890899f };
        const int div = divs[(h >> 16) % 6u];
        const float rate = rates[(h >> 20) % 6u];

        int type = p.glitchType;
        float burst = 0.0f;
        if (type == 0)
        {
            const uint32_t pick = (h >> 24) % 8u;   // weighted random
            if (pick < 4)       type = 1;
            else if (pick < 6)  type = 2;
            else if (pick < 7)  type = 3;
            else              { type = 1; burst = 0.4f; }   // crushed stutter
        }

        switch (type)
        {
            case 2:  glitch.start (Glitch::Reverse,  stepSamples, 1, 1.0f, burst); break;
            case 3:  glitch.start (Glitch::TapeStop, stepSamples, 1, 1.0f, burst); break;
            default: glitch.start (Glitch::Stutter,  stepSamples, div, rate, burst); break;
        }
    }

    AcidVoice voice;
    Glitch glitch;
    double sr = 44100.0, internalPpq = 0.0;
    int64_t lastK = kNone;
    int held[kMaxHeld] {}, heldVel[kMaxHeld] {}, numHeld = 0;
    float volSm = 0.0f;
    bool wasRunning = false, internalRun = false, gateOpen = false, gateHold = false, lastSeqOn = true;
};
} // namespace acid
