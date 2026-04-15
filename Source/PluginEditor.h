#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "dial.h"

class X18gainerAudioProcessorEditor  :  public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    X18gainerAudioProcessorEditor (X18gainerAudioProcessor&);
    ~X18gainerAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (X18gainerAudioProcessorEditor)

    X18gainerAudioProcessor& audioProcessor;
    
    CustomDial customDial;

    juce::Label headLabel;
    juce::OwnedArray<juce::Label>  labels;
    juce::OwnedArray<juce::Slider> knobs;
    juce::OwnedArray<juce::TextButton> phantomSwitchers;
    juce::OwnedArray<juce::TextButton> linkButtons;
    juce::TextButton leaderButton;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> phantomAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> feedbackAttachment;
};
