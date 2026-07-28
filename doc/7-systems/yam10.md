# YAM10

a fictional FM chip by rednoobmusic. it keeps the parts of classic FM that are pleasant to write for and drops the parts that are not.

synthesis is done the way hardware does it rather than in floating point: a quarter-wave log-sine ROM, a 256-entry exponential ROM, 10-bit attenuation and a fixed-point phase accumulator. that is what gives FM its particular grain, and it means an operator here behaves like one on a real part.

what it carries:

- 10 channels of 6 operators each.
- no algorithm list. each operator has a modulation input mask, so any operator may modulate any other, including itself.
- feedback on every operator rather than only the first.
- a delay on every operator, so one can come in after another.
- 21 waveforms grouped by family, three noise types, and custom wavetables of any length.
- ADSR with a second decay.
- per-operator panning, detune in both semitones and cents, and fixed pitch reaching down to a fraction of a hertz.
- a per-channel DSP chain: three filters in series, distortion, chorus, reverb, a three band compressor, an EQ, phase inversion and echo.

## operators

each of the six operators carries its own envelope, waveform, level, panning and tuning. an operator is a carrier when its output level is above zero, and a modulator otherwise; there is no algorithm to pick, only the routing matrix.

the modulation input is a set of six checkboxes. ticking the box for an operator's own number gives self-feedback in addition to the feedback slider.

### waveforms

the waveforms are grouped by family, so everything derived from one shape sits next to it.

| | |
| --- | --- |
| 0 to 6 | the sines: sine, half sine, absolute sine, pulse sine, squished sine, squished abssine, quarter squished sine |
| 7 to 11 | the triangles: triangle, absolute, half, squished, squished absolute |
| 12 to 16 | the squared sines: squared sine, absolute, half, squished, squished absolute |
| 17 to 18 | saw and half saw |
| 19 to 20 | square, and a logarithmic saw: an exponential fall either side of zero |
| 21 to 23 | noise, one bit noise and sample and hold |

the triangle is a real linear triangle.

the squared sine squares the magnitude and keeps the sign, so it is sin times the absolute value of sin rather than sin squared on its own. squaring alone would never go negative; this does. the absolute one in that family is the true sin squared. it is a rounder shape than the triangle: it peaks in the same place but sits lower either side of the peak, and at half amplitude the two are 0.11 apart.

both families carry the same five derivations, so either can be used as the starting point.

21 to 23 are generated as they play rather than read from a table, and all three are clocked from the operator's own phase, so they follow the note and any pitch or arpeggio macro:

- 21 is noise proper, taking many shift-register steps per cycle.
- 22 is the same but one bit deep, so it only ever sits at full scale either side of zero.
- 23 is sample and hold, taking exactly one step per cycle. it is still noise rather than a tone, but a much coarser one, and it makes a good modulator.

selecting the wavetable checkbox reads a wavetable instead. any length works; the chip reads it directly.

instruments written before the waveforms were grouped are moved across when they load, so they keep the shapes they were given.

### delay

an operator can be held silent for a while after the note starts, before its attack begins. the setting runs 0 to 7, where 0 is no delay and each step doubles the wait: 10 ms, 21, 41, 82, 165, 330, and 659 ms at the top. it is what lets one operator arrive after another from a single note.

### fixed pitch

an operator set to fixed pitch ignores the note and holds one frequency, given as a block and an F-num. the frequency is `F-num * 2^block / 8`, which reaches 0.125 Hz at block 0 and 16368 Hz at block 7. the low end is there so the noise waveforms can be given a slow step rate.

a frequency of zero holds the phase still.

### phase reset

the phase reset timer restarts an operator's phase every so many engine ticks, so it follows the song tempo. zero switches it off.

## DSP

each channel carries its own chain, in this order:

- three filters in series, each of which may be low pass, high pass, band pass or a notch, with a cutoff and a resonance.
- distortion: the lows are cut, the signal is driven hard into a clip, then brought back down by the output level.
- chorus, with a stereo width control that offsets the left and right modulation.
- reverb, with a send, a decay and a mix. early sets the level of the first reflections, the ones that tell you how big the room is, and size sets how far apart they fall. diffusion decides how much the tail is smeared, from separate echoes at the bottom to a single wash at the top. damping is how much treble the tail loses on every pass.
- a compressor working on three bands at once. two crossover points split the channel into low, mid and high, each band is squeezed on its own, and each gets its own level back afterwards. the ratios run both ways: anything above the threshold comes down by the down ratio and anything below is lifted by the up ratio, so 1:1 up leaves the quiet parts alone. attack and decay are in milliseconds. the level is followed as power rather than peak and the corner is rounded off over 12 dB, so it holds a line without sounding worked. a band that sits more than 15 dB under the threshold is left alone, so the leakage either side of a crossover is not lifted back into the mix.
- an EQ of up to eight bands. an instrument starts with none, and a band is placed where it is wanted. each one can be a peak, a low shelf, a high shelf, a low pass, a high pass or a notch, and carries its own frequency, gain and Q. a gain of 128 is flat and the sliders cover 12 dB either side of it, in steps of 0.15625 dB. the pass and notch shapes have no gain of their own. the editor draws the resulting curve and the bands are dragged around on it.
- phase inversion, either side independently. flipping one side widens the channel and cancels it on a mono mix.
- echo, with a mix, a delay in milliseconds and a feedback amount.

the frequency, gain and Q controls all read as real values rather than as raw bytes, so a cutoff is in hertz, an EQ gain is in decibels and a resonance is a Q. every part of the chain belongs to one channel, including the EQ and the phase inversion, so two channels can be treated completely differently.

## effects

