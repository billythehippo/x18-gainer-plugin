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
    setSize (530, 320);
    int offsetLeft;
    int offsetRight;
    
    headLabel.setText("Scanning...", juce::dontSendNotification);
    headLabel.setFont(juce::FontOptions("Consolas", 18.0f, juce::Font::bold));
    headLabel.setJustificationType(juce::Justification::centred);
    headLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    headLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkslategrey);
    headLabel.setBounds(30, 20, 470, 20);
    addAndMakeVisible(headLabel);

    for (int i = 1; i <= 17; ++i)
    {
        auto* knob = new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        knob->setLookAndFeel(&customDial);
        knob->setName ("GAIN_" + juce::String(i));
        knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
        knob->setTooltip ("Gain " + juce::String(i));
        if (i < 17) knob->setRange(-12, 60, 0.5);
        else knob->setRange(-12, 20, 0.5);
        if (i < 9) knob->setBounds(50*i - 20, 70, 50, 80);
        else if (i < 17) knob->setBounds(50*(i - 8) - 20, 200, 50, 80);
        else knob->setBounds(450, 70, 50, 80);
        addAndMakeVisible(knob);
        knobs.add(knob);
        gainAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment (audioProcessor.parameters, "GAIN_" + juce::String (i), *knob));

        auto* label = new juce::Label("label "+juce::String(i), "CH"+juce::String(i));
        if (i==17) label->setText("AUX", juce::dontSendNotification);
        label->setFont(juce::FontOptions("Consolas", 18.0f, juce::Font::bold));
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, juce::Colours::white);
        label->setColour(juce::Label::backgroundColourId, juce::Colours::darkslategrey);
        if (i < 9) label->setBounds(50*i + 1 - 20, 50, 48, 20);
        else if (i < 17) label->setBounds(50*(i - 8) - 20 + 1, 180, 48, 20);
        else label->setBounds(470 + 1 - 20, 50, 48, 20);
        addAndMakeVisible(label);
        labels.add(label);

        if (i < 17)
        {
            auto* phantomSwitcher = new juce::TextButton ();
            phantomSwitcher->setButtonText("48V");
            if (i < 9) phantomSwitcher->setBounds(50*i + (1 - (i&1))*20 - 20, 150, 30, 20);
            else phantomSwitcher->setBounds(50*(i - 8) + (1 - (i&1))*20 - 20, 280, 30, 20);
            phantomSwitcher->setClickingTogglesState(true);
            phantomSwitcher->setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
            phantomSwitcher->setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
            phantomSwitcher->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            phantomSwitcher->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
            addAndMakeVisible(phantomSwitcher);
            phantomSwitchers.add(phantomSwitcher);
            phantomAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment (audioProcessor.parameters, "PHANTOM_" + juce::String (i), *phantomSwitcher));

            if ((i&1)!=0)
            {
                auto* linkButton = new juce::TextButton();
                linkButton->setButtonText("Link");
                if (i < 9) linkButton->setBounds(50*i + 30 - 20, 150, 40, 20);
                else linkButton->setBounds(50*(i - 8) + 30 - 20, 280, 40, 20);
                linkButton->setClickingTogglesState(true);
                linkButton->onClick = [this, linkButton]
                {
                    auto active = linkButton->getToggleState();
                    if (active) linkButton->setButtonText("Unink");
                    else linkButton->setButtonText("Link");
                };
                //linkButton->setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
                linkButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
                linkButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
                linkButton->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
                addAndMakeVisible(linkButton);
                linkButtons.add(linkButton);
                linkAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment (audioProcessor.parameters, "LINK_" + juce::String ((i>>1)+1), *linkButton));
                fprintf(stderr, "Attached link %d\r\n", (i>>1)+1);
            }
        }
    }

    leaderButton.setButtonText ("FLWR");
    leaderButton.setBounds(450, 150, 50, 20);
    leaderButton.setClickingTogglesState(true);
    //leaderButton.setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
    leaderButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    leaderButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    leaderButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    leaderButton.onClick = [this]
    {
        auto active = leaderButton.getToggleState();
        if (active)
        {
            leaderButton.setButtonText("LDR");
            //leaderButton.setTooltip("Leader Mode: disable control surfaces make changes");
        }
        else
        {
            leaderButton.setButtonText("FLWR");
            //leaderButton.setTooltip("Follower Mode: Sync with the external control surface");
        }
    };
    addAndMakeVisible(leaderButton);
    feedbackAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment (audioProcessor.parameters, "LEADER", leaderButton));

    startTimer(100);
}

X18gainerAudioProcessorEditor::~X18gainerAudioProcessorEditor()
{
    stopTimer();
    gainAttachments.clear();
    phantomAttachments.clear();
    feedbackAttachment = nullptr;
    fprintf(stderr, "Attachments cleared\r\n");
}

//==============================================================================
void X18gainerAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void X18gainerAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

void X18gainerAudioProcessorEditor::timerCallback()
{
    if (audioProcessor.isConnected) headLabel.setText(audioProcessor.mixerName, juce::dontSendNotification);
    else headLabel.setText("No mixer connected! Scanning...", juce::dontSendNotification);
}
