# MidiPolySynth

- Moved wavetable to its own file, but trying to implement a queue of synthvoices for voice stealing is not working as I though.

- Using the juce filter just so I have one. Need to make one myself. 

- Slider mixing between square and saw.

- ADSR added possibly something off with Decay, possibly

- added sawtooth and smoother transitions between notes, but pushing in the very real case I ruin everything 

- Stopped trying to make my own ADSR and used the standard juce one, fixed issue with clearing notes when ADSR is not active, for tomorrow add ADSR controls. 

- Fixed little conversion errors

- Added "decay" even though thats the wrong word.

- Last repo destroyed due to the destruction of almost all the files, now organised into better .h files AND currently just a square wavetable that is set up to make 
sound.
