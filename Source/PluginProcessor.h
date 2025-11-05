#pragma once

#include <JuceHeader.h>

#include "lo/lo.h"
#include "x18.h"
#ifndef _WIN32
#include <arpa/inet.h>
#endif
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

    float getChannelGain(uint8_t channel);
    void setChannelGain(uint8_t channel, float val);

    x18_context_t x18_context;
    //float x18_gains[17];
    //float prj_gains[17];

private:
    //==============================================================================
    void parameterChanged (const juce::String& paramID, float newValue) override;
    static int replyHandler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
    int findMixer(uint16_t port);

    std::thread thread;
    void startThread(void* arg);
    void stopThread();
    void threadHandler(void* arg);

    lo_server_thread osc_server_thread;
    lo_server osc_server;
    lo_address osc_address;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (X18gainerAudioProcessor)
};
