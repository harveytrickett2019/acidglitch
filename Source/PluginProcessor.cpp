#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using Range = juce::NormalisableRange<float>;

static Range skewed (float lo, float hi, float step, float centre)
{
    Range r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout AcidGlitchProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };

    // synth
    l.add (std::make_unique<APF> (id ("tune"),    "Tune",      Range (-12.0f, 12.0f, 0.01f), 0.0f));
    l.add (std::make_unique<APF> (id ("wave"),    "Wave",      Range (0.0f, 1.0f, 0.01f), 0.0f));
    l.add (std::make_unique<APF> (id ("cutoff"),  "Cutoff",    skewed (40.0f, 8000.0f, 1.0f, 600.0f), 420.0f));
    l.add (std::make_unique<APF> (id ("reso"),    "Resonance", Range (0.0f, 1.0f, 0.01f), 0.78f));
    l.add (std::make_unique<APF> (id ("envmod"),  "Env Mod",   Range (0.0f, 1.0f, 0.01f), 0.55f));
    l.add (std::make_unique<APF> (id ("decay"),   "Decay",     skewed (0.05f, 2.5f, 0.01f, 0.5f), 0.35f));
    l.add (std::make_unique<APF> (id ("accent"),  "Accent",    Range (0.0f, 1.0f, 0.01f), 0.6f));
    l.add (std::make_unique<APF> (id ("slide"),   "Slide",     Range (10.0f, 300.0f, 1.0f), 60.0f));
    l.add (std::make_unique<APF> (id ("drive"),   "Drive",     Range (0.0f, 1.0f, 0.01f), 0.35f));
    l.add (std::make_unique<APF> (id ("volume"),  "Volume",    Range (0.0f, 1.0f, 0.01f), 0.7f));

    // sequencer
    l.add (std::make_unique<juce::AudioParameterBool>   (id ("seq"),    "Sequencer", true));
    l.add (std::make_unique<juce::AudioParameterChoice> (id ("rate"),   "Rate",
                                                         juce::StringArray { "1/8", "1/16", "1/16T", "1/32" }, 1));
    l.add (std::make_unique<APF> (id ("swing"),   "Swing",     Range (0.0f, 0.6f, 0.01f), 0.0f));
    l.add (std::make_unique<juce::AudioParameterInt>    (id ("length"), "Length", 1, 16, 16));
    l.add (std::make_unique<juce::AudioParameterChoice> (id ("root"),   "Root",
                                                         juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9));
    juce::StringArray scales;
    for (int i = 0; i < acid::kNumScales; ++i) scales.add (acid::scaleAt (i).name);
    l.add (std::make_unique<juce::AudioParameterChoice> (id ("scale"),  "Scale", scales, 1));
    l.add (std::make_unique<APF> (id ("density"), "Density",   Range (0.0f, 1.0f, 0.01f), 0.8f));

    // glitch
    l.add (std::make_unique<APF> (id ("gchance"), "Glitch Chance", Range (0.0f, 1.0f, 0.01f), 0.08f));
    l.add (std::make_unique<juce::AudioParameterChoice> (id ("gtype"),  "Glitch Type",
                                                         juce::StringArray { "Random", "Stutter", "Reverse", "Tape Stop" }, 0));
    l.add (std::make_unique<APF> (id ("gmix"),    "Glitch Mix",    Range (0.0f, 1.0f, 0.01f), 1.0f));
    l.add (std::make_unique<APF> (id ("crush"),   "Crush",         Range (0.0f, 1.0f, 0.01f), 0.0f));
    l.add (std::make_unique<APF> (id ("down"),    "Downsample",    Range (0.0f, 1.0f, 0.01f), 0.0f));
    return l;
}

