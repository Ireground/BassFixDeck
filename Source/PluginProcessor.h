#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"

class BassFixDeckProcessor : public juce::AudioProcessor
{
public:
    BassFixDeckProcessor();
    ~BassFixDeckProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BassFixDeck"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    bfd::CurveParams getCurveParams() const;

    // Live values for the GUI
    std::atomic<float> uiPitch { 0.0f }, uiGainDb { 0.0f }, uiHarm { 0.0f }, uiConfidence { 0.0f };

    // Editor size is remembered per instance (and saved with the project)
    std::atomic<int> editorWidth { 860 }, editorHeight { 540 };

private:
    void updateControl (int numSamples);

    bfd::PitchTracker tracker;

    struct Channel
    {
        bfd::SVF eq, harmBand, harmHp1, harmHp2;
        double env = 0;
        std::vector<float> delay;
        int delayPos = 0;
    };
    std::array<Channel, 2> ch;

    double sr = 44100;
    int lookaheadSamples = 0;
    int controlCounter = 0;

    double heldPitch = 0, smoothPitch = 0;
    double silentTime = 0;
    double gainDbSmoothed = 0, harmSmoothed = 0;
    float  outGainSmoothed = 1.0f, mixSmoothed = 1.0f;
    bool   lastLookahead = true;

    std::atomic<float>* pBoomNote, *pBoomDepth, *pBodyNote, *pBodyDepth, *pRange,
                      * pFocus, *pHarm, *pSpeed, *pMix, *pOut, *pLookahead, *pDelta;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassFixDeckProcessor)
};
