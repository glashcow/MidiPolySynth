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
const unsigned int tableSize = 1 << 11, numberOfTablesInMasterTable = 2, totalTableSize = tableSize * numberOfTablesInMasterTable + 1;
const float sampleRate = 48000.0f;
juce::ADSR::Parameters adsrParas;
juce::SmoothedValue<float> filterCutoff,  oscMix = 0.0f;
extern unsigned short int numberOfVoices;


class WavetableOscillator
{
private:
    const juce::AudioSampleBuffer& wavetable;
    unsigned short int tableSize;
public:
    WavetableOscillator(juce::AudioSampleBuffer& wavetableToUse) : wavetable(wavetableToUse), tableSize(wavetable.getNumSamples() - 1)
    {
        jassert(wavetable.getNumChannels() == 1);
    }

    float currentIndex = 0.0f, tableDelta = 0.0f;

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

        if ((currentIndex += tableDelta) > oscMix.getCurrentValue() + (float)tableSize)
            currentIndex -= (float)tableSize;
        return currentSample;
    }
};
//=================================================================================
class SynthVoice : public juce::MPESynthesiserVoice
{
public:
    //==============================================================================
    SynthVoice()
    {
        writeWholeWaveTable();
        adsr.setSampleRate(sampleRate);
        masterOscillator = new WavetableOscillator(masterWaveTable);
        filter.setCoefficients( filterCoeffs.makeLowPass(sampleRate, filterCutoff.getCurrentValue() ) );
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

        masterOscillator->setFrequency(frequency.getCurrentValue(), sampleRate);
    }

    void noteStopped(bool allowTailOff) override
    {
        jassert(currentlyPlayingNote.keyState == juce::MPENote::off);
        adsr.noteOff();
        //limiter->add(*this);
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
        adsr.setParameters(adsrParas);
        filter.setCoefficients(filterCoeffs.makeLowPass(sampleRate, filterCutoff.getCurrentValue() ));
    }

   
    void writeSquareWavetable(float* samples, unsigned short int startSample)
    {
        for (unsigned short int i = startSample; i < startSample + tableSize; ++i)
        {
            (i < ((startSample + tableSize) / 2)) ? samples[i] = 0.6 : samples[i] = -0.6;
        }
    }

    void writeSawtoothTable(float* samples, unsigned short int startSample)
    {
        float tableDelta = 1.0f / tableSize;
        float phase = 0.0f;
        for (unsigned short int i = startSample; i < startSample + tableSize; ++i)
        {
            samples[i] = (-1.0f + phase) * 0.6;
            phase += tableDelta;
        }
    }
    
    void writeWholeWaveTable()
    {
        masterWaveTable.setSize(1, totalTableSize);
        masterWaveTable.clear();
        auto* samples = masterWaveTable.getWritePointer(0);
        writeSawtoothTable(samples, 0);
        writeSquareWavetable(samples, tableSize);
        samples[totalTableSize] = samples[0];
    }

    void clearNote()
    {
        clearCurrentNote();
    }

private:
    //==============================================================================
    float getNextSample() noexcept
    {
        if (!adsr.isActive())
        {
            clearCurrentNote();
            masterOscillator->currentIndex = oscMix.getCurrentValue();
        }
            
        float rawSample = masterOscillator->getNextSample();
        return filter.processSingleSampleRaw(rawSample * adsr.getNextSample() * 0.5f);
    }
    //==============================================================================
    juce::SmoothedValue<float> level, timbre, frequency;
    juce::AudioSampleBuffer masterWaveTable;
    
    juce::ADSR adsr;
    juce::IIRFilter filter;
    juce::IIRCoefficients filterCoeffs;

    WavetableOscillator* masterOscillator;
    
    float smoothingLengthInSeconds = 0.1f;
};
//==============================================================================
/*
class VoiceLimiter
{
public:
    VoiceLimiter() {}

    void add(SynthVoice& voice)
    {
        voices.push(voice);
        if (voices.size() >= numberOfVoices)
        {
            SynthVoice& temp = voices.front();
            temp.clearNote();
        }
    }

private:
    static std::queue<SynthVoice> voices;
};*/

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
            adsrParas.attack = (float)attackSlider.getValue();
        };

        addAndMakeVisible(attackLabel);
        attackLabel.setBounds(75, 50, 20, 20);
        attackLabel.setText(juce::String("A"), juce::dontSendNotification);
        //========================================================================

        addAndMakeVisible(decaySlider);
        decaySlider.setRange(0.0, 5.0);
        decaySlider.setBounds(250, 50, 200, 100);
        decaySlider.onValueChange = [this]
        {
            adsrParas.decay = (float)decaySlider.getValue();
        };

        addAndMakeVisible(decayLabel);
        decayLabel.setBounds(275, 50, 20, 20);
        decayLabel.setText(juce::String("D"), juce::dontSendNotification);
        //========================================================================

        addAndMakeVisible(sustainSlider);
        sustainSlider.setRange(0.0, 5.0);
        sustainSlider.setBounds(450, 50, 200, 100);
        sustainSlider.onValueChange = [this]
        {
            adsrParas.sustain = (float)sustainSlider.getValue();
        };

        addAndMakeVisible(sustainLabel);
        sustainLabel.setBounds(475, 50, 20, 20);
        sustainLabel.setText(juce::String("S"), juce::dontSendNotification);
        //========================================================================

        addAndMakeVisible(releaseSlider);
        releaseSlider.setRange(0.0, 5.0);
        releaseSlider.setBounds(650, 50, 200, 100);
        releaseSlider.onValueChange = [this]
        {
            adsrParas.release = (float)releaseSlider.getValue();
        };

        addAndMakeVisible(releaseLabel);
        releaseLabel.setBounds(675, 50, 20, 20);
        releaseLabel.setText(juce::String("R"), juce::dontSendNotification);
        //========================================================================

        addAndMakeVisible(oscMixSlider);
        oscMixSlider.setBounds(50, 300, 200, 100);
        oscMixSlider.setRange(0.0, 1024.0);
        oscMixSlider.onValueChange = [this]
        {
            oscMix = oscMixSlider.getValue();
        };
        //========================================================================

        addAndMakeVisible(cutoffSlider);
        cutoffSlider.setBounds(50, 400, 400, 100);
        cutoffSlider.setRange(20.0, 20000.0f);
        cutoffSlider.setSkewFactorFromMidPoint(5000.0);
        cutoffSlider.onValueChange = [this]
        {
            filterCutoff = cutoffSlider.getValue();
        };
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

    juce::Slider oscMixSlider;
    juce::Slider cutoffSlider;
};

