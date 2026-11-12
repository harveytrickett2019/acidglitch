// Offline render of the engine to a WAV file, for checking the sound engine
// without a DAW.  g++ -O2 -std=c++17 tests/render.cpp -o render && ./render out.wav
#include "../Source/dsp/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static void writeWav (const char* path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen (path, "wb");
    if (! f) return;
    const uint32_t dataBytes = (uint32_t) x.size() * 2u;
    auto u32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (1); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 2u); u16 (2); u16 (16);
    std::fwrite ("data", 1, 4, f); u32 (dataBytes);
    for (float v : x)
    {
        const float c = v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
        u16 ((uint16_t) (int16_t) (c * 32767.0f));
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "render.wav";
    const uint32_t seed = argc > 2 ? (uint32_t) std::atoi (argv[2]) : 303u;
    const float chance = argc > 3 ? (float) std::atof (argv[3]) : 0.12f;
    const int sr = 48000, block = 256, bars = 8;
    const double bpm = 132.0;

    acid::Engine eng;
    eng.prepare (sr);
    acid::generatePattern (eng.pattern, seed, 1, 0.8f);

    for (int i = 0; i < acid::kMaxSteps; ++i)
    {
        const acid::Step s = eng.pattern.get (i);
        std::printf ("%2d semi=%3d %s%s%s%s\n", i, s.semi, s.gate ? "G" : "-", s.accent ? "A" : "-",
                     s.slide ? "S" : "-", s.glitch ? "X" : "-");
    }

    acid::EngineParams p;
    p.glitchChance = chance;
    p.crush = argc > 4 ? (float) std::atof (argv[4]) : 0.0f;

    // optional 5th argument "sample": play a synthetic plucked-bass sample instead of the synth
    std::vector<float> smp;
    if (argc > 5)
    {
        smp.resize (22050);
        for (size_t i = 0; i < smp.size(); ++i)
        {
            const float tt = (float) i / 44100.0f;
            smp[i] = std::sin (6.2831853f * 65.4f * tt + 2.0f * std::exp (-tt * 12.0f) * std::sin (6.2831853f * 130.8f * tt)) * std::exp (-tt * 5.0f);
        }
        p.sample = smp.data(); p.sampleLen = (int) smp.size(); p.sampleRate = 44100.0;
    }
    acid::Transport t;
    t.playing = true; t.bpm = bpm;

    const int total = (int) (bars * 4 * 60.0 / bpm * sr);
    std::vector<float> out ((size_t) total);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        const float prog = (float) pos / (float) total;
        p.voice.cutoffHz = 250.0f * std::exp2 (prog * 3.5f);     // slow filter sweep
        p.voice.reso = 0.6f + 0.35f * prog;
        t.ppq = (double) pos * bpm / 60.0 / sr;
        eng.process (out.data() + pos, n, t, p);
    }
    writeWav (path, out, sr);
    return 0;
}
