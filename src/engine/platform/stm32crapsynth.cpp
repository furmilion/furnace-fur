
/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2024 tildearrow and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "stm32crapsynth.h"
#include "../engine.h"
#include "furIcons.h"
#include <math.h>

//#define rWrite(a,v) pendingWrites[a]=v;
#define rWrite(a,v) { writes.push(QueuedWrite(a,v)); if (dumpWrites) {addWrite(a,v);} }

#define ad9833_write(ch,type,val) rWrite(((ch << 8) | (type)), val)

#define CHIP_FREQBASE 524288*16
#define CHIP_TIMERS_FREQBASE 524288*64
#define CHIP_DIVIDER 1

const char* regCheatSheetCrapSynth[]={
  ".", "0",
  NULL
};

const char** DivPlatformSTM32CRAPSYNTH::getRegisterSheet() {
  return regCheatSheetCrapSynth;
}

void DivPlatformSTM32CRAPSYNTH::acquire(short** buf, size_t len) {
  for (size_t h=0; h<len; h++) 
  {
    for(int i = 0; i < 4; i++)
    {
      crapsynth_clock(crap_synth);
    }
    
    for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) 
    {
      oscBuf[i]->data[oscBuf[i]->needle++]=CLAMP(crap_synth->chan_outputs[i],-32768,32767);
    }

    if (!writes.empty()) 
    {
      QueuedWrite w=writes.front();
      int ch = w.addr >> 8;
      int type = w.addr & 0xff;
      crapsynth_write(crap_synth, ch, type, w.val);
      if(ch < 5)
      {
        regPool[ch * 8 + type]=w.val;
      }
      if(ch == 5 || ch == 6 || ch == 7) //DAC
      {
        regPool[5 * 8 + (ch - 5) * 16 + type]=w.val;
      }
      if(ch > 7)
      {
        regPool[5 * 8 + 3 * 16 + (ch - 8) * 8 + type]=w.val;
      }
      
      writes.pop();
    }

    float dVbp = (w0_ceil_1 * Vhp);
    float dVlp = (w0_ceil_1 * Vbp);
    Vbp += dVbp;
    Vlp += dVlp;
    Vhp = (float)CLAMP(crap_synth->final_output,-32768,32767) - (Vbp * _1024_div_Q) - Vlp;

    buf[0][h]=Vhp;
  }
}

void DivPlatformSTM32CRAPSYNTH::updateWave(int ch) {
  if(ch > 4 && ch < 8)
  {
    if(chan[ch].wave == 6 || chan[ch].wave == 7) return;
    bool need_update = false;
    for (int i=0; i<STM32CRAPSYNTH_WAVETABLE_SIZE; i++) 
    {
      if(chan[ch].ws.output[i] != (int)crap_synth->dac[ch-5].wavetable[i])
      {
        need_update = true;
        break;
      }
    }
    if(!need_update) return;
    for (int i=0; i<STM32CRAPSYNTH_WAVETABLE_SIZE; i++) 
    {
      ad9833_write(ch, 10, ((uint32_t)chan[ch].ws.output[i] | (i << 8)));

      crap_synth->dac[ch - 5].wavetable[i] = chan[ch].ws.output[i];
    }
  }
}

