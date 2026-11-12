// Glitch effects: stutter / reverse / tape-stop events that last one step,
// plus always-on bitcrush and sample-rate reduction. No JUCE dependency.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace acid
{
class Glitch
{
public:
    enum Type { Stutter = 0, Reverse, TapeStop };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        buf.assign ((size_t) (sampleRate * 4.0) + 16, 0.0f);
        size = (int) buf.size();
        w = 0; active = false; held = 0.0f; holdPhase = 0.0f;
    }

    // lengthSamples = how long the event lasts (one sequencer step)
    void start (Type t, int lengthSamples, int divisions, float repeatRate, float burst)
    {
        type = t;
        len = std::clamp (lengthSamples, 64, size / 2);
        slice = std::max (32, len / std::max (1, divisions));
        fade = std::min (64, slice / 4);
        rateStep = repeatRate;
        extra = burst;
        startW = w;
        t0 = 0;
        tapePos = 0.0;
        active = true;
    }

    void stop() { active = false; }

    float tick (float in, float mix, float crush, float down)
    {
        buf[(size_t) w] = in;
        float out = in;

        if (active)
        {
            float wet = in;
            switch (type)
            {
                case Stutter:
                    if (t0 < slice)    // first slice plays live while it is captured
                    {
                        wet = in * std::min (1.0f, (float) (slice - t0) / (float) fade);
                    }
                    else
                    {
                        const int rep = t0 / slice;
                        const int tt = t0 - rep * slice;
                        const double rate = std::pow ((double) rateStep, (double) rep);
                        const double pos = std::fmod ((double) tt * rate, (double) slice);
                        const float f = std::min (1.0f, std::min ((float) tt, (float) (slice - tt)) / (float) fade);
                        wet = read ((double) startW + pos) * f;
                    }
                    break;

                case Reverse:
                {
                    const float f = std::min (1.0f, std::min ((float) t0, (float) (len - t0)) / 64.0f);
                    wet = read ((double) (startW - 1 - t0)) * std::max (0.0f, f);
                    break;
                }

                case TapeStop:
                {
                    const float prog = (float) t0 / (float) len;
                    const float rate = std::max (0.0f, 1.0f - prog);
                    tapePos += (double) (rate * rate);
                    wet = read ((double) startW + tapePos) * std::min (1.0f, (1.0f - prog) * 8.0f);
                    break;
                }
            }

            // ease back to the dry signal at the end of the event
            const float tail = std::min (1.0f, (float) (len - t0) / 64.0f);
            out = in + (wet - in) * mix * tail;
            if (++t0 >= len) active = false;
        }

        // sample-rate reduction
        const float d = std::max (down, active ? extra : 0.0f);
        if (d > 0.001f)
        {
            const float period = std::exp (d * std::log (sr / 500.0f));
            holdPhase += 1.0f;
            if (holdPhase >= period) { holdPhase -= period; held = out; }
            out = held;
        }
        else
        {
            held = out; holdPhase = 0.0f;
        }

        // bit reduction
        const float c = std::max (crush, active ? extra : 0.0f);
        if (c > 0.001f)
        {
            const float q = std::exp2 (15.0f - c * 13.0f);
            out = std::round (out * q) / q;
        }

        if (++w >= size) w = 0;
        return out;
    }

private:
    float read (double pos) const
    {
        double p = std::fmod (pos, (double) size);
        if (p < 0.0) p += (double) size;
        int i0 = (int) p;
        if (i0 >= size) i0 = 0;
        const int i1 = (i0 + 1 >= size) ? 0 : i0 + 1;
        const float fr = (float) (p - (double) i0);
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    }

    std::vector<float> buf;
    float sr = 44100.0f, rateStep = 1.0f, extra = 0.0f, held = 0.0f, holdPhase = 0.0f;
    double tapePos = 0.0;
    int size = 0, w = 0, startW = 0, t0 = 0, len = 0, slice = 0, fade = 64;
    Type type = Stutter;
    bool active = false;
};
} // namespace acid
