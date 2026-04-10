/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "x18.h"
#include <cmath>

bool threadRun;

//==============================================================================
X18gainerAudioProcessor::X18gainerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    parameters ( *this, nullptr, "pluginParams", createParameterLayout())
{
    juce::String id;

    memset(&x18_context, 0, sizeof(x18_context));
    x18_context.port = 10024;
    juce::String portStr(x18_context.port);
    
    for (int i = 1; i <= 17; ++i)
    {
        id   = "GAIN_" + juce::String (i);
        parameters.addParameterListener (id, this);
    }
    
    socket = std::make_unique<juce::DatagramSocket>(true);
    if (socket->bindToPort(0)) startThread((void*)&x18_context);
    else exit(1);//socket = nullptr;
}

X18gainerAudioProcessor::~X18gainerAudioProcessor()
{
    for (int i = 1; i <= 17; ++i)
    {
        juce::String id   = "GAIN_" + juce::String (i);
        parameters.removeParameterListener (id, this);
        setChannelGain(i, x18_context.old_gain[i - 1]);
    }
    parameters.removeParameterListener("FEEDBACK", this);
    stopThread();
    socket = nullptr;
}

juce::AudioProcessorValueTreeState::ParameterLayout X18gainerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    float min;
    float def;
    float max;
    juce::String id;
    juce::String name;

    for (int i = 1; i <= 17; ++i)
    {
        id   = "GAIN_" + juce::String (i);
        name = "Gain " + juce::String (i);
        min = -12.0;
        if (i==17)
        {
            def = 0.0;
            max = 20.0;
        }
        else
        {
            def = 20.0;
            max = 60.0;
        }
        layout.add(std::make_unique<juce::AudioParameterFloat> (id, name, min, max, def));      // значение по умолчанию
    }

    layout.add(std::make_unique<juce::AudioParameterBool> ("FEEDBACK", "Feedback", false));

    return layout;
}

void X18gainerAudioProcessor::parameterChanged(const juce::String& paramID, float newValue)
{
    juce::String id;
    float gain;
    
    if (x18_context.ip)
    {
        for (int i = 1; i <= 17; ++i)
        {
            id = "GAIN_" + juce::String (i);
            if (paramID == id)
            {
                fprintf(stderr, "Changed: %s \t %f\r\n", id.toRawUTF8(), newValue);
                gain = (newValue + 12) / (i == 17 ? 32 : 72);
                // if (i == 17) gain = (newValue + 12) / 32;
                // else gain = (newValue + 12) / 72;
                if (gain < 0.0) gain = 0.0;
                if (gain > 1.0) gain = 1.0;
                setChannelGain(i, gain);
            }
        }
        if (paramID == "FEEDBACK")
        {
            if (*parameters.getRawParameterValue("FEEDBACK")==true)
            {
                fprintf(stderr, "X18 OSC feedback enabled!\r\n");
                x18_context.flags|= FBENABLED;
            }
            else
            {
                fprintf(stderr, "X18 OSC feedback disabled!\r\n");
                x18_context.flags&=~FBENABLED;
            }
        }
    }
    else fprintf(stderr, "Mixer is not connected");
}

void X18gainerAudioProcessor::startThread(void* arg)
{
    threadRun = true;
    thread = std::thread([this, arg](){threadHandler(arg);});
}

void X18gainerAudioProcessor::stopThread()
{
    threadRun = false;
    thread.join();
    isConnected = false;
}

void X18gainerAudioProcessor::threadHandler(void* arg)
{
    juce::String id;
    x18_context_t* x18_context = (x18_context_t*)arg;
    float tmpGainM;
    float tmpGainP;

    while((!findMixer(x18_context->port))&&(threadRun == true)) usleep(1000);
    x18_context->flags|= CONNECTED;
    isConnected = true;

    if (threadRun&&isConnected)
    {
        for (int i = 1; i <= 17; ++i)
        {
            id   = "GAIN_" + juce::String (i);
            getChannelGain(i, &x18_context->old_gain[i - 1]);//*parameters.getRawParameterValue(id);
            fprintf(stderr, "Saved %s = %f\r\n", id.toRawUTF8(), x18_context->old_gain[i - 1] * (i==17 ? 32 : 72) - 12);
        }
    }
    fprintf(stderr, "GAINS SAVED...\r\n");
    while(threadRun)
    {
        //fprintf(stderr, "Running\r\n");
        juce::Thread::sleep(50);
        if (threadRun) for (int i = 1; i <= 17; ++i)
        {
            id   = "GAIN_" + juce::String (i);
            getChannelGain(i, &tmpGainM);
            tmpGainP = (parameters.getRawParameterValue(id)->load() + 12)/(i==17 ? 32 : 72);
            //fprintf(stderr, "Got: %s %f %f\r\n", id.toRawUTF8(), tmpGainM, tmpGainP);
            if ((tmpGainM!= tmpGainP)&&(threadRun == true))
            {
                if (*parameters.getRawParameterValue("FEEDBACK")==true) parameters.getParameter(id)->setValueNotifyingHost(tmpGainM);
                else
                {
                    parameters.getParameter(id)->setValueNotifyingHost(tmpGainP);
                    setChannelGain(i, tmpGainP);
                }
            }
        }
    }
    fprintf(stderr, "Thread stopped!\r\n");
}

