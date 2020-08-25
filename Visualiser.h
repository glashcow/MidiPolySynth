/*
  ==============================================================================

    Visualiser.h
    Created: 25 Aug 2020 1:29:36pm
    Author:  ewana

  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>

//==============================================================================
class NoteComponent : public juce::Component
{
public:
    NoteComponent(const juce::MPENote& n)
        : note(n), colour(juce::Colours::white)
    {}

    //==============================================================================
    void update(const juce::MPENote& newNote, juce::Point<float> newCentre)
    {
        note = newNote;
        centre = newCentre;

        setBounds(getSquareAroundCentre(juce::jmax(getNoteOnRadius(), getNoteOffRadius(), getPressureRadius()))
            .getUnion(getTextRectangle())
            .getSmallestIntegerContainer()
            .expanded(3));

        repaint();
    }

    //==============================================================================
    void paint(juce::Graphics& g) override
    {
        if (note.keyState == juce::MPENote::keyDown || note.keyState == juce::MPENote::keyDownAndSustained)
            drawPressedNoteCircle(g, colour);
        else if (note.keyState == juce::MPENote::sustained)
            drawSustainedNoteCircle(g, colour);
        else
            return;

        drawNoteLabel(g, colour);
    }

    //==============================================================================
    juce::MPENote note;
    juce::Colour colour;
    juce::Point<float> centre;

private:
    //==============================================================================
    void drawPressedNoteCircle(juce::Graphics& g, juce::Colour zoneColour)
    {
        g.setColour(zoneColour.withAlpha(0.3f));
        g.fillEllipse(translateToLocalBounds(getSquareAroundCentre(getNoteOnRadius())));
        g.setColour(zoneColour);
        g.drawEllipse(translateToLocalBounds(getSquareAroundCentre(getPressureRadius())), 2.0f);
    }

    //==============================================================================
    void drawSustainedNoteCircle(juce::Graphics& g, juce::Colour zoneColour)
    {
        g.setColour(zoneColour);
        juce::Path circle, dashedCircle;
        circle.addEllipse(translateToLocalBounds(getSquareAroundCentre(getNoteOffRadius())));
        const float dashLengths[] = { 3.0f, 3.0f };
        juce::PathStrokeType(2.0, juce::PathStrokeType::mitered).createDashedStroke(dashedCircle, circle, dashLengths, 2);
        g.fillPath(dashedCircle);
    }

    //==============================================================================
    void drawNoteLabel(juce::Graphics& g, juce::Colour)
    {
        auto textBounds = translateToLocalBounds(getTextRectangle()).getSmallestIntegerContainer();
        g.drawText("+", textBounds, juce::Justification::centred);
        g.drawText(juce::MidiMessage::getMidiNoteName(note.initialNote, true, true, 3), textBounds, juce::Justification::centredBottom);
        g.setFont(juce::Font(22.0f, juce::Font::bold));
        g.drawText(juce::String(note.midiChannel), textBounds, juce::Justification::centredTop);
    }

    //==============================================================================
    juce::Rectangle<float> getSquareAroundCentre(float radius) const noexcept
    {
        return juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);
    }

    juce::Rectangle<float> translateToLocalBounds(juce::Rectangle<float> r) const noexcept
    {
        return r - getPosition().toFloat();
    }

    juce::Rectangle<float> getTextRectangle() const noexcept
    {
        return juce::Rectangle<float>(30.0f, 50.0f).withCentre(centre);
    }

    float getNoteOnRadius()   const noexcept { return note.noteOnVelocity.asUnsignedFloat() * maxNoteRadius; }
    float getNoteOffRadius()  const noexcept { return note.noteOffVelocity.asUnsignedFloat() * maxNoteRadius; }
    float getPressureRadius() const noexcept { return note.pressure.asUnsignedFloat() * maxNoteRadius; }

    static constexpr auto maxNoteRadius = 100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteComponent)
};

//==============================================================================
class Visualiser : public juce::Component,
    public juce::MPEInstrument::Listener,
    private juce::AsyncUpdater
{
public:
    //==============================================================================
    Visualiser() {}

    //==============================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        auto noteDistance = float(getWidth()) / 128;

        for (auto i = 0; i < 128; ++i)
        {
            auto x = noteDistance * (float)i;
            auto noteHeight = int(juce::MidiMessage::isMidiNoteBlack(i) ? 0.7 * getHeight() : getHeight());
            g.setColour(juce::MidiMessage::isMidiNoteBlack(i) ? juce::Colours::white : juce::Colours::grey);
            g.drawLine(x, 0.0f, x, (float)noteHeight);

            if (i > 0 && i % 12 == 0)
            {
                g.setColour(juce::Colours::grey);
                auto octaveNumber = (i / 12) - 2;
                g.drawText("C" + juce::String(octaveNumber), (int)x - 15, getHeight() - 30, 30, 30, juce::Justification::centredBottom);
            }
        }
    }

    //==============================================================================
    void noteAdded(juce::MPENote newNote) override
    {
        const juce::ScopedLock sl(lock);
        activeNotes.add(newNote);
        triggerAsyncUpdate();
    }

    void notePressureChanged(juce::MPENote note) override { noteChanged(note); }
    void notePitchbendChanged(juce::MPENote note) override { noteChanged(note); }
    void noteTimbreChanged(juce::MPENote note) override { noteChanged(note); }
    void noteKeyStateChanged(juce::MPENote note) override { noteChanged(note); }

    void noteChanged(juce::MPENote changedNote)
    {
        const juce::ScopedLock sl(lock);

        for (auto& note : activeNotes)
            if (note.noteID == changedNote.noteID)
                note = changedNote;

        triggerAsyncUpdate();
    }

    void noteReleased(juce::MPENote finishedNote) override
    {
        const juce::ScopedLock sl(lock);

        for (auto i = activeNotes.size(); --i >= 0;)
            if (activeNotes.getReference(i).noteID == finishedNote.noteID)
                activeNotes.remove(i);

        triggerAsyncUpdate();
    }


private:
    //==============================================================================
    const juce::MPENote* findActiveNote(int noteID) const noexcept
    {
        for (auto& note : activeNotes)
            if (note.noteID == noteID)
                return &note;

        return nullptr;
    }

    NoteComponent* findNoteComponent(int noteID) const noexcept
    {
        for (auto& noteComp : noteComponents)
            if (noteComp->note.noteID == noteID)
                return noteComp;

        return nullptr;
    }

    //==============================================================================
    void handleAsyncUpdate() override
    {
        const juce::ScopedLock sl(lock);

        for (auto i = noteComponents.size(); --i >= 0;)
            if (findActiveNote(noteComponents.getUnchecked(i)->note.noteID) == nullptr)
                noteComponents.remove(i);

        for (auto& note : activeNotes)
            if (findNoteComponent(note.noteID) == nullptr)
                addAndMakeVisible(noteComponents.add(new NoteComponent(note)));

        for (auto& noteComp : noteComponents)
            if (auto* noteInfo = findActiveNote(noteComp->note.noteID))
                noteComp->update(*noteInfo, getCentrePositionForNote(*noteInfo));
    }

    //==============================================================================
    juce::Point<float> getCentrePositionForNote(juce::MPENote note) const
    {
        auto n = float(note.initialNote) + float(note.totalPitchbendInSemitones);
        auto x = (float)getWidth() * n / 128;
        auto y = (float)getHeight() * (1 - note.timbre.asUnsignedFloat());

        return { x, y };
    }

    //==============================================================================
    juce::OwnedArray<NoteComponent> noteComponents;
    juce::CriticalSection lock;
    juce::Array<juce::MPENote> activeNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Visualiser)
};