AcidGlitchProcessor::AcidGlitchProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto get = [this] (const char* s) { return apvts.getRawParameterValue (s); };
    pTune = get ("tune");       pWave = get ("wave");     pCutoff = get ("cutoff"); pReso = get ("reso");
    pEnvMod = get ("envmod");   pDecay = get ("decay");   pAccent = get ("accent"); pSlide = get ("slide");
    pDrive = get ("drive");     pVolume = get ("volume");
    pSeq = get ("seq");         pRate = get ("rate");     pSwing = get ("swing");   pLength = get ("length");
    pRoot = get ("root");       pScale = get ("scale");   pDensity = get ("density");
    pGChance = get ("gchance"); pGType = get ("gtype");   pGMix = get ("gmix");
    pCrush = get ("crush");     pDown = get ("down");

    acid::generatePattern (engine.pattern, 303u, 1, 0.8f);   // default pattern
}

acid::EngineParams AcidGlitchProcessor::readParams() const
{
    acid::EngineParams p;
    p.voice.wave     = pWave->load();
    p.voice.cutoffHz = pCutoff->load();
    p.voice.reso     = pReso->load();
    p.voice.envMod   = pEnvMod->load();
    p.voice.decayS   = pDecay->load();
    p.voice.accent   = pAccent->load();
    p.voice.slideS   = pSlide->load() * 0.001f;
    p.voice.drive    = pDrive->load();
    p.tune           = pTune->load();
    p.volume         = pVolume->load();
    p.seqOn          = pSeq->load() > 0.5f;
    p.rate           = (int) pRate->load();
    p.swing          = pSwing->load();
    p.length         = (int) pLength->load();
    p.root           = (int) pRoot->load();
    p.glitchChance   = pGChance->load();
    p.glitchType     = (int) pGType->load();
    p.glitchMix      = pGMix->load();
    p.crush          = pCrush->load();
    p.downsample     = pDown->load();
    return p;
}

void AcidGlitchProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    mono.assign ((size_t) juce::jmax (samplesPerBlock, 8192), 0.0f);
}

bool AcidGlitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AcidGlitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;
    if ((int) mono.size() < numSamples) mono.resize ((size_t) numSamples);   // host ignored the block size hint

    const acid::EngineParams p = readParams();

    acid::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm())         t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
        }
    const double ppqPerSample = t.bpm / 60.0 / getSampleRate();
    const double blockPpq = t.ppq;

    // Render in chunks split at MIDI events so notes land sample-accurately.
    int done = 0;
    auto renderTo = [&] (int end)
    {
        if (end <= done) return;
        t.ppq = blockPpq + (double) done * ppqPerSample;
        engine.process (mono.data() + done, end - done, t, p);
        done = end;
    };

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        renderTo (juce::jlimit (0, numSamples, meta.samplePosition));
        if (msg.isNoteOn())                                   engine.noteOn (msg.getNoteNumber(), msg.getVelocity(), p);
        else if (msg.isNoteOff())                             engine.noteOff (msg.getNoteNumber(), p);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())  engine.allNotesOff();
    }
    renderTo (numSamples);
    midi.clear();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono.data(), numSamples);
}

void AcidGlitchProcessor::randomisePattern()
{
    acid::generatePattern (engine.pattern, (uint32_t) random.nextInt(), (int) pScale->load(), pDensity->load());
}

void AcidGlitchProcessor::mutatePattern()
{
    acid::mutatePattern (engine.pattern, (uint32_t) random.nextInt(), (int) pScale->load(), pDensity->load());
}

void AcidGlitchProcessor::clearPattern()
{
    for (int i = 0; i < acid::kMaxSteps; ++i)
    {
        acid::Step s;
        s.gate = false;
        engine.pattern.set (i, s);
    }
}

void AcidGlitchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", juce::String (engine.pattern.toString()), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AcidGlitchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid() || ! state.hasType (apvts.state.getType())) return;
        engine.pattern.fromString (state.getProperty ("pattern").toString().toStdString());
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* AcidGlitchProcessor::createEditor() { return new AcidGlitchEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AcidGlitchProcessor(); }