void DivPlatformSTM32CRAPSYNTH::tick(bool sysTick) 
{
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) 
  {
    chan[i].std.next();

    bool update_duty = false;

    if (chan[i].std.vol.had) { //for ch3 PWM vol correction.... (faulty mux out line?)
      if(i < 8)
      {
        if(i != 7)
        {
          chan[i].outVol=VOL_SCALE_LOG(chan[i].vol&255,MIN(255,chan[i].std.vol.val),255);
        }
        else
        {
          chan[i].outVol=VOL_SCALE_LINEAR(chan[i].vol&255,MIN(255,chan[i].std.vol.val),255);
        }
        
        ad9833_write(i, 0, chan[i].outVol);
      }
    }
    
    if (NEW_ARP_STRAT) {
      chan[i].handleArp();
    } else if (chan[i].std.arp.had) {
      if (!chan[i].inPorta) {
        chan[i].baseFreq=NOTE_FREQUENCY(parent->calcArp(chan[i].note,chan[i].std.arp.val));
      }
      chan[i].freqChanged=true;
    }
    
    if (chan[i].std.pitch.had) {
      if (chan[i].std.pitch.mode) {
        chan[i].pitch2+=chan[i].std.pitch.val;
        CLAMP_VAR(chan[i].pitch2,-32768,32767);
      } else {
        chan[i].pitch2=chan[i].std.pitch.val;
      }
      chan[i].freqChanged=true;
    }

    if (chan[i].std.wave.had) {
      if(i < 4)
      {
        ad9833_write(i, 1, chan[i].std.wave.val);

        if((chan[i].std.wave.val == 5 && chan[i].wave != 5) || (chan[i].std.wave.val != 5 && chan[i].wave == 5))
        {
          chan[i].freqChanged = true;
        }

        chan[i].wave = chan[i].std.wave.val;
      }
      if(i == 5 || i == 6 || i == 7)
      {
        ad9833_write(i, 9, chan[i].std.wave.val);

        chan[i].freqChanged = true;

        chan[i].wave = chan[i].std.wave.val;
      }
    }

    if (chan[i].std.phaseReset.had) {
      if(i < 5 && chan[i].std.phaseReset.val)
      {
        ad9833_write(i, 3, chan[i].lfsr); //LFSR value is ignored for all chans except noise
      }
      if(i > 8) //phase reset for phase reset timers lmao
      {
        ad9833_write(i, 2, 0);
      }
    }

    if (chan[i].std.ex1.had) { //zero cross detection
      if(i < 7)
      {
        ad9833_write(i, 6, chan[i].std.ex1.val);
      }
    }

    if (chan[i].std.ex2.had) { //LFSR bits
      if(i == 4)
      {
        chan[i].lfsr = chan[i].std.ex2.val;
      }
    }

    if (chan[i].std.ex3.had) { //wavetable index
      if(i > 4 && i < 8)
      {
        chan[i].wavetable = chan[i].std.ex3.val;
        chan[i].ws.changeWave1(chan[i].wavetable);
        chan[i].updateWave = true;
      }
    }

    if (chan[i].std.ex4.had) { //noise/triangle amplitude
      if(i > 4 && i < 7 && (chan[i].wave > 1 && chan[i].wave < 6)) //PWM DAC doesn't have noise/tri gen
      {
        chan[i].noise_tri_amp = chan[i].std.ex4.val;
        ad9833_write(i, 11, chan[i].std.ex4.val);
        chan[i].freqChanged = true;
      }
    }

    if (chan[i].std.ex5.had) { //phase reset timers channel bitmask
      if((i >= 8 && i < 14))
      {
        ad9833_write(i, 0, chan[i].std.ex5.val);
      }
    }

    if (chan[i].std.ex6.had) { //noise clock internal/external
      if(i == 4)
      {
        ad9833_write(i, 1, chan[i].std.ex6.val);
        chan[i].extNoiseClk = !chan[i].std.ex6.val;
      }
    }
    
    if (chan[i].active && i > 4 && i < 8 && chan[i].do_wavetable && chan[i].wave == 1) {
      if (chan[i].ws.tick()) {
        //updateWave(i);
        chan[i].updateWave = true;
      }
    }

    if(chan[i].updateWave)
    {
      updateWave(i);
      chan[i].updateWave = false;
    }

    if(chan[i].apply_sample)
    {
      DivSample* s = parent->song.sample[chan[i].dacSample];

      unsigned int sample_offset = chan[i].sampleInRam ? (sampleOffRam[chan[i].dacSample]) : sampleOff[chan[i].dacSample];

      if(chan[i].set_sample_pos)
      {
        sample_offset += chan[i].sample_offset;
      }

      if(sample_offset >= (chan[i].sampleInRam ? (sampleOffRam[chan[i].dacSample]) : sampleOff[chan[i].dacSample]) + s->length8)
      {
        sample_offset = (chan[i].sampleInRam ? (sampleOffRam[chan[i].dacSample]) : sampleOff[chan[i].dacSample]) + s->length8 - 1;
      }

      if(chan[i].sampleInRam)
      {
        sample_offset |= 0x1000000;
      }

      ad9833_write(i, 2, sample_offset); //sample offset
      if(s->loop)
      {
        ad9833_write(i, 7, ((chan[i].sampleInRam ? (sampleOffRam[chan[i].dacSample]) : sampleOff[chan[i].dacSample]) + s->loopStart) | (chan[i].sampleInRam ? 0x1000000 : 0)); // loop point
      }
      else
      {
        //ad9833_write(i, 7, sampleOff[chan[i].dacSample] | (chan[i].sampleInRam ? 0x1000000 : 0)); // loop point = start
      }
      ad9833_write(i, 8, (s->length8) | (chan[i].sampleInRam ? 0x1000000 : 0)); //length

      if(crap_synth->dac[i - 5].playing)
      {
        ad9833_write(i, 1, 0); //stop playing sample
      }

      ad9833_write(i, 1, 1 | (chan[i].do_wavetable ? 2 : 0) | (s->loop ? 4 : 0) | (chan[i].sampleInRam ? 8 : 0)); //play sample

      chan[i].apply_sample = false;
      chan[i].set_sample_pos = false;
    }

    bool write_pwm_freq = false;

    if (chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff) {
      //DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_STM32CRAPSYNTH);
      if(chan[i].freqChanged && i < 4)
      {
        chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock/2,CHIP_FREQBASE);
        chan[i].timer_freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock/2,CHIP_TIMERS_FREQBASE);

        if(chan[i].freq > (1 << 28) - 1) chan[i].freq = (1 << 28) - 1;
        if(chan[i].timer_freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;
        
        if(chan[i].wave != 5)
        {
            ad9833_write(i, 2, chan[i].freq);
        }
        else
        {
            //ad9833_write(i, 4, chan[i].timer_freq);
            write_pwm_freq = true;
            update_duty = true;
        }
      }
      if(chan[i].freqChanged && i == 4) //&& chan[4].extNoiseClk)
      {
        chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock/2,CHIP_TIMERS_FREQBASE * 32);
        chan[i].timer_freq=chan[i].freq;

        while((double)chan[i].freq > 333300.0 * (double)(1 << 29) / (double)(chipClock/2)) //333333.(3) Hz max MM5437 clock freq
        {
            chan[i].freq = 333300.0 * (double)(1 << 29) / (double)(chipClock/2);
            chan[i].timer_freq=chan[i].freq;
        }

        //if(chan[i].freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;
        //if(chan[i].timer_freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;
        
        ad9833_write(i, 4, chan[i].timer_freq);
      }
      if(chan[i].freqChanged && (i == 5 || i == 6 || i == 7))
      {
        chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock/2,CHIP_TIMERS_FREQBASE * 32);
        chan[i].timer_freq=chan[i].freq;

        switch(chan[i].wave)
        {
          case 1:
          case 3:
          case 5:
          {
            if(chan[i].pcm)
            {
              double off=1.0;
              if (chan[i].dacSample>=0 && chan[i].dacSample<parent->song.sampleLen) {
                DivSample* s=parent->getSample(chan[i].dacSample);
                if (s->centerRate<1) {
                  off=1.0;
                } else {
                  off=8363.0/(double)s->centerRate;
                }
              }
              chan[i].timer_freq *= off * 2.0;
            }
            else
            {
              chan[i].timer_freq *= 8; //wavetable
            }
            break;
          }
          case 2: //triangle
          {
            chan[i].timer_freq *= (1 << chan[i].noise_tri_amp);
            chan[i].timer_freq /= 16;
            break;
          }
          case 4: //noise
          {
            chan[i].timer_freq *= 2;
            break;
          }
          case 6:
          case 7:
          {
            chan[i].timer_freq *= 8; //wavetable
            break;
          }
          default: break;
        }

        if(chan[i].freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;
        if(chan[i].timer_freq > (1 << 30) - 1) chan[i].timer_freq = (1 << 30) - 1;
        
        ad9833_write(i, 4, chan[i].timer_freq);
      }
      if(chan[i].freqChanged && i >= 8) //phase reset timers
      {
        chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock/2,CHIP_FREQBASE*4);
        chan[i].timer_freq=chan[i].freq;

        if(i == 8 + 1) //systick timer can't go below 4.29 Hz (72 MHz / 2^24)
        {
          if(chan[i].freq < (double)(1 << 29) * 4.29 / (double)(chipClock/2))
          {
            chan[i].freq = (double)(1 << 29) * 4.29 / (double)(chipClock/2);
            chan[i].timer_freq = chan[i].freq;
          }
        }

        if(i == 8 + 2) //USART1 can't go below 72 MHz / 12 / 0xfff7 = 91.565 Hz
        {
          if(chan[i].freq < (double)(1 << 29) * 91.565 / (double)(chipClock/2))
          {
            chan[i].freq = (double)(1 << 29) * 91.565 / (double)(chipClock/2);
            chan[i].timer_freq = chan[i].freq;
          }
        }

        if(i >= 8 + 2) //USART2, USART3, UART5 can't go below 36 MHz / 12 / 0xfff7 = 45.783 Hz
        {
          if(chan[i].freq < (double)(1 << 29) * 45.783 / (double)(chipClock/2))
          {
            chan[i].freq = (double)(1 << 29) * 45.783 / (double)(chipClock/2);
            chan[i].timer_freq = chan[i].freq;
          }
        }

        if(chan[i].freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;
        if(chan[i].timer_freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;

        ad9833_write(i, 1, chan[i].timer_freq);
      }
      if(chan[i].keyOff && i < 8)
      {
        ad9833_write(i, 0, 0);

        if(i > 4)
        {
          ad9833_write(i, 1, 0); //stop wave/sample playback
        }
      }
      if(chan[i].keyOff && i >= 8)
      {
        ad9833_write(i, 3, 0);
      }

      if (chan[i].keyOn) chan[i].keyOn=false;
      if (chan[i].keyOff) chan[i].keyOff=false;
      chan[i].freqChanged=false;
    }

    if(write_pwm_freq)
    {
      ad9833_write(i, 4, chan[i].timer_freq);
      write_pwm_freq = false;
    }

    if (chan[i].std.duty.had) { //duty after freq for export proper duty to compare count register conversion
      if(i < 4 || i == 5 || i == 6 || i == 7)
      {
        chan[i].duty = chan[i].std.duty.val;
        if(i < 4)
        {
          ad9833_write(i, 5, chan[i].std.duty.val);
          update_duty = false;
        }
        if((i == 5 || i == 6 || i == 7) && chan[i].wave == 6)
        {
          ad9833_write(i, 9, ((uint32_t)chan[i].wave | (((uint32_t)chan[i].duty >> 8) << 4)));
        }
      }
    }

    if(update_duty && i < 4)
    {
      ad9833_write(i, 5, chan[i].std.duty.val);
      update_duty = false;
    }
  }
}

