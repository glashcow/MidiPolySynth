/*
  ==============================================================================

    Wavetable.h
    Created: 27 Aug 2020 1:12:48pm
    Author:  ewana

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>


class WavetableOscillator
{
public:
    float currentIndex = 0.0f, tableDelta = 0.0f;

    WavetableOscillator(const juce::AudioSampleBuffer& wavetableToUse) : wavetable(wavetableToUse), tableSize(wavetable.getNumSamples() - 1)
    {
        jassert(wavetable.getNumChannels() == 1);
    }

    void setFrequency(float frequency, float sampleRate)
    {
        auto tableSizeOverSampleRate = (float)tableSize / sampleRate;
        tableDelta = frequency * tableSizeOverSampleRate;
    }

    forcedinline float getNextSample() noexcept
    {
        auto index0 = (unsigned int)currentIndex;
        auto index1 = index0 + 1;

        auto frac = currentIndex - (float)index0;

        auto* table = wavetable.getReadPointer(0);
        auto value0 = table[index0];
        auto value1 = table[index1];

        //interpolate
        auto currentSample = value0 + frac * (value1 - value0);

        if ((currentIndex += tableDelta) > (float)tableSize)
            currentIndex -= (float)tableSize;

        return currentSample;
    }

private:
    const juce::AudioSampleBuffer& wavetable;
    unsigned short int tableSize;
};
/*
class VoiceLimiter
{
public:
    VoiceLimiter() {}

    void addVoice(SynthVoice voice)
    {
        voiceQueue->push(voice);
        if (voiceQueue->size() >= numberOfVoices)
        {
            SynthVoice* temp = &voiceQueue->front();
            temp->clearNote();
        }
    }

private:
    std::queue<SynthVoice>* voiceQueue;
};*/