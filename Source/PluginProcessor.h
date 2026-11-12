#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/Engine.h"

class AcidGlitchProcessor : public juce::AudioProcessor
{
public:
    AcidGlitchProcessor();
    ~AcidGlitchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AcidGlitch"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Pattern actions, called from the editor.
    void randomisePattern();
    void mutatePattern();
    void clearPattern();

    // Sample slots: the sequencer can play a loaded sample instead of the synth.
    static constexpr int numSlots = 4;
    bool loadSample (int slot, const juce::File& file);
    void clearSample (int slot);
    juce::String getSampleName (int slot) const;
    void selectSource (int index);          // 0 = synth, 1..numSlots = sample slot

    juce::AudioProcessorValueTreeState apvts;
    acid::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    acid::EngineParams readParams() const;

    std::atomic<float>* pTune, *pWave, *pCutoff, *pReso, *pEnvMod, *pDecay, *pAccent, *pSlide, *pDrive, *pVolume;
    std::atomic<float>* pSeq, *pRate, *pSwing, *pLength, *pRoot, *pScale, *pDensity;
    std::atomic<float>* pGChance, *pGType, *pGMix, *pCrush, *pDown;

    std::atomic<float>* pSource;

    struct SampleData
    {
        std::vector<float> data;
        double sampleRate = 44100.0;
    };
    juce::AudioFormatManager formats;
    juce::SpinLock sampleLock;                              // audio thread only ever try-locks
    std::array<std::shared_ptr<SampleData>, numSlots> slots;
    juce::CriticalSection nameLock;
    std::array<juce::String, numSlots> slotNames, slotPaths;

    std::vector<float> mono;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidGlitchProcessor)
};