int DivPlatformSTM32CRAPSYNTH::dispatch(DivCommand c) {
  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_STM32CRAPSYNTH);
      if (c.value!=DIV_NOTE_NULL) {
        chan[c.chan].baseFreq=NOTE_FREQUENCY(c.value);
        chan[c.chan].freqChanged=true;
        chan[c.chan].note=c.value;
      }
      chan[c.chan].active=true;
      chan[c.chan].keyOn=true;
      chan[c.chan].macroInit(ins);
      if (!parent->song.brokenOutVol && !chan[c.chan].std.vol.will) {
        chan[c.chan].outVol=chan[c.chan].vol;
      }
      if (chan[c.chan].wavetable<0) {
        chan[c.chan].wavetable=0;
        chan[c.chan].ws.changeWave1(chan[c.chan].wavetable);
      }
      
      chan[c.chan].insChanged=false;

      if (!ins->amiga.useSample && c.chan > 4 && c.chan < 8) {
        chan[c.chan].pcm = false;
        chan[c.chan].do_wavetable = true;
        chan[c.chan].ws.init(ins,256,256,chan[c.chan].insChanged);
        chan[c.chan].updateWave = true;
        chan[c.chan].freqChanged = true;
        ad9833_write(c.chan, 1, 1 | (chan[c.chan].do_wavetable ? 2 : 0)); //play wave
      }
      if (ins->amiga.useSample && c.chan > 4 && c.chan < 8) {
        chan[c.chan].pcm = true;
        chan[c.chan].do_wavetable = false;
        if (c.value!=DIV_NOTE_NULL) {
          chan[c.chan].dacSample=ins->amiga.getSample(c.value);
          chan[c.chan].sampleNote=c.value;
          c.value=ins->amiga.getFreq(c.value);
          chan[c.chan].sampleNoteDelta=c.value-chan[c.chan].sampleNote;
          chan[c.chan].baseFreq = NOTE_FREQUENCY(c.value);
          chan[c.chan].freqChanged = true;
          chan[c.chan].note = c.value; //hack but works?
        } else if (chan[c.chan].sampleNote!=DIV_NOTE_NULL) {
          chan[c.chan].dacSample=ins->amiga.getSample(chan[c.chan].sampleNote);
          c.value=ins->amiga.getFreq(chan[c.chan].sampleNote);
        }
        if (chan[c.chan].dacSample<0 || chan[c.chan].dacSample>=parent->song.sampleLen) {
          chan[c.chan].dacSample=-1;
          //if (dumpWrites) addWrite(0xffff0002+(c.chan<<8),0);
          break;
        } else {
          //if (dumpWrites) {
            //addWrite(0xffff0000+(c.chan<<8),chan[c.chan].dacSample);
          //}
          chan[c.chan].sampleInRam = false;

          if(chan[c.chan].dacSample < parent->song.sampleLen && chan[c.chan].dacSample >= 0)
          {
            if(!sampleLoaded[0][chan[c.chan].dacSample] && !sampleLoaded[1][chan[c.chan].dacSample]) break;

            if(sampleLoaded[0][chan[c.chan].dacSample])
            {
              chan[c.chan].sampleInRam = false;
            }
            if(sampleLoaded[1][chan[c.chan].dacSample])
            {
              chan[c.chan].sampleInRam = true;
            }
            
            chan[c.chan].apply_sample = true;
          }
        }
        chan[c.chan].dacPos=0;
        chan[c.chan].dacPeriod=0;
      }

      if(c.chan >= 8) //phase reset timers
      {
        ad9833_write(c.chan, 3, 1);
      }
      /*else
      {
        chan[c.chan].pcm = false;
      }*/
      break;
    }
    case DIV_CMD_NOTE_OFF:
      chan[c.chan].dacSample=-1;
      //if (dumpWrites) addWrite(0xffff0002+(c.chan<<8),0);
      chan[c.chan].pcm=false;
      chan[c.chan].active=false;
      chan[c.chan].keyOff=true;
      chan[c.chan].macroInit(NULL);
      break;
    case DIV_CMD_NOTE_OFF_ENV:
    case DIV_CMD_ENV_RELEASE:
      chan[c.chan].std.release();
      if(c.chan >= 8)
      {
        chan[c.chan].keyOff=true;
      }
      break;
    case DIV_CMD_INSTRUMENT:
      if (chan[c.chan].ins!=c.value || c.value2==1) {
        chan[c.chan].ins=c.value;
        chan[c.chan].insChanged=true;
      }
      break;
    case DIV_CMD_VOLUME:
      if (chan[c.chan].vol!=c.value) {
        chan[c.chan].vol=c.value;
        if (!chan[c.chan].std.vol.has) {
          chan[c.chan].outVol=c.value;
          chan[c.chan].vol=chan[c.chan].outVol;
        }

        if(c.chan < 4)
        {
            ad9833_write(c.chan, 0, (VOL_SCALE_LOG(crap_synth->ad9833[c.chan].zero_cross ? crap_synth->ad9833[c.chan].pending_vol : crap_synth->volume[c.chan],chan[c.chan].outVol&255,255)));
        }
      }
      break;
    case DIV_CMD_GET_VOLUME:
      if (chan[c.chan].std.vol.has) {
        return chan[c.chan].vol;
      }
      return chan[c.chan].outVol;
      break;
    case DIV_CMD_PITCH:
      chan[c.chan].pitch=c.value;
      chan[c.chan].freqChanged=true;
      break;
    case DIV_CMD_WAVE:
      chan[c.chan].wave=c.value;
      /*if(c.chan > 4 && c.chan < 7 && chan[c.chan].do_wavetable)
      {
        chan[c.chan].ws.changeWave1(chan[c.chan].wavetable);
      }*/
      
      if(c.chan < 4 || (c.chan > 4 && c.chan < 8))
      {
        ad9833_write(c.chan, 1, c.value);
      }
      
      break;
    case DIV_CMD_NOTE_PORTA: {
      int destFreq=NOTE_PERIODIC(c.value2+chan[c.chan].sampleNoteDelta);
      bool return2=false;
      if (destFreq>chan[c.chan].baseFreq) {
        chan[c.chan].baseFreq+=c.value;
        if (chan[c.chan].baseFreq>=destFreq) {
          chan[c.chan].baseFreq=destFreq;
          return2=true;
        }
      } else {
        chan[c.chan].baseFreq-=c.value;
        if (chan[c.chan].baseFreq<=destFreq) {
          chan[c.chan].baseFreq=destFreq;
          return2=true;
        }
      }
      chan[c.chan].freqChanged=true;
      if (return2) {
        chan[c.chan].inPorta=false;
        return 2;
      }
      break;
    }
    case DIV_CMD_LEGATO:
      chan[c.chan].baseFreq=NOTE_PERIODIC(c.value+chan[c.chan].sampleNoteDelta+((HACKY_LEGATO_MESS)?(chan[c.chan].std.arp.val):(0)));
      chan[c.chan].freqChanged=true;
      chan[c.chan].note=c.value;
      break;
    case DIV_CMD_PRE_PORTA:
      if (chan[c.chan].active && c.value2) {
        if (parent->song.resetMacroOnPorta) chan[c.chan].macroInit(parent->getIns(chan[c.chan].ins,DIV_INS_PCE));
      }
      if (!chan[c.chan].inPorta && c.value && !parent->song.brokenPortaArp && chan[c.chan].std.arp.will && !NEW_ARP_STRAT) chan[c.chan].baseFreq=NOTE_PERIODIC(chan[c.chan].note);
      chan[c.chan].inPorta=c.value;
      break;
    case DIV_CMD_SAMPLE_POS:
      if (chan[c.chan].do_wavetable) break;
      if(c.chan > 6 || c.chan < 4) break;
      chan[c.chan].sample_offset=c.value;
      chan[c.chan].set_sample_pos = true;
      break;
    case DIV_CMD_GET_VOLMAX:
      return 255;
      break;
    case DIV_CMD_MACRO_OFF:
      chan[c.chan].std.mask(c.value,true);
      break;
    case DIV_CMD_MACRO_ON:
      chan[c.chan].std.mask(c.value,false);
      break;
    case DIV_CMD_MACRO_RESTART:
      chan[c.chan].std.restart(c.value);
      break;
    default:
      break;
  }
  return 1;
}

