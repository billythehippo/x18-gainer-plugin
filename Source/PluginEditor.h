#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class X18gainerAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    X18gainerAudioProcessorEditor (X18gainerAudioProcessor&);
    ~X18gainerAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    X18gainerAudioProcessor& audioProcessor;
    
    juce::OwnedArray<juce::Label>  labels;
    juce::OwnedArray<juce::Slider> knobs;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    juce::ToggleButton feedbackButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> feedbackAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (X18gainerAudioProcessorEditor)
};
