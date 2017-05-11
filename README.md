# Guenther-Chiptune
This is a repository of schematics and firmware code for building the Guenther Chiptune: An Arduino controlled monophonic MIDI capable semi-modular analog synthesizer.

This synth has 4 VCOs (2 Audio, one Sub-audio and one hybrid that switches between either), a 4-pole low pass filter, a Voltage Controlled Amplifier (VCA) and two envelope generators (AR1 and AR2).  It also has an arpeggiator built in to add some videogame-esque sounds.  I never really sat down to figure out how many octaves it can work with before the tuning goes bonkers, but it appears to handle six octaves pretty well.

The VCOs, LPF and VCA cores are all built around the LM13700 OTA using mostly datasheet schematics with component values adjusted to work on +-9V supply.  Also, since the synth uses the LM13700 for all VCOs and VCF, all exponential current sources use PNP transistors.  I added in my own design for input CV signal mixing to the modules and output signal normalizers (to produce both CV and audio signals).  The CV generation via Arduino is not ideal because of the PWM noise.

The firmware code is messy.  I've done my best to add some documentation where possible in the code and added a logical data model to track functions and global variables.  It could use some cleaning up and the arpeggio generation is horribly inefficient, but I rushed the firmware together in a hazy week while the remainder of the project took the better part of a year.

Please enjoy!

PLEASE NOTE: 

- I've only tested the firmware on an Arduino Pro Mini with ATMega 328 chipset.  I started off using the ATMega 168 chipset, but the sketch became too large and began to brick my 168's.  It loads fine into the 328.

- The Schematics (KiCAD format) show in the CV Generation page, two methods for generating CVs.  V1.1 of the firmware only supports the Original 1V DAC.  The New 1V DAC in the schematic, while complete, has no firmware code behind it.  If building this, pleaes use the Original DAC or feel free to modify the sketch to support the new DAC.

- I did my best to cite schematics or pieces of schematics that were not my work from the sources, such as the Filter and Expo Converter.


