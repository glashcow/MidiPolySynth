/*
  ==============================================================================

    Synth.h
    Created: 25 Aug 2020 1:29:45pm
    Author:  ewana

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//Global Synth Variables
extern juce::SmoothedValue<float> sustainRamp;

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
    const int tableSize = 1 << 9;
};
//==============================================================================
class SynthVoice : public juce::MPESynthesiserVoice
{
public:
    //==============================================================================
    SynthVoice()
    {
        createSquareWavetable();
        osc = new WavetableOscillator(squareTable);
    }

    //==============================================================================
    void noteStarted() override
    {
        jassert(currentlyPlayingNote.isValid());
        jassert(currentlyPlayingNote.keyState == juce::MPENote::keyDown
            || currentlyPlayingNote.keyState == juce::MPENote::keyDownAndSustained);

        // get data from the current MPENote
        level.setTargetValue(currentlyPlayingNote.pressure.asUnsignedFloat());
        frequency.setTargetValue((float)currentlyPlayingNote.getFrequencyInHertz());
        timbre.setTargetValue(currentlyPlayingNote.timbre.asUnsignedFloat());

        osc->setFrequency(frequency.getCurrentValue(), 48000.0);
        tailOff = 1;
    }

    void noteStopped(bool allowTailOff) override
    {
        jassert(currentlyPlayingNote.keyState == juce::MPENote::off);

        clearCurrentNote();
        osc->currentIndex = 0.0;
    }

    void notePressureChanged() override
    {
        level.setTargetValue(currentlyPlayingNote.pressure.asUnsignedFloat());
    }

    void notePitchbendChanged() override
    {
        frequency.setTargetValue(currentlyPlayingNote.getFrequencyInHertz());
    }

    void noteTimbreChanged() override
    {
        timbre.setTargetValue(currentlyPlayingNote.timbre.asUnsignedFloat());
    }

    void noteKeyStateChanged() override {}

    void setCurrentSampleRate(double newRate) override
    {
        if (currentSampleRate != newRate)
        {
            noteStopped(false);
            currentSampleRate = newRate;

            level.reset(currentSampleRate, smoothingLengthInSeconds);
            timbre.reset(currentSampleRate, smoothingLengthInSeconds);
            frequency.reset(currentSampleRate, smoothingLengthInSeconds);
        }
    }

    //==============================================================================
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples) override
    {
        for (auto sample = 0; sample < numSamples; ++sample)
        {
            float levelSample = getNextSample() * 0.25f;
            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample(i, startSample, levelSample);

            ++startSample;
        }
        
    }

    void createPureSineWavetable()
    {
        pureSineTable.setSize(1, (int)tableSize + 1);
        pureSineTable.clear();

        auto* samples = pureSineTable.getWritePointer(0);

        auto angleDelta = juce::MathConstants<float>::twoPi / (float)(tableSize - 1);
        auto currentAngle = 0.0;

        for (unsigned short int i = 0; i < tableSize; ++i)
        {
            auto sample = std::sin(currentAngle);
            samples[i] += (float)sample;
            currentAngle += angleDelta;
        }
        samples[tableSize] = samples[0];
    }

    void createSquareWavetable()
    {
        squareTable.setSize(1, (int)tableSize + 1);
        squareTable.clear();

        auto* samples = squareTable.getWritePointer(0);

        for (unsigned short int i = 0; i < tableSize; ++i)
        {
            if (i < (tableSize / 2))
            {
                samples[i] = 1.0;
            }
            else
            {
                samples[i] = -1.0;
            }
        }
        samples[tableSize] = samples[0];
    }

private:
    //==============================================================================
    unsigned short int modulo = 0;
    float getNextSample() noexcept
    {
        ++modulo;
        if (modulo == 128)
        {
            tailOff *= sustainRamp.getCurrentValue();
            modulo = 0;
        }
        return osc->getNextSample() * tailOff;
    }

    //==============================================================================
    juce::SmoothedValue<float> level, timbre, frequency;
    float tailOff;
    juce::AudioSampleBuffer pureSineTable;
    juce::AudioSampleBuffer squareTable;

    WavetableOscillator* osc;

    // some useful constants
    static constexpr auto maxLevel = 0.1;
    static constexpr auto maxLevelDb = 0.2;
    static constexpr auto smoothingLengthInSeconds = 0.01;
    const unsigned short int tableSize = 1 << 9;
};

//==============================================================================