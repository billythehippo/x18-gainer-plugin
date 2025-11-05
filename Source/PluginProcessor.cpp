/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    parameters ( *this, nullptr, juce::Identifier ("pluginParams"), juce::AudioProcessorValueTreeState::ParameterLayout())
{
    float min, def, max;
    juce::String id;
    juce::String name;
    
    memset(&x18_context, 0, sizeof(x18_context));
    x18_context.port = 10024;
    juce::String portStr(x18_context.port);
    
    osc_server_thread = lo_server_thread_new(portStr.getCharPointer().getAddress(), NULL);
    lo_server_thread_add_method(osc_server_thread, NULL, NULL, X18gainerAudioProcessor::replyHandler, (void*)&x18_context);
    if (lo_server_thread_start(osc_server_thread)==-1) exit(1);
    osc_server = lo_server_thread_get_server(osc_server_thread);
    
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
        parameters.createAndAddParameter (std::make_unique<juce::AudioParameterFloat> (id, name, min, max, def));      // значение по умолчанию
        parameters.addParameterListener (id, this);
    }
    
    parameters.createAndAddParameter (std::make_unique<juce::AudioParameterBool> ("FEEDBACK", "Feedback", false));
    parameters.addParameterListener ("FEEDBACK", this);
    
    parameters.state = juce::ValueTree (juce::Identifier ("pluginParams"));
    
    startThread((void*)&x18_context);

    // while(!findMixer(x18_context.port)) usleep(1000);
    //
    // for (int i = 1; i <= 17; ++i)
    // {
    //     id   = "GAIN_" + juce::String (i);
    //     x18_gains[i - 1] = getChannelGain(i);//*parameters.getRawParameterValue(id);
    //     fprintf(stderr, "%s = %f\r\n", id.toRawUTF8(), x18_gains[i - 1] * (i==17 ? 32 : 72) - 12);
    // }
}

X18gainerAudioProcessor::~X18gainerAudioProcessor()
{
    stopThread();
    for (int i = 1; i <= 17; ++i)
    {
        juce::String id   = "GAIN_" + juce::String (i);
        parameters.removeParameterListener (id, this);
        setChannelGain(i, x18_context.old_gain[i - 1]);
    }
    lo_server_thread_stop(osc_server_thread);
    if (!lo_address_get_port(osc_address)) lo_address_free(osc_address);
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
                fprintf(stderr, "%s \t %f\r\n", id.toRawUTF8(), newValue);
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
    //thread = std::thread(X18gainerAudioProcessor::threadHandler, arg);
    thread = std::thread([this, arg](){threadHandler(arg);});
}

void X18gainerAudioProcessor::stopThread()
{
    threadRun = false;
    thread.join();
    fprintf(stderr, "Thread stopped!\r\n");
}

void X18gainerAudioProcessor::threadHandler(void* arg)
{
    juce::String id;
    x18_context_t* x18_context = (x18_context_t*)arg;

    while(!findMixer(x18_context->port)) usleep(1000);

    for (int i = 1; i <= 17; ++i)
    {
        id   = "GAIN_" + juce::String (i);
        x18_context->old_gain[i - 1] = getChannelGain(i);//*parameters.getRawParameterValue(id);
        fprintf(stderr, "%s = %f\r\n", id.toRawUTF8(), x18_context->old_gain[i - 1] * (i==17 ? 32 : 72) - 12);
    }

    while(threadRun)
    {
        //fprintf(stderr, "Running\r\n");
        usleep(100000);
        for (int i = 1; i <= 17; ++i)
        {
            id   = "GAIN_" + juce::String (i);
            float tmpGainM = getChannelGain(i);
            float tmpGainP = (parameters.getRawParameterValue(id)->load() + 12)/(i==17 ? 32 : 72);
            fprintf(stderr, "%s %f %f\r\n", id.toRawUTF8(), tmpGainM, tmpGainP);
            if (tmpGainM!= tmpGainP)
                if (*parameters.getRawParameterValue("FEEDBACK")==true) parameters.getParameter(id)->setValueNotifyingHost(tmpGainM);
                else
                {
                    parameters.getParameter(id)->setValueNotifyingHost(tmpGainP);
                    setChannelGain(i, tmpGainP);
                }
        }
    }
}