void DivPlatformSTM32CRAPSYNTH::muteChannel(int ch, bool mute) {
  isMuted[ch]=mute;
  crapsynth_set_is_muted(crap_synth, ch, mute);
}

void DivPlatformSTM32CRAPSYNTH::forceIns() {
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    chan[i].insChanged=true;
    chan[i].freqChanged=true;
    //updateWave(i);
    if(chan[i].wave == 1) chan[i].updateWave = true;
    //chWrite(i,0x05,isMuted[i]?0:chan[i].pan);
  }
}

void* DivPlatformSTM32CRAPSYNTH::getChanState(int ch) {
  return &chan[ch];
}

DivMacroInt* DivPlatformSTM32CRAPSYNTH::getChanMacroInt(int ch) {
  return &chan[ch].std;
}

void DivPlatformSTM32CRAPSYNTH::getPaired(int ch, std::vector<DivChannelPair>& ret) {
  /*if (ch==1 && lfoMode>0) {
    ret.push_back(DivChannelPair(_("mod"),0));
  }*/
}

DivChannelModeHints DivPlatformSTM32CRAPSYNTH::getModeHints(int ch) {
  DivChannelModeHints ret;
  if (ch<4) return ret;
  ret.count=1;
  ret.hint[0]=ICON_FUR_NOISE;
  ret.type[0]=0;
  
  return ret;
}

