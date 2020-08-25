#pragma once

#include "Synth.h"
#include "Visualiser.h"

class MainComponent : public juce::Component,
    private juce::AudioIODeviceCallback,  
    private juce::MidiInputCallback       
{
public:
    //==============================================================================
    MainComponent()
        : audioSetupComp(audioDeviceManager, 0, 0, 0, 256,
            true,
            true, true, false)
    {
        audioDeviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
        audioDeviceManager.addMidiInputDeviceCallback({}, this); 
        audioDeviceManager.addAudioCallback(this);

        addAndMakeVisible(audioSetupComp);
        addAndMakeVisible(visualiserViewport);

        visualiserViewport.setScrollBarsShown(false, true);
        visualiserViewport.setViewedComponent(&visualiserComp, false);
        visualiserViewport.setViewPositionProportionately(0.5, 0.0);

        visualiserInstrument.addListener(&visualiserComp);

        for (auto i = 0; i < 15; ++i)
            synth.addVoice(new MPEDemoSynthVoice());

        synth.enableLegacyMode(24);
        synth.setVoiceStealingEnabled(false);

        visualiserInstrument.enableLegacyMode(24);

        setSize(650, 560);
    }

    ~MainComponent() override
    {
        audioDeviceManager.removeMidiInputDeviceCallback({}, this);
    }

    //==============================================================================
    void resized() override
    {
        auto visualiserCompWidth = 2800;
        auto visualiserCompHeight = 300;

        auto r = getLocalBounds();

        visualiserViewport.setBounds(r.removeFromBottom(visualiserCompHeight));
        visualiserComp.setBounds({ visualiserCompWidth,
                                    visualiserViewport.getHeight() - visualiserViewport.getScrollBarThickness() });

        audioSetupComp.setBounds(r);
    }

    //==============================================================================
    void audioDeviceIOCallback(const float** /*inputChannelData*/, int /*numInputChannels*/,
        float** outputChannelData, int numOutputChannels,
        int numSamples) override
    {
        // make buffer
        juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels, numSamples);

        // clear it to silence
        buffer.clear();

        juce::MidiBuffer incomingMidi;

        // get the MIDI messages for this audio block
        midiCollector.removeNextBlockOfMessages(incomingMidi, numSamples);

        // synthesise the block
        synth.renderNextBlock(buffer, incomingMidi, 0, numSamples);
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        auto sampleRate = device->getCurrentSampleRate();
        midiCollector.reset(sampleRate);
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }

    void audioDeviceStopped() override {}

private:
    //==============================================================================
    void handleIncomingMidiMessage(juce::MidiInput* /*source*/,
        const juce::MidiMessage& message) override
    {
        visualiserInstrument.processNextMidiEvent(message);
        midiCollector.addMessageToQueue(message);
    }

    //==============================================================================
    juce::AudioDeviceManager audioDeviceManager;         // [3]
    juce::AudioDeviceSelectorComponent audioSetupComp;   // [4]

    Visualiser visualiserComp;
    juce::Viewport visualiserViewport;

    juce::MPEInstrument visualiserInstrument;
    juce::MPESynthesiser synth;
    juce::MidiMessageCollector midiCollector;            // [5]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};