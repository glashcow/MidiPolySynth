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
const unsigned int tableSize = 1 << 11;
const float sampleRate = 48000.0f;

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
//==============================================================================
class SynthVoice : public juce::MPESynthesiserVoice
{
public:
    //==============================================================================
    SynthVoice()
    {
        createSawtoothTable();
        adsr.setSampleRate(sampleRate);
        adsrParas.attack = 2.0f;
        adsrParas.release = 2.0f;
        adsr.setParameters(adsrParas);
        osc = new WavetableOscillator(sawtoothTable);
    }
    
    //==============================================================================
    void noteStarted() override
    {
        jassert(currentlyPlayingNote.isValid());
        jassert(currentlyPlayingNote.keyState == juce::MPENote::keyDown
            || currentlyPlayingNote.keyState == juce::MPENote::keyDownAndSustained);

        adsr.noteOn();
        // get data from the current MPENote
        level.setTargetValue(currentlyPlayingNote.pressure.asUnsignedFloat());
        frequency.setTargetValue((float)currentlyPlayingNote.getFrequencyInHertz());
        timbre.setTargetValue(currentlyPlayingNote.timbre.asUnsignedFloat());

        osc->setFrequency(frequency.getCurrentValue(), sampleRate);
        
    }

    void noteStopped(bool allowTailOff) override
    {
        jassert(currentlyPlayingNote.keyState == juce::MPENote::off);

        osc->currentIndex = 0.0;
        adsr.noteOff();
        //clearCurrentNote();
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
            float levelSample = getNextSample() * 0.5f;
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

    void createSawtoothTable()
    {
        sawtoothTable.setSize(1, (int)tableSize + 1);
        sawtoothTable.clear();

        auto* samples = sawtoothTable.getWritePointer(0);

        float tableDelta = 1.0f / tableSize;
        float phase = 0.0f;
        for (unsigned short int i = 0; i < tableSize; ++i)
        {
            samples[i] = -1.0f + phase;
            phase += tableDelta;
        }
        samples[tableSize] = samples[0];
    }

    void setAttack(float seconds)
    {
        adsrParas.attack = seconds;
        adsr.setParameters(adsrParas);
    }

    void setDecay(float seconds)
    {
        adsrParas.decay = seconds;
        adsr.setParameters(adsrParas);
    }

    void setSustain(float seconds)
    {
        adsrParas.sustain = seconds;
        adsr.setParameters(adsrParas);
    }

    void setRelease(float seconds)
    {
        adsrParas.release = seconds;
        adsr.setParameters(adsrParas);
    }

private:
    //==============================================================================
    unsigned short int modulo = 0;
    float getNextSample() noexcept
    {
        if (!adsr.isActive())
        {
            clearCurrentNote();
            osc->currentIndex = 0.0f;
        }
            
        return osc->getNextSample() * adsr.getNextSample();
    }

    //==============================================================================
    juce::SmoothedValue<float> level, timbre, frequency;

    juce::AudioSampleBuffer pureSineTable;
    juce::AudioSampleBuffer squareTable;
    juce::AudioSampleBuffer sawtoothTable;

    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParas;

    WavetableOscillator* osc;

    float smoothingLengthInSeconds = 0.1f;
};
//==============================================================================

//class adsrAdaptor
//{
//public:
//    adsrAdaptor() {}
            
    
//};
//==============================================================================
class SynthComponent : public juce::Component
{
public:
    SynthComponent()
    {
        addAndMakeVisible(attackSlider);
        attackSlider.setRange(0.0, 5.0);
        attackSlider.setBounds(50, 50, 200, 100);
        attackSlider.onValueChange = [this]
        {
            synth.setAttack((float)attackSlider.getValue());
        };

        addAndMakeVisible(attackLabel);
        attackLabel.setBounds(25, 50, 20, 20);
        attackLabel.setText(juce::String("A"), juce::dontSendNotification);
        //========================================================================

        addAndMakeVisible(decaySlider);
        decaySlider.setRange(0.0, 5.0);
        decaySlider.setBounds(200, 50, 200, 100);
        decaySlider.onValueChange = [this]
        {
            synth.setDecay((float)decaySlider.getValue());
        };

        addAndMakeVisible(decayLabel);
        decayLabel.setBounds(175, 50, 20, 20);
        decayLabel.setText(juce::String("D"), juce::dontSendNotification);
        //========================================================================

        addAndMakeVisible(sustainSlider);
        sustainSlider.setRange(0.0, 5.0);
        sustainSlider.setBounds(350, 50, 200, 100);
        sustainSlider.onValueChange = [this]
        {
            synth.setSustain((float)sustainSlider.getValue());
        };

        addAndMakeVisible(sustainLabel);
        sustainLabel.setBounds(325, 50, 20, 20);
        sustainLabel.setText(juce::String("S"), juce::dontSendNotification);
        //========================================================================

        addAndMakeVisible(releaseSlider);
        releaseSlider.setRange(0.0, 5.0);
        releaseSlider.setBounds(500, 50, 200, 100);
        releaseSlider.onValueChange = [this]
        {
            synth.setRelease((float)releaseSlider.getValue());
        };

        addAndMakeVisible(releaseLabel);
        releaseLabel.setBounds(475, 50, 20, 20);
        releaseLabel.setText(juce::String("R"), juce::dontSendNotification);
    }
private:
    juce::Label attackLabel;
    juce::Slider attackSlider;

    juce::Label decayLabel;
    juce::Slider decaySlider;

    juce::Label sustainLabel;
    juce::Slider sustainSlider;

    juce::Label releaseLabel;
    juce::Slider releaseSlider;

    SynthVoice synth;
};