int X18gainerAudioProcessor::findMixer(uint16_t port)
{
    struct in_addr ipstd;
    lo_message msg = lo_message_new();
    
    auto allAddrs = juce::IPAddress::getAllAddresses();
    for (const auto& ip : allAddrs) 
    {
        if (strncmp(ip.toString().toRawUTF8(), "127.0.0.", 8)) 
        {
            inet_pton(AF_INET, ip.toString().toRawUTF8(), &ipstd);
            ipstd.s_addr|= 0xFF000000;
            osc_address = lo_address_new_with_proto(LO_UDP, inet_ntoa(ipstd), juce::String(port).toRawUTF8());
            lo_send_message_from(osc_address, osc_server, "/xinfo", msg);
            usleep(5000);
            if (x18_context.ip)
            {
                lo_address_free(osc_address);
                ipstd.s_addr = x18_context.ip;
                fprintf(stderr, "Mixer found on %s\r\n", inet_ntoa(ipstd));
                osc_address = lo_address_new_with_proto(LO_UDP, inet_ntoa(ipstd), juce::String(port).toRawUTF8());
                return 1;
                break;
            }
            lo_address_free(osc_address);
        }
    }
    return 0;
}

float X18gainerAudioProcessor::getChannelGain(uint8_t channel)
{
    lo_message msg = lo_message_new();
    juce::String path("/headamp/");
    juce::String zero("0");
    juce::String channelNumber(channel);
    juce::String gainAddr("/gain");
    if (channel<10) path += zero;
    path += channelNumber;
    path += gainAddr;
    x18_context.flags&=~ANSWERED;
    lo_send_message_from(osc_address, osc_server, path.toStdString().c_str(), msg);
    while ((x18_context.flags&ANSWERED)!=ANSWERED) usleep(100);
    lo_message_free(msg);
    return x18_context.osc_gain[channel];
}

void X18gainerAudioProcessor::setChannelGain(uint8_t channel, float val)
{
    lo_message msg = lo_message_new();
    juce::String path = "/headamp/";
    juce::String zero("0");
    juce::String channelNumber(channel);
    juce::String gainAddr("/gain");
    if (channel<10) path += zero;
    path += channelNumber;
    path += gainAddr;
    lo_message_add_float(msg, val);
    lo_send_message_from(osc_address, osc_server, path.toStdString().c_str(), msg);
    lo_message_free(msg);
}

int X18gainerAudioProcessor::replyHandler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
    struct in_addr addr;
    
    x18_context_t* mixer_context = (x18_context_t*)user_data;
    //(void)msg;
    //(void)user_data;

    if (argc > 0)
    {
        if (strncmp(path, "/xinfo", 6) == 0)
        {
            inet_pton(AF_INET, &argv[0]->s, &addr);
            //fprintf(stderr, "%s\r\n", inet_ntoa(addr));
            mixer_context->ip = addr.s_addr;
        }
        else if (strncmp(path, "/headamp", 8)==0)
        {
            if (strstr(path, "/gain")!= nullptr)
            {
                uint8_t channel = atoi(path + 9);
                float val = argv[0]->f;
                mixer_context->osc_gain[channel] = argv[0]->f;
                if ((mixer_context->flags&&FBENABLED)==FBENABLED) fprintf(stderr, "GAIN GETTED");
                mixer_context->flags|= ANSWERED;
            }
        }
    }


    //for (int i = 0; i < argc; ++i)
    //{
    //    switch (types[i])
    //    {
    //    case 'i': fprintf(stderr, "  %d: %d (int)\n", i, argv[i]->i); break;
    //    case 'f': fprintf(stderr, "  %d: %f (float)\n", i, argv[i]->f); break;
    //    case 's': fprintf(stderr, "  %d: %s (string)\n", i, &argv[i]->s); break;
    //    default:  fprintf(stderr, "  %d: <неизвестный тип '%c'>\n", i, types[i]); break;
    //    }
    //}
    return 0;
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

    for (int i = 1; i <= 17; ++i)
    {
        id = "GAIN_" + juce::String (i);
        newValue = *parameters.getRawParameterValue(id);
        fprintf(stderr, "%s \t %f\r\n", id.toRawUTF8(), newValue);
        gain = (newValue + 12) / (i == 17 ? 32 : 72);
        if (gain < 0.0) gain = 0.0;
        if (gain > 1.0) gain = 1.0;
        setChannelGain(i, gain);
    }
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
