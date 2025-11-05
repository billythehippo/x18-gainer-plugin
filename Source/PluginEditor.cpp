/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
X18gainerAudioProcessorEditor::X18gainerAudioProcessorEditor (X18gainerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (600, 240);
    
    for (int i = 1; i <= 17; ++i)
    {
        auto* knob = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        knob->setName ("GAIN_" + juce::String(i));
        knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
        knob->setTooltip ("Gain " + juce::String(i));
        if (i < 17) knob->setRange(-12, 60, 0.5);
        else knob->setRange(-12, 20, 0.5);
        if (i < 9) knob->setBounds(50*i, 30, 50, 80);
        else if (i < 17) knob->setBounds(50*(i - 8), 140, 50, 80);
        else knob->setBounds(500, 30, 50, 80);
        addAndMakeVisible(knob);
        knobs.add(knob);
        attachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment (audioProcessor.parameters, "GAIN_" + juce::String (i), *knob));

        auto* label = new juce::Label("label "+juce::String(i), "CH"+juce::String(i));
        if (i==17) label->setText("AUX", juce::dontSendNotification);
        label->setFont(juce::Font(18.0f, juce::Font::bold));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        label->setColour(juce::Label::backgroundColourId, juce::Colours::darkslategrey);
        if (i < 9) label->setBounds(50*i + 1, 10, 48, 20);
        else if (i < 17) label->setBounds(50*(i - 8) + 1, 120, 48, 20);
        else label->setBounds(500 + 1, 10, 48, 20);
        addAndMakeVisible(label);
        labels.add(label);
    }

    feedbackButton.setButtonText ("FB");
    feedbackButton.setBounds(500, 105, 50, 50);
    addAndMakeVisible(feedbackButton);
    feedbackAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment (audioProcessor.parameters, "FEEDBACK", feedbackButton));
}

X18gainerAudioProcessorEditor::~X18gainerAudioProcessorEditor()
{
}

//==============================================================================
void X18gainerAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void X18gainerAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
