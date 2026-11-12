// Step pattern storage + generator. No JUCE dependency so it can be tested offline.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

namespace acid
{
constexpr int kMaxSteps = 16;
constexpr int kMinSemi = -12;
constexpr int kMaxSemi = 24;

struct Step
{
    int  semi   = 0;      // semitones above the root note
    bool gate   = true;   // false = rest
    bool accent = false;
    bool slide  = false;  // glide + tie into the next step
    bool glitch = false;  // force a glitch on this step
};

inline uint32_t packStep (const Step& s)
{
    int semi = s.semi < kMinSemi ? kMinSemi : (s.semi > kMaxSemi ? kMaxSemi : s.semi);
    return (uint32_t) (semi - kMinSemi)
         | (s.gate   ? 1u << 6 : 0u)
         | (s.accent ? 1u << 7 : 0u)
         | (s.slide  ? 1u << 8 : 0u)
         | (s.glitch ? 1u << 9 : 0u);
}

inline Step unpackStep (uint32_t v)
{
    Step s;
    s.semi   = (int) (v & 63u) + kMinSemi;
    if (s.semi > kMaxSemi) s.semi = kMaxSemi;
    s.gate   = (v & (1u << 6)) != 0;
    s.accent = (v & (1u << 7)) != 0;
    s.slide  = (v & (1u << 8)) != 0;
    s.glitch = (v & (1u << 9)) != 0;
    return s;
}

struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed ? seed : 0x9E3779B9u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni()     { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    int range (int n) { return (int) (next() % (uint32_t) n); }
};

inline uint32_t hash32 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

struct Scale { const char* name; int n; int notes[12]; };

inline const Scale& scaleAt (int i)
{
    static const Scale scales[] = {
        { "Minor",       7,  { 0, 2, 3, 5, 7, 8, 10 } },
        { "Phrygian",    7,  { 0, 1, 3, 5, 7, 8, 10 } },
        { "Minor Pent",  5,  { 0, 3, 5, 7, 10 } },
        { "Acid",        5,  { 0, 1, 3, 7, 10 } },
        { "Chromatic",   12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    if (i < 0) i = 0;
    if (i > 4) i = 4;
    return scales[i];
}
constexpr int kNumScales = 5;

// Lock-free pattern: UI thread writes, audio thread reads.
class Pattern
{
public:
    Pattern() { for (auto& s : steps) s.store (packStep (Step {})); }

    Step get (int i) const          { return unpackStep (steps[(size_t) (i & 15)].load (std::memory_order_relaxed)); }
    void set (int i, const Step& s) { steps[(size_t) (i & 15)].store (packStep (s), std::memory_order_relaxed); }

    std::string toString() const
    {
        std::string out;
        char buf[8];
        for (auto& s : steps)
        {
            std::snprintf (buf, sizeof (buf), "%03x", (unsigned) s.load());
            out += buf;
        }
        return out;
    }

    bool fromString (const std::string& str)
    {
        if (str.size() != (size_t) kMaxSteps * 3) return false;
        for (int i = 0; i < kMaxSteps; ++i)
        {
            unsigned v = 0;
            if (std::sscanf (str.substr ((size_t) i * 3, 3).c_str(), "%x", &v) != 1) return false;
            steps[(size_t) i].store ((uint32_t) v & 1023u);
        }
        return true;
    }

private:
    std::array<std::atomic<uint32_t>, kMaxSteps> steps;
};

inline Step randomStep (Rng& r, const Scale& sc, float density, bool forceGate)
{
    Step s;
    s.gate = forceGate || r.uni() < density;
    int semi = (r.uni() < 0.42f) ? 0 : sc.notes[r.range (sc.n)];
    const float o = r.uni();
    if (o < 0.18f)      semi += 12;
    else if (o < 0.27f) semi -= 12;
    s.semi   = semi;
    s.accent = r.uni() < 0.30f;
    s.slide  = s.gate && r.uni() < 0.22f;
    s.glitch = false;
    return s;
}

// Builds an 8-step phrase, repeats it, then varies the second half: acid lines
// work because they repeat, so pure 16-step noise sounds worse.
inline void generatePattern (Pattern& p, uint32_t seed, int scaleIdx, float density)
{
    Rng r (hash32 (seed) | 1u);
    const Scale& sc = scaleAt (scaleIdx);

    Step half[8];
    for (int i = 0; i < 8; ++i)
        half[i] = randomStep (r, sc, density, i == 0);

    Step full[kMaxSteps];
    for (int i = 0; i < kMaxSteps; ++i)
    {
        full[i] = half[i % 8];
        if (i >= 9 && r.uni() < 0.35f)
            full[i] = randomStep (r, sc, density, false);
    }

    full[12 + r.range (4)].glitch = true;          // a fill near the end of the bar
    if (r.uni() < 0.4f) full[5 + r.range (3)].glitch = true;

    for (int i = 0; i < kMaxSteps; ++i) p.set (i, full[i]);
}

// Re-rolls a few steps, keeping the rest of the line intact.
inline void mutatePattern (Pattern& p, uint32_t seed, int scaleIdx, float density)
{
    Rng r (hash32 (seed) | 1u);
    const Scale& sc = scaleAt (scaleIdx);
    const int changes = 2 + r.range (2);
    for (int c = 0; c < changes; ++c)
    {
        const int i = r.range (kMaxSteps);
        Step s = randomStep (r, sc, density, i == 0);
        s.glitch = p.get (i).glitch;
        p.set (i, s);
    }
}
} // namespace acid
