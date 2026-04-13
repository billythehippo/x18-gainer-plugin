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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (X18gainerAudioProcessorEditor)

    X18gainerAudioProcessor& audioProcessor;
    
    juce::OwnedArray<juce::Label>  labels;
    juce::OwnedArray<juce::Slider> knobs;
    juce::OwnedArray<juce::ToggleButton> phantomSwitchers;
    juce::OwnedArray<juce::TextButton>linkButtons;
    juce::ToggleButton feedbackButton;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> phantomAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> feedbackAttachment;
};