DivSamplePos DivPlatformSTM32CRAPSYNTH::getSamplePos(int ch) {
  if (ch < 5 || ch > 8) return DivSamplePos();
  if (!chan[ch].pcm) return DivSamplePos();

  int channel = ch - 5;

  if(chan[ch].dacSample < 0 || chan[ch].dacSample > parent->song.sampleLen) return DivSamplePos();

  //DivSample* s = parent->getSample(chan[ch].dacSample);

  uint32_t start_addr = (chan[ch].sampleInRam ? (sampleOffRam[chan[ch].dacSample]) : sampleOff[chan[ch].dacSample]);

  return DivSamplePos(
    chan[ch].dacSample,
    crap_synth->dac[channel].curr_pos - start_addr,
    (int)(double)(chipClock / 2) * (double)crap_synth->dac[channel].timer_freq / (double)(1 << 29)
  );

  //return DivSamplePos();
}

DivDispatchOscBuffer* DivPlatformSTM32CRAPSYNTH::getOscBuffer(int ch) {
  return ch > 7 ? NULL : oscBuf[ch];
}

int DivPlatformSTM32CRAPSYNTH::mapVelocity(int ch, float vel) {
  return round(31.0*pow(vel,0.22));
}

float DivPlatformSTM32CRAPSYNTH::getGain(int ch, int vol) {
  if (vol==0) return 0;
  return 1.0/pow(10.0,(float)(31-vol)*3.0/20.0);
}

