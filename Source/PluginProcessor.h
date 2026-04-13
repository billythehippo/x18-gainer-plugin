#pragma once

#include <JuceHeader.h>
#include "tinyosc.h"
#include "x18.h"
#include <iostream>
#include <thread>


class X18gainerAudioProcessor  : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    juce::AudioProcessorValueTreeState parameters;
    //==============================================================================
    X18gainerAudioProcessor();
    ~X18gainerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    x18_context_t x18_context;

private:
    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& paramID, float newValue) override;

    std::unique_ptr<juce::DatagramSocket> socket;
    int findMixer(uint16_t port);
    void getChannelGain(uint8_t channel, float* gain);
    void setChannelGain(uint8_t channel, float val);
    void getChannelPhantom(uint8_t channel, bool* phantom);
    void setChannelPhantom(uint8_t channel, bool val);
    void getChannelLink(uint8_t channel, bool* link);
    void setChannelLink(uint8_t channel, bool val);

    std::thread thread;
    void startThread(void* arg);
    void stopThread();
    void threadHandler(void* arg);

    bool changeRequired = false;
    bool isConnected = false;
    bool oldGot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (X18gainerAudioProcessor)
};