int X18gainerAudioProcessor::findMixer(uint16_t port)
{
    int ret = 0;
    auto allAddrs = juce::IPAddress::getAllAddresses();
    juce::IPAddress bcast;
    char sendbuf[16];
    char recvbuf[1024];
    int buflen;
    tosc_message msg;

    for (const auto& ip : allAddrs)
    {
        if (ip.address[0] == 127 || ip.toString().contains(":")) continue;
        bcast = ip;
        bcast.address[3] = 255;

        fprintf(stderr, "Scanning on: %s (Broadcast: %s:%d)\n", ip.toString().toRawUTF8(), bcast.toString().toRawUTF8(), x18_context.port);
        memset(sendbuf, 0, 16);
        buflen = tosc_writeMessage(sendbuf, sizeof(sendbuf), "/xinfo", "");
        int sent = socket->write (bcast.toString(), x18_context.port, sendbuf, buflen);
        if (socket->waitUntilReady(true, 10) == 1)
        {
            buflen = socket->read(recvbuf, sizeof(recvbuf), false);
            if (buflen > 0)
            {
                if (tosc_parseMessage(&msg, recvbuf, buflen) == 0) // 0 - успех
                {
                    if (std::string(tosc_getAddress(&msg)) == "/xinfo")
                    {
                        const char* x18ip   = tosc_getNextString(&msg);

                        if (x18ip != nullptr)
                        {

                            const char* name    = tosc_getNextString(&msg);
                            const char* model   = tosc_getNextString(&msg);
                            const char* version = tosc_getNextString(&msg);
                            fprintf(stderr, "Found %s (%s) at %s, v%s\n", name, model, x18ip, version);
                            juce::IPAddress x18_address(x18ip);
                            x18_context.ip    = (uint32_t)x18_address.address[0] << 24 |
                                                (uint32_t)x18_address.address[1] << 16 |
                                                (uint32_t)x18_address.address[2] << 8  |
                                                (uint32_t)x18_address.address[3];
                            isConnected = true;
                            ret = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    return ret;
}

void X18gainerAudioProcessor::getChannelGain(uint8_t channel, float* gain)
{
    char addrbuf[128];
    char buffer[128];
    char answerbuf[128];
    int len;
    tosc_message msg;

    if (isConnected)
    {
        memset(addrbuf, 0, 128);
        memset(buffer, 0, 128);
        memset(answerbuf, 0, 128);

        sprintf(addrbuf, "/headamp/%02d/gain", channel);
        len = tosc_writeMessage(buffer, sizeof(buffer), addrbuf, "");
        if (socket != nullptr && len > 0) socket->write(juce::IPAddress(x18_context.ip).toString(), x18_context.port, buffer, len);
        if (socket->waitUntilReady(true, 20) == 1)
        {
            len = socket->read(answerbuf, sizeof(answerbuf), false);
            if ((len > 0)&&(tosc_parseMessage(&msg, answerbuf, len) == 0))
            {
                juce::String address = tosc_getAddress(&msg);
                if (address.startsWith("/headamp/")&&(address.endsWith("/gain"))) *gain = tosc_getNextFloat(&msg);
            }
        }
        else isConnected = false;
    }
}

void X18gainerAudioProcessor::setChannelGain(uint8_t channel, float val)
{
    char addrbuf[128];
    char buffer[128];
    int len;
    tosc_message msg;

    if (isConnected)
    {
        memset(addrbuf, 0, 128);
        memset(buffer, 0, 128);
        sprintf(addrbuf, "/headamp/%02d/gain", channel);
        len = tosc_writeMessage(buffer, sizeof(buffer), addrbuf, "f", val);
        if (socket != nullptr && len > 0) socket->write(juce::IPAddress(x18_context.ip).toString(), x18_context.port, buffer, len);
    }
}

//==============================================================================
const juce::String X18gainerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool X18gainerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool X18gainerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool X18gainerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double X18gainerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int X18gainerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int X18gainerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void X18gainerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String X18gainerAudioProcessor::getProgramName (int index)
{
    return {};
}

void X18gainerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void X18gainerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void X18gainerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool X18gainerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void X18gainerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
    }
}

//==============================================================================
bool X18gainerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* X18gainerAudioProcessor::createEditor()
{
    return new X18gainerAudioProcessorEditor (*this);
}

//==============================================================================
void X18gainerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree stateTree = parameters.copyState();
    juce::MemoryOutputStream stream (destData, true);
    stateTree.writeToStream (stream);

    juce::String id;
    float gain;
    float newValue;
    bool changed = false;

    for (int i = 1; i <= 17; ++i)
    {
        id = "GAIN_" + juce::String (i);
        newValue = *parameters.getRawParameterValue(id);
        fprintf(stderr, "%s \t %f\r\n", id.toRawUTF8(), newValue);
        gain = (newValue + 12) / (i == 17 ? 32 : 72);
        if (gain < 0.0) gain = 0.0;
        if (gain > 1.0) gain = 1.0;
        if (roundf(gain)!=roundf(x18_context.osc_gain[i - 1]))
        {
            x18_context.osc_gain[i - 1] = gain;
            setChannelGain(i, x18_context.osc_gain[i - 1]);
            changed = true;
        }
    }
    changeRequired = changed;
}

void X18gainerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (sizeInBytes > 0)
    {
        juce::MemoryInputStream stream (data, static_cast<size_t> (sizeInBytes), false);
        juce::ValueTree stateTree = juce::ValueTree::readFromStream (stream);
        if (stateTree.isValid()) parameters.replaceState (stateTree);
        else jassertfalse; // отладка: получено некорректное состояние
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new X18gainerAudioProcessor();
}
