/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "tinyosc.h"
#include "x18.h"
#include <cmath>
#include <cstdint>

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
        if (i < 17)
        {
            id   = "PHANTOM_" + juce::String (i);
            parameters.addParameterListener (id, this);
        }
        if (i < 9)
        {
            id   = "LINK_" + juce::String (i);
            parameters.addParameterListener (id, this);
        }
    }
    parameters.addParameterListener ("FEEDBACK", this);
    
    socket = std::make_unique<juce::DatagramSocket>(true);
    if (socket->bindToPort(0)) startThread((void*)&x18_context);
    else exit(1);//socket = nullptr;
}

X18gainerAudioProcessor::~X18gainerAudioProcessor()
{
    juce::String id;

    for (int i = 1; i <= 17; ++i)
    {
        id = "GAIN_" + juce::String (i);
        parameters.removeParameterListener (id, this);
        setChannelGain(i, x18_context.old_gain[i - 1]);
        if (i < 17)
        {
            id = "PHANTOM_" + juce::String (i);
            parameters.removeParameterListener (id, this);
            if ((x18_context.phantomsOld&(1<<(i - 1)))!= 0) setChannelPhantom(i, 1);
            else setChannelPhantom(i, 0);
        }
        if (i < 9)
        {
            id = "LINK_" + juce::String (i);
            parameters.removeParameterListener (id, this);
            if ((x18_context.linksOld&(1<<(i - 1)))!= 0) setChannelLink(i, 1);
            else setChannelLink(i, 0);
        }
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

    for (int i = 1; i < 17; ++i)
    {
        id   = "PHANTOM_" + juce::String (i);
        name = "Phantom " + juce::String (i);
        layout.add(std::make_unique<juce::AudioParameterBool> (id, name, false));
    }

    for (int i = 0; i < 8; i++)
    {
        id   = "LINK_" + juce::String (1 + i);
        name = "Link " + juce::String (1 + i);
        layout.add(std::make_unique<juce::AudioParameterBool> (id, name, false));
    }

    layout.add(std::make_unique<juce::AudioParameterBool> ("FEEDBACK", "Feedback", false));

    return layout;
}

void X18gainerAudioProcessor::parameterChanged(const juce::String& paramID, float newValue)
{
    juce::String ind;
    juce::String id;
    juce::String idp;
    juce::String lid;
    int li;
    int ipr;
    float gain;
    bool newVal = false;
    bool linked;

    if (isConnected)
    {
        for (int i = 1; i <= 17; ++i)
        {
            li = (i + 1)>>1;
            lid  = "LINK_" + juce::String (li);
            if (i&1) ipr = i + 1;
            else ipr = i - 1;
            idp = "GAIN_" + juce::String (ipr);
            ind = juce::String (i);
            id = "GAIN_" + ind;
            if ((paramID == id)&&(paramsMonitorEnabled))
            {
                fprintf(stderr, "Changed: %s \t %f\r\n", id.toRawUTF8(), newValue);
                gain = (newValue + 12) / (i == 17 ? 32 : 72);
                if (gain < 0.0) gain = 0.0;
                if (gain > 1.0) gain = 1.0;
                setChannelGain(i, gain);
                //if (parameters.getRawParameterValue(lid)->load()) setChannelGain(ipr, gain);
            }

            if (i < 17)
            {
                id = "PHANTOM_" + ind;
                if (i&1) ipr = i + 1;
                else ipr = i - 1;
                idp = "PHANTOM_" + juce::String (ipr);
                if ((paramID == id)&&(paramsMonitorEnabled))
                {
                    fprintf(stderr, "Changed: %s \t %d\r\n", id.toRawUTF8(), (int)newValue);
                    linked = parameters.getRawParameterValue(lid)->load();
                    if (!linked) setChannelPhantom(i, (bool)newValue);
                }
            }

            if (i < 9)
            {
                id = "LINK_" + ind;
                if ((paramID == id)&&(paramsMonitorEnabled))
                {
                    fprintf(stderr, "Changed: %s \t %d\r\n", id.toRawUTF8(), (int)newValue);
                    setChannelLink(i, (bool)newValue);
                }
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
    juce::String idp;
    juce::String lid;
    int li;
    int ipr;
    x18_context_t* x18_context = (x18_context_t*)arg;
    float tmpGainM;
    float tmpGainP;
    bool ph;
    bool phCur;
    bool link;
    bool linkCur;
    bool linked;

    while(threadRun)
    {
        if (!isConnected)
        {
            while((!findMixer(x18_context->port))&&(threadRun == true)) usleep(1000);
            x18_context->flags|= CONNECTED;
            isConnected = true;

            if (!oldGot)
            {
                for (int i = 1; i <= 17; ++i)
                {
                    id = "GAIN_" + juce::String (i);
                    getChannelGain(i, &x18_context->old_gain[i - 1]);
                    fprintf(stderr, "Saved %s = %f\r\n", id.toRawUTF8(), x18_context->old_gain[i - 1] * (i==17 ? 32 : 72) - 12);
                    if (i<17)
                    {
                        id = "PHANTOM_" + juce::String (i);
                        getChannelPhantom(i, &ph);
                        if (ph) x18_context->phantomsOld |= (1 << (i - 1));
                        else x18_context->phantomsOld &=~ (1 << (i - 1));
                    }
                    if (i<9)
                    {
                        id = "LINK_" + juce::String (i);
                        getChannelLink(i, &link);
                        if (link) x18_context->linksOld |= (1 << (i - 1));
                        else x18_context->linksOld &=~ (1 << (i - 1));
                    }
                }
                if (isConnected)
                {
                    oldGot = true;
                    paramsMonitorEnabled = true;
                }
            }
        }
        else
        {
            //fprintf(stderr, "Running\r\n");
            juce::Thread::sleep(50);
            if (threadRun) for (int i = 1; i <= 17; ++i)
            {
                id   = "GAIN_" + juce::String (i);
                li = (i + 1)>>1;
                lid  = "LINK_" + juce::String (li);
                if (i&1) ipr = i + 1;
                else ipr = i - 1;
                idp = "GAIN_" + juce::String (ipr);
                getChannelGain(i, &tmpGainM);
                tmpGainP = (parameters.getRawParameterValue(id)->load() + 12)/(i==17 ? 32 : 72);
                //fprintf(stderr, "Got: %s %f %f\r\n", id.toRawUTF8(), tmpGainM, tmpGainP);
                if ((tmpGainM!= tmpGainP)&&(threadRun == true))
                {
                    if (*parameters.getRawParameterValue("FEEDBACK")==true) parameters.getParameter(id)->setValueNotifyingHost(tmpGainM);
                    else
                    {
                        paramsMonitorEnabled = false;
                        parameters.getParameter(id)->setValueNotifyingHost(tmpGainP);
                        setChannelGain(i, tmpGainP);
                        linked = parameters.getRawParameterValue(lid)->load();
                        if ((linked)&&(i<17))
                        {
                            parameters.getParameter(idp)->setValueNotifyingHost(tmpGainP);
                            setChannelGain(ipr, tmpGainP);
                        }
                        paramsMonitorEnabled = true;
                    }
                }
                if (i<17)
                {
                    id = "PHANTOM_" + juce::String (i);
                    idp = "PHANTOM_" + juce::String (ipr);
                    paramsMonitorEnabled = false;
                    getChannelPhantom(i, &ph);
                    phCur = parameters.getRawParameterValue(id)->load();
                    if ((ph != phCur)&&(threadRun == true))
                    {
                        if (*parameters.getRawParameterValue("FEEDBACK")==true) parameters.getParameter(id)->setValueNotifyingHost(ph);
                        else
                        {
                            linked = parameters.getRawParameterValue(lid)->load();
                            if (linked)
                            {
                                parameters.getParameter(id)->setValueNotifyingHost(phCur);
                                parameters.getParameter(idp)->setValueNotifyingHost(phCur);
                                fprintf(stderr, "%d %d\r\n", i, ipr);
                                setChannelPhantom(i, phCur);
                                setChannelPhantom(ipr, phCur);
                            }
                            else
                            {
                                parameters.getParameter(id)->setValueNotifyingHost(phCur);
                                setChannelPhantom(i, phCur);
                            }
                        }
                    }
                    paramsMonitorEnabled = true;
                }
                if (i<9)
                {
                    id = "LINK_" + juce::String (i);
                    paramsMonitorEnabled = false;
                    getChannelLink(i, &link);
                    linkCur = parameters.getRawParameterValue(id)->load();
                    if ((link != linkCur)&&(threadRun == true))
                    {
                        if (*parameters.getRawParameterValue("FEEDBACK")==true) parameters.getParameter(id)->setValueNotifyingHost(link);
                        else
                        {
                            parameters.getParameter(id)->setValueNotifyingHost(linkCur);
                            setChannelLink(i, linkCur);
                        }
                    }
                    paramsMonitorEnabled = true;
                }
            }
        }
    }

    // while((!findMixer(x18_context->port))&&(threadRun == true)) usleep(1000);
    // x18_context->flags|= CONNECTED;
    // isConnected = true;
    //
    // if (threadRun&&isConnected)
    // {
    //     for (int i = 1; i <= 17; ++i)
    //     {
    //         id = "GAIN_" + juce::String (i);
    //         getChannelGain(i, &x18_context->old_gain[i - 1]);
    //         fprintf(stderr, "Saved %s = %f\r\n", id.toRawUTF8(), x18_context->old_gain[i - 1] * (i==17 ? 32 : 72) - 12);
    //         if (i<17)
    //         {
    //             id = "PHANTOM_" + juce::String (i);
    //             getChannelPhantom(i, &ph);
    //             if (ph) x18_context->phantomsOld |= (1 << (i - 1));
    //             else x18_context->phantomsOld &=~ (1 << (i - 1));
    //         }
    //     }
    // }
    // fprintf(stderr, "GAINS SAVED...\r\n");
    // while(threadRun)
    // {
    //     //fprintf(stderr, "Running\r\n");
    //     juce::Thread::sleep(50);
    //     if (threadRun) for (int i = 1; i <= 17; ++i)
    //     {
    //         id   = "GAIN_" + juce::String (i);
    //         getChannelGain(i, &tmpGainM);
    //         tmpGainP = (parameters.getRawParameterValue(id)->load() + 12)/(i==17 ? 32 : 72);
    //         //fprintf(stderr, "Got: %s %f %f\r\n", id.toRawUTF8(), tmpGainM, tmpGainP);
    //         if ((tmpGainM!= tmpGainP)&&(threadRun == true))
    //         {
    //             if (*parameters.getRawParameterValue("FEEDBACK")==true) parameters.getParameter(id)->setValueNotifyingHost(tmpGainM);
    //             else
    //             {
    //                 parameters.getParameter(id)->setValueNotifyingHost(tmpGainP);
    //                 setChannelGain(i, tmpGainP);
    //             }
    //         }
    //         if (i<17)
    //         {
    //             id = "PHANTOM_" + juce::String (i);
    //             getChannelPhantom(i, &ph);
    //             phCur = parameters.getRawParameterValue(id)->load();
    //             if ((ph != phCur)&&(threadRun == true))
    //             {
    //                 if (*parameters.getRawParameterValue("FEEDBACK")==true) parameters.getParameter(id)->setValueNotifyingHost(ph);
    //                 else
    //                 {
    //                     parameters.getParameter(id)->setValueNotifyingHost(phCur);
    //                     setChannelPhantom(i, phCur);
    //                 }
    //             }
    //         }
    //     }
    // }
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

        //fprintf(stderr, "Scanning on: %s (Broadcast: %s:%d)\n", ip.toString().toRawUTF8(), bcast.toString().toRawUTF8(), x18_context.port);
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

void X18gainerAudioProcessor::getChannelGain(uint16_t channel, float* gain)
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

void X18gainerAudioProcessor::setChannelGain(uint16_t channel, float val)
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

void X18gainerAudioProcessor::getChannelPhantom(uint16_t channel, bool* phantom)
{
    char addrbuf[128];
    char buffer[128];
    char answerbuf[128];
    int len;
    tosc_message msg;

    if (isConnected)
    {
        sprintf(addrbuf, "/headamp/%02d/phantom", channel);
        len = tosc_writeMessage(buffer, sizeof(buffer), addrbuf, "");
        if (socket != nullptr && len > 0) socket->write(juce::IPAddress(x18_context.ip).toString(), x18_context.port, buffer, len);
        if (socket->waitUntilReady(true, 20) == 1)
        {
            len = socket->read(answerbuf, sizeof(answerbuf), false);
            if ((len > 0)&&(tosc_parseMessage(&msg, answerbuf, len) == 0))
            {
                juce::String address = tosc_getAddress(&msg);
                if (address.startsWith("/headamp/")&&address.endsWith("/phantom")) *phantom = tosc_getNextInt32(&msg);
            }
        }
        else isConnected = false;
    }
}

void X18gainerAudioProcessor::setChannelPhantom(uint16_t channel, bool val)
{
    char addrbuf[128];
    char buffer[128];
    int len;
    tosc_message msg;

    if (isConnected)
    {
        memset(addrbuf, 0, 128);
        memset(buffer, 0, 128);
        sprintf(addrbuf, "/headamp/%02d/phantom", channel);
        len = tosc_writeMessage(buffer, sizeof(buffer), addrbuf, "i", (int32_t)val);
        if (socket != nullptr && len > 0) socket->write(juce::IPAddress(x18_context.ip).toString(), x18_context.port, buffer, len);
    }
}

void X18gainerAudioProcessor::getChannelLink(uint16_t channel, bool* link)
{
    char addrbuf[128];
    char buffer[128];
    char answerbuf[128];
    int len;
    tosc_message msg;

    if ((isConnected)&&(channel > 0))
    {
        sprintf(addrbuf, "/config/chlink/%d-%d", (channel<<1) - 1, (channel<<1));
        len = tosc_writeMessage(buffer, sizeof(buffer), addrbuf, "");
        if (socket != nullptr && len > 0) socket->write(juce::IPAddress(x18_context.ip).toString(), x18_context.port, buffer, len);
        if (socket->waitUntilReady(true, 20) == 1)
        {
            len = socket->read(answerbuf, sizeof(answerbuf), false);
            if ((len > 0)&&(tosc_parseMessage(&msg, answerbuf, len) == 0))
            {
                juce::String address = tosc_getAddress(&msg);
                if (address.startsWith("/config/chlink/")) *link = tosc_getNextInt32(&msg);
            }
        }
        else isConnected = false;
    }
}

void X18gainerAudioProcessor::setChannelLink(uint16_t channel, bool val)
{
    char addrbuf[128];
    char buffer[128];
    int len;
    tosc_message msg;

    if ((isConnected)&&(channel > 0))
    {
        memset(addrbuf, 0, 128);
        memset(buffer, 0, 128);
        sprintf(addrbuf, "/config/chlink/%d-%d", (channel<<1) - 1, (channel<<1));
        len = tosc_writeMessage(buffer, sizeof(buffer), addrbuf, "i", (int32_t)val);
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

    for (int i = 1; i <= 17; i++)
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

    for (int i = 1; i < 17; i++)
    {
        id = "PHANTOM_" + juce::String (i);
        newValue = *parameters.getRawParameterValue(id);
        setChannelPhantom(i, (bool)newValue);
    }

    for (int i = 1; i < 9; i++)
    {
        id = "LINK_" + juce::String (i);
        newValue = *parameters.getRawParameterValue(id);
        setChannelLink(i, (bool)newValue);
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
