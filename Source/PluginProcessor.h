#pragma once
#include <JuceHeader.h>
#include "Common.h"
#include "TapeBank.h"
#include "MelloEngine.h"
#include "FxRack.h"

class MelloAudioProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::Timer
{
public:
    MelloAudioProcessor();
    ~MelloAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MELLOLAB"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    ml::TapeBank bank;
    ml::MelloEngine engine;      // publico: la interfaz lee engine.keyLevel / motorOut para animar
    FxRack rack;

    // cintas A / B / C
    void setBankSynth (int slot, int synthIdx);
    void setBankFile (int slot, const juce::File& f, bool applyDetectedRoot = true);
    juce::String bankName (int slot) const { return bank.getName (slot); }
    bool bankIsUser (int slot) const { return bank.isUser (slot); }
    float bankConfidence (int slot) const;

    void uiNote (int note, float vel01, bool on);     // teclas con el raton
    std::atomic<float> outLevel { 0.f };

private:
    ml::MelloParams readParams() const;
    void handleMidi (const juce::MidiMessage& m);
    void timerCallback() override { bank.collectGarbage(); }
    void handleAsyncUpdate() override;

    double fs = 44100.0;
    std::atomic<int> pendingRoot[ml::kBanks];
    juce::AbstractFifo uiFifo { 64 };
    int uiBuf[64] {};
    juce::ThreadPool pool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelloAudioProcessor)
};
