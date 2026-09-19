# GP (Toshiba TC24SC201AF-002)

the GP is the PCM chip Roland used in the SC-55, SC-55mkII, CM-300 and SCC-1. it is the part that actually makes the sound in a Sound Canvas, and this is that chip on its own, without the rest of the module around it. the emulation comes from [Nuked-SC55](https://github.com/nukeykt/Nuked-SC55), which was written from a decap of the die.

what you get is 28 sample voices with 14-bit pitch resolution and stereo panning. samples are stored in the chip's own delta format, so they pick up its grain.

there is no MIDI, no GS sound set and no firmware here. you supply the samples.

## info

this chip uses the [Generic Sample](../4-instrument/sample.md) instrument editor.

a sample plays at its own rate four octaves above the base note, the same as Furnace's other sample chips. a voice steps through its sample at most four bytes per pass, so you get two more octaves above that before the pitch stops rising.

ping pong loops run on the chip rather than by unrolling the sample, so a sample set to ping pong costs no extra memory. backward loops are not supported and play forwards.

## samples

samples are converted to the chip's own format, called FCE-DPCM. each sample byte is a signed delta, and every block of 16 samples carries a 4-bit shift code that scales those deltas. it is lossy, and how lossy depends on the material: steep transients force a coarse shift code and pick up grain, while quiet passages stay clean. this is the same format the factory wave ROMs use.

pick **FCE-DPCM** in the sample editor to hear what the chip will actually play.

wave memory is 8MB, split into eight 1MB banks. the first 32KB of each bank holds that bank's shift codes, the same way a real wave ROM is laid out, so a little under 8MB is usable. a sample cannot straddle a bank boundary, because a voice's address counter is only 20 bits wide, which also caps a single sample at just under 1MB.

looped samples get a small correction on the last block of the loop. the chip never resets its accumulator when playback jumps back, so a loop that does not end where it started would walk further from zero on every pass. samples that do not loop get a short ramp back to zero and then sit on a silent block.

## chip config

the following options are available in the Chip Manager window:

- **Model**: picks between the SC-55mkII and SC-55ST at 24MHz, and the original SC-55, CM-300 and SCC-1 at 23.2MHz. this sets the rate everything plays back at.
- **Oversampling**: the chip emits two frames per pass instead of one. turning it off halves the output rate.

## what is left out

the chip has a resonant filter on every voice, and a reverb and chorus unit driven from the same register file. neither is used here.

the filter is a two pole state variable design. one coefficient sets the frequency and a second sets the damping, and at low damping it self oscillates. the values that keep it in range are the ones the SC-55 firmware supplies. the reverb and chorus are the same story: the delay line addresses and coefficients live in the firmware rather than in the chip, so without them the effects section is either silent or runs away. both are bypassed.

the emulation core still contains all of it, so either could be switched back on if the firmware tables are recovered.

## levels

the chip sums its voices into a 20-bit accumulator that wraps rather than clips, which on hardware means a loud enough passage tears instead of limiting. each voice is therefore scaled so that all 28 at once still fit, and the gain is taken back on the way out. the result is that the output stays clean however many voices are playing, and one voice on its own sits at roughly the same level as Furnace's other sample chips.