unsigned char* DivPlatformSTM32CRAPSYNTH::getRegisterPool() {
  return (unsigned char*)regPool;
}

int DivPlatformSTM32CRAPSYNTH::getRegisterPoolSize() {
  return 8*11+8*6;
}

int DivPlatformSTM32CRAPSYNTH::getRegisterPoolDepth()
{
    return 32; // some of the registers of STM32 timers are 32-bit. or 24 bit. AD9833 phase acc/frequency is 28 bits...
}

void DivPlatformSTM32CRAPSYNTH::reset() {
  writes.clear();
  memset(regPool,0,(8*11 + 8 * 6)*sizeof(unsigned int));
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    chan[i]=DivPlatformSTM32CRAPSYNTH::Channel();
    chan[i].std.setEngine(parent);
    chan[i].ws.setEngine(parent);
    chan[i].ws.init(NULL,256,256,false);

    chan[i].duty = 0x7fff;
    chan[i].wave = 0;
    chan[i].noise_tri_amp = 11;
    chan[i].do_wavetable = false;
    chan[i].updateWave = false;
    chan[i].extNoiseClk = true;

    chan[i].apply_sample = false;
    chan[i].set_sample_pos = false;
    chan[i].sample_offset = 0;
  }
  if (dumpWrites) 
  {
    addWrite(0xffffffff,0);
  }
  crapsynth_reset(crap_synth);
  writeOscBuf = 0;

  Vhp = 0;
  Vbp = 0;
  Vlp = 0;

  _1024_div_Q = (1.0/(0.707));
  w0_ceil_1 = (2.0*3.1415*(float)5) / (float)rate;
}