per-operator effects take the operator in `x`, where `0` means every operator.

- `11xy`: **set feedback.**
  - `x` is the operator from 1 to 6, or `0` for all.
  - `y` is the feedback from 0 to 7.
- `12xx` to `17xx`: **set level of operators 1 to 6.** 0 is loudest, 7F quietest.
- `18xx`: **set level of all operators.**
- `19xx`: **set attack of all operators.**
- `1Axx` to `1Fxx`: **set attack of operators 1 to 6.**
- `20xx` to `25xx`: **set decay of operators 1 to 6.**
- `26xx`: **set decay of all operators.**
- `27xx` to `2Cxx`: **set decay 2 of operators 1 to 6.**
- `2Dxx`: **set decay 2 of all operators.**
- `2Exx` to `33xx`: **set detune of operators 1 to 6 in semitones.** 40 is the centre.
- `34xx`: **set fine detune of all operators in cents.** 40 is the centre.
- `50xy`: **set sustain level.**
- `51xy`: **set release.**
- `52xy`: **set multiplier.** the operator must be given; there is no "all" for this one. `y` is one digit, so this reaches 0 to 15; use the operator multiplier macro for 16.
- `53xy`: **set envelope scale**, from 0 to 3.
- `54xy`: **set key scale rate**, 0 or 1.
- `55xy`: **set waveform**, from 0 to F. `y` is one digit, so this effect reaches waveforms 0 to 15, which covers the sines, the triangles and the squared sines. use the operator waveform macro for 16 to 23.
  - only reaches the first sixteen waveforms, since `y` is one digit. use the waveform macro for 16 to 19.
- `60xx`: **set operator mask.** bits 0 to 5, one per operator. this is which operators sound, not which ones modulate.
- `7Cxy`: **set the delay before an operator attacks**, 0 to 7.
- `5Cxy`: **set waveform 16 to 23.** `y` is the waveform minus 16. `55xy` cannot reach these, since `y` is one digit.
- `61xx` to `66xx`: **set output level of operators 1 to 6.**
- `67xx`: **set output level of all operators.**
- `68xx` to `6Dxx`: **set panning of operators 1 to 6.** 80 is centre.
- `6Exx`: **set panning of all operators.**
- `70xx` to `75xx`: **set modulation input of operators 1 to 6.** bits 0 to 5, one per operator, and the bit matching the operator itself is self feedback.
- `76xx` to `7Bxx`: **set phase reset of operators 1 to 6**, in ticks. 0 switches it off.

the DSP chain:

- `44xx`, `46xx`, `48xx`: **set cutoff of filters 1, 2 and 3.**
- `45xx`, `47xx`, `49xx`: **set resonance of filters 1, 2 and 3.**
- `4Axy`: **set filter mode.**
  - `x` is the filter from 1 to 3.
  - `y` is `0` for low pass, `1` for high pass, `2` for band pass or `3` for a notch.
- `4Bxy`: **switch a filter on or off.**
  - `x` is the filter from 1 to 3.
  - `y` is `0` or `1`.
- `36xx`: **switch distortion on or off.** `01` on, `00` off.
- `4Cxx`: **set distortion gain.** this also switches distortion on.
- `4Dxx`: **set distortion level.**
- `35xx`: **set the cutoff of the high pass before the clipper.**
- `37xx`: **switch chorus on or off.** `01` on, `00` off.
- `4Exx`: **set chorus mix.** this also switches chorus on.
- `4Fxx`: **set chorus rate.**
- `B3xx`: **set chorus depth.**
- `38xx`: **set chorus feedback.**
- `39xx`: **set chorus width.**
- `B4xx`: **set echo mix.**
- `B5xx`: **set echo feedback.**
- `3Axx`: **set echo delay**, 4 ms a step.

the reverb:

- `BDxx`: **switch the reverb on or off.** `01` on, `00` off.
- `B6xx`: **set reverb mix.**
- `B7xx`: **set reverb send.**
- `B8xx`: **set reverb decay.**
- `B9xx`: **set reverb early reflections.**
- `BAxx`: **set reverb diffusion.**
- `BBxx`: **set reverb size.**
- `BCxx`: **set reverb damping.**

the compressor:

- `ABxx`: **switch the compressor on or off.** `01` on, `00` off.
- `A0xx`: **set threshold.**
- `A1xx`: **set the down ratio.**
- `A2xx`: **set the up ratio.** `00` leaves the quiet parts alone.
- `A3xx`: **set attack.**
- `A4xx`: **set decay.**
- `A5xx`: **set the low to mid crossover.**
- `A6xx`: **set the mid to high crossover.**
- `A7xx`, `A8xx`, `A9xx`: **set the low, mid and high band levels.** `80` is flat.
- `AAxx`: **set the output level.** `40` is unity.

the EQ. the bands are addressed one at a time: pick one with `AFxx`, then the effects after it apply to that band.

- `ACxx`: **switch the EQ on or off.** `01` on, `00` off.
- `AFxx`: **pick the band**, 0 to 7, that the effects below address.
- `56xx`: **set the picked band's frequency.**
- `57xx`: **set the picked band's gain.** `80` is flat.
- `58xx`: **set the picked band's Q.**
- `59xx`: **set the picked band's shape.** `0` peak, `1` low shelf, `2` high shelf, `3` low pass, `4` high pass, `5` notch.
- `5Axx`: **switch the picked band on or off.** switching one on also brings it into being if the instrument had fewer bands than that.

and the sides:

- `5Bxx`: **invert phase.** bit 0 is the left side, bit 1 the right, so `03` flips both.

## info

this chip uses the [YAM10](../4-instrument/yam10.md) instrument editor.

## chip config

no settings.
