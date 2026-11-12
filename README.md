# AcidGlitch

A VST3 instrument for acid basslines and glitch sounds: a 303-style mono synth, a 16-step
pattern generator, and a glitch section (stutter, reverse, tape stop, bitcrush, downsample).

## Install (Windows)

1. Download `AcidGlitch-Windows-VST3.zip` from the [latest release](../../releases/latest).
2. Unzip it and copy the `AcidGlitch.vst3` folder into `C:\Program Files\Common Files\VST3`.
3. In FL Studio: **Options > Manage plugins > Find installed plugins**, then add AcidGlitch
   to a channel.

## Using it

- **SEQ on** (default): press play and the pattern runs in time with the project. With the
  transport stopped, hold a key to audition the pattern. Notes transpose the pattern
  (C5 in FL Studio's piano roll = no transpose).
- **SEQ off**: plays like a normal mono synth. Overlapping notes slide; velocity 100+ is an accent.
- **RANDOMISE** writes a new bassline in the chosen scale; **MUTATE** changes a few steps.
- In the pattern grid, drag in the note row to set pitch and click the rows below to toggle
  gate, accent, slide and glitch per step.
- **GLITCH**: flagged steps always glitch; *Chance* adds random glitches on other steps.

If the plugin goes silent in FL Studio when no notes are playing, turn off **Smart disable**
for it (plugin wrapper settings, the cog icon).

## Building

Every push to `main` builds the Windows VST3 with GitHub Actions. To build locally:

    cmake -B build
    cmake --build build --config Release

Built with [JUCE](https://juce.com) 8 (AGPLv3).