int DivPlatformSTM32CRAPSYNTH::getOutputCount() {
  return 1;
}

bool DivPlatformSTM32CRAPSYNTH::keyOffAffectsArp(int ch) {
  return false;
}

void DivPlatformSTM32CRAPSYNTH::notifyWaveChange(int wave) {
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    if (chan[i].wavetable==wave && i > 4 && i < 8) {
      chan[i].ws.changeWave1(wave);
      //updateWave(i);
      chan[i].updateWave = true;
    }
  }
}

void DivPlatformSTM32CRAPSYNTH::notifyInsDeletion(void* ins) {
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    chan[i].std.notifyInsDeletion((DivInstrument*)ins);
  }
}

const void* DivPlatformSTM32CRAPSYNTH::getSampleMem(int index) {
  if(index > 1) return NULL;
  return index == 0 ? (unsigned char*)crap_synth->sample_mem_flash : (unsigned char*)crap_synth->sample_mem_ram;
}

size_t DivPlatformSTM32CRAPSYNTH::getSampleMemCapacity(int index) {
  if(index > 1) return 0;
  return index == 0 ? STM32CRAPSYNTH_FLASH_SAMPLE_MEM_SIZE : STM32CRAPSYNTH_RAM_SAMPLE_MEM_SIZE;
}

size_t DivPlatformSTM32CRAPSYNTH::getSampleMemUsage(int index) {
  if(index > 1) return 0;
  return index == 0 ? sampleMemLenFlash : sampleMemLenRam;
}

bool DivPlatformSTM32CRAPSYNTH::isSampleLoaded(int index, int sample) {
  if (index<0 || index>1) return false;
  if (sample<0 || sample>255) return false;
  return sampleLoaded[index][sample];
}

const DivMemoryComposition* DivPlatformSTM32CRAPSYNTH::getMemCompo(int index) {
  if (index>1) return NULL;
  return index == 0 ? &sampleMemFlashCompo : &sampleMemRamCompo;
}

void DivPlatformSTM32CRAPSYNTH::renderSamples(int sysID) {
  size_t maxPos=getSampleMemCapacity(0);

  memset(crap_synth->sample_mem_flash,0,maxPos);
  memset(sampleOff,0,256*sizeof(unsigned int));
  memset(sampleOffRam,0,256*sizeof(unsigned int));
  memset(sampleLoaded,0,256*2*sizeof(bool));

  //sample flash memory

  sampleMemFlashCompo=DivMemoryComposition();
  sampleMemFlashCompo.name=_("Sample memory (Flash)");
  //sampleMemFlashCompo.memory = (unsigned char*)crap_synth->sample_mem_flash;

  size_t memPos=0;
  for (int i=0; i<parent->song.sampleLen; i++) 
  {
    DivSample* s=parent->song.sample[i];
    if (!s->renderOn[0][sysID]) 
    {
      sampleOff[i]=0;
      continue;
    }
    int length=s->length8;
    int actualLength=MIN((int)(maxPos-memPos),length);
    if (actualLength>0) 
    {
      sampleOff[i]=memPos;
      //memcpy(&crap_synth->sample_mem_flash[memPos],s->data8,actualLength);
      for(int ssyka = 0; ssyka < actualLength; ssyka++)
      {
        crap_synth->sample_mem_flash[memPos + ssyka] = (uint8_t)s->data8[ssyka] + 0x80;
      }
      memPos+=actualLength;
      sampleMemFlashCompo.entries.push_back(DivMemoryEntry(DIV_MEMORY_SAMPLE,"PCM",i,sampleOff[i],memPos));
    }
    if (actualLength<length)
    {
      logW("out of STM32CrapSynth flash sample memory for sample %d!",i);
      break;
    }
    sampleLoaded[0][i]=true;
  }
  sampleMemLenFlash=memPos;
  sampleMemFlashCompo.used=sampleMemLenFlash;
  sampleMemFlashCompo.capacity=maxPos;

  //sample RAM

  maxPos=getSampleMemCapacity(1);
  memset(crap_synth->sample_mem_ram,0,maxPos);

  sampleMemRamCompo=DivMemoryComposition();
  sampleMemRamCompo.name=_("Sample memory (RAM)");
  //sampleMemRamCompo.memory = (unsigned char*)crap_synth->sample_mem_ram;

  memPos=0;
  for (int i=0; i<parent->song.sampleLen; i++) 
  {
    DivSample* s=parent->song.sample[i];
    if (!s->renderOn[1][sysID]) 
    {
      sampleOffRam[i]=0;
      continue;
    }
    int length=s->length8;
    int actualLength=MIN((int)(maxPos-memPos),length);
    if (actualLength>0) 
    {
      sampleOffRam[i]=memPos;
      //memcpy(&crap_synth->sample_mem_flash[memPos],s->data8,actualLength);
      for(int ssyka = 0; ssyka < actualLength; ssyka++)
      {
        crap_synth->sample_mem_ram[memPos + ssyka] = (uint8_t)s->data8[ssyka] + 0x80;
      }
      memPos+=actualLength;
      sampleMemRamCompo.entries.push_back(DivMemoryEntry(DIV_MEMORY_SAMPLE,"PCM",i,sampleOffRam[i],memPos));
    }
    if (actualLength<length) 
    {
      logW("out of STM32CrapSynth RAM sample memory for sample %d!",i);
      break;
    }
    sampleLoaded[1][i]=true;
  }
  sampleMemLenRam=memPos;
  sampleMemRamCompo.used=sampleMemLenRam;
  sampleMemRamCompo.capacity=maxPos;
}

void DivPlatformSTM32CRAPSYNTH::setFlags(const DivConfig& flags) {
  chipClock=2500000;
  CHECK_CUSTOM_CLOCK;
  rate=chipClock/10; // 250 kHz
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    oscBuf[i]->rate=rate;
  }
}

void DivPlatformSTM32CRAPSYNTH::poke(unsigned int addr, unsigned short val) {
  rWrite(addr,val);
}

void DivPlatformSTM32CRAPSYNTH::poke(std::vector<DivRegWrite>& wlist) {
  for (DivRegWrite& i: wlist) rWrite(i.addr,i.val);
}

int DivPlatformSTM32CRAPSYNTH::init(DivEngine* p, int channels, int sugRate, const DivConfig& flags) {
  parent=p;
  dumpWrites=false;
  skipRegisterWrites=false;
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    isMuted[i]=false;
    oscBuf[i]=new DivDispatchOscBuffer;
  }
  crap_synth = crapsynth_create();

  sampleMemFlashCompo=DivMemoryComposition();
  sampleMemFlashCompo.name=_("Sample memory (Flash)");
  sampleMemFlashCompo.capacity=STM32CRAPSYNTH_FLASH_SAMPLE_MEM_SIZE;
  //sampleMemFlashCompo.memory=(unsigned char*)crap_synth->sample_mem_flash;

  sampleMemRamCompo=DivMemoryComposition();
  sampleMemRamCompo.name=_("Sample memory (RAM)");
  sampleMemRamCompo.capacity=STM32CRAPSYNTH_RAM_SAMPLE_MEM_SIZE;
  //sampleMemRamCompo.memory=(unsigned char*)crap_synth->sample_mem_ram;

  setFlags(flags);
  reset();
  return STM32CRAPSYNTH_NUM_CHANNELS;
}

void DivPlatformSTM32CRAPSYNTH::quit() {
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    delete oscBuf[i];
  }

  if (crap_synth!=NULL) {
    crapsynth_free(crap_synth);
  }
}

DivPlatformSTM32CRAPSYNTH::~DivPlatformSTM32CRAPSYNTH() {
}
