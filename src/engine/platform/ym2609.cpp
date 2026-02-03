/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2026 tildearrow and contributors
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

#include "ym2609.h"
#include "../engine.h"
#include <math.h>
#include "../../ta-log.h"

#define rWrite(a,v) if (!skipRegisterWrites) {writes.push(QueuedWrite(a,v)); if (dumpWrites) {addWrite(a,v);} }

#define CHIP_FREQBASE 524288*64
#define CHIP_DIVIDER 1

//TODO: replace with custom clock

#define YM2609_CLOCK 8000000
#define YM2609_DSP_RATE 44100

const char** DivPlatformYM2609::getRegisterSheet() {
  return NULL;
}

void DivPlatformYM2609::acquire(short** buf, size_t len) 
{
    for (int i=0; i<YM2609_NUM_CHANNELS; i++) 
    {
      oscBuf[i]->begin(len);
    }

    for(size_t samp = 0; samp < len; samp++)
    {
      output_buf[0][0] = 0;
      output_buf[1][0] = 0;

      ym2609->Mix(output_buf, 1);

      buf[0][len] = output_buf[0][0];
      buf[1][len] = output_buf[1][0];

      if (!writes.empty()) 
      {
        QueuedWrite w=writes.front();
        //sid3_write(sid3, w.addr, w.val);
        regPool[w.addr % YM2609_NUM_REGISTERS]=w.val;
        writes.pop();
      }
    }

    for (int i=0; i<YM2609_NUM_CHANNELS; i++) 
    {
      oscBuf[i]->end(len);
    }
}

void DivPlatformYM2609::tick(bool sysTick) 
{
  bool doUpdateWave = false;

  for (int i=0; i<YM2609_NUM_CHANNELS; i++) 
  {
    chan[i].std.next();

    DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_SID3);
    
    if(sysTick)
    {
      
    }

    if (chan[i].std.vol.had) 
    {
      //chan[i].outVol=VOL_SCALE_LINEAR(chan[i].vol&255,MIN(255,chan[i].std.vol.val),255);
      //rWrite(13 + i * SID3_REGISTERS_PER_CHANNEL, chan[i].outVol);
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
        CLAMP_VAR(chan[i].pitch2,-65535,65535);
      } else {
        chan[i].pitch2=chan[i].std.pitch.val;
      }
      chan[i].freqChanged=true;
    }
    

    if (chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff) 
    {
      chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock,CHIP_FREQBASE);

      if (chan[i].keyOn) 
      {
        DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_SID3);
        
        chan[i].gate = true;
      }
      if (chan[i].keyOff) 
      {
        chan[i].gate = false;
      }

      //if (chan[i].freq<0) chan[i].freq=0;
      //if (chan[i].freq>0xffffff) chan[i].freq=0xffffff;

      //updateFreq(i);
      
      

      /*if (chan[i].pcm && i == YM2609_NUM_CHANNELS - 1) {
        double off=1.0;
        if (chan[i].dacSample>=0 && chan[i].dacSample<parent->song.sampleLen) {
          DivSample* s=parent->getSample(chan[i].dacSample);
          if (s->centerRate<1) {
            off=1.0;
          } else {
            off=(double)s->centerRate/parent->getCenterRate();
          }
        }
        chan[i].dacRate=chan[i].freq*(off / 32.0)*(double)chipClock/1000000.0;
      }

      chan[i].noiseFreqChanged = true;

      if(chan[i].independentNoiseFreq)
      {
        chan[i].noise_pitch2 = chan[i].pitch2;
      }*/

      if (chan[i].keyOn) chan[i].keyOn=false;
      if (chan[i].keyOff) chan[i].keyOff=false;
      chan[i].freqChanged=false;
    }
  }

  /*if (chan[YM2609_NUM_CHANNELS - 1].active && !chan[YM2609_NUM_CHANNELS - 1].pcm) 
  {
    if (ws.tick()) 
    {
      doUpdateWave = true;
    }
  }

  if(doUpdateWave)
  {
    updateWave();
    doUpdateWave = false;
  }*/
}

int DivPlatformYM2609::dispatch(DivCommand c) {
  if (c.chan>YM2609_NUM_CHANNELS - 1) return 0;

  //bool updEnv = false;
  DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_SID3);
  //int filter = 0;

  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_SID3);
      if (c.value!=DIV_NOTE_NULL) {
        chan[c.chan].baseFreq=NOTE_FREQUENCY(c.value);
        chan[c.chan].freqChanged=true;
        chan[c.chan].note=c.value;
      }
      chan[c.chan].active=true;
      chan[c.chan].keyOn=true;

      if (chan[c.chan].insChanged) 
      {
        chan[c.chan].insChanged=false;
      }
      chan[c.chan].macroInit(ins);
      break;
    }
    case DIV_CMD_NOTE_OFF:
      chan[c.chan].active=false;
      chan[c.chan].keyOff=true;
      chan[c.chan].keyOn=false;
      //chan[c.chan].macroInit(NULL);
      break;
    case DIV_CMD_NOTE_OFF_ENV:
      chan[c.chan].active=false;
      chan[c.chan].keyOff=true;
      chan[c.chan].keyOn=false;
      chan[c.chan].std.release();
      break;
    case DIV_CMD_ENV_RELEASE:
      chan[c.chan].std.release();
      break;
    case DIV_CMD_INSTRUMENT:
      if (chan[c.chan].ins!=c.value || c.value2==1) {
        chan[c.chan].insChanged=true;
        chan[c.chan].ins=c.value;
      }
      break;
    case DIV_CMD_VOLUME:
      if (chan[c.chan].vol!=c.value) {
        chan[c.chan].vol=c.value;
        if (!chan[c.chan].std.vol.has) {
          //chan[c.chan].outVol=c.value;
          //chan[c.chan].vol=chan[c.chan].outVol;
          //rWrite(SID3_REGISTER_ADSR_VOL + c.chan * SID3_REGISTERS_PER_CHANNEL, chan[c.chan].vol);
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
    case DIV_CMD_NOTE_PORTA: {
      int destFreq=NOTE_FREQUENCY(c.value2);
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
      chan[c.chan].baseFreq=NOTE_FREQUENCY(c.value+((HACKY_LEGATO_MESS)?(chan[c.chan].std.arp.val):(0)));
      chan[c.chan].freqChanged=true;
      chan[c.chan].note=c.value;
      break;
    case DIV_CMD_PRE_PORTA:
      if (chan[c.chan].active && c.value2) {
        if (parent->song.compatFlags.resetMacroOnPorta || parent->song.compatFlags.preNoteNoEffect) {
          chan[c.chan].macroInit(parent->getIns(chan[c.chan].ins,DIV_INS_SID3));
          chan[c.chan].keyOn=true;
        }
      }
      if (!chan[c.chan].inPorta && c.value && !parent->song.compatFlags.brokenPortaArp && chan[c.chan].std.arp.will && !NEW_ARP_STRAT) chan[c.chan].baseFreq=NOTE_FREQUENCY(chan[c.chan].note);
      chan[c.chan].inPorta=c.value;
      break;
    case DIV_CMD_GET_VOLMAX:
      return 0xff; //TODO: do properly
      break;
    case DIV_CMD_WAVE:
      
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

void DivPlatformYM2609::muteChannel(int ch, bool mute) {
  isMuted[ch]=mute;
  //sid3_set_is_muted(sid3,ch,mute);
}

void DivPlatformYM2609::forceIns() {
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) {
    chan[i].insChanged=true;
    if (chan[i].active) {
      chan[i].keyOn=true;
      chan[i].freqChanged=true;
    }
    //updateFilter(i);
  }
}

void DivPlatformYM2609::notifyInsChange(int ins) {
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) {
    if (chan[i].ins==ins) {
      chan[i].insChanged=true;
    }
  }
}

void DivPlatformYM2609::notifyWaveChange(int wave) 
{
  /*if (chan[YM2609_NUM_CHANNELS - 1].wavetable == wave)
  {
    ws.changeWave1(wave, false);
    updateWave();
  }*/
}

void DivPlatformYM2609::notifyInsDeletion(void* ins) {
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) {
    chan[i].std.notifyInsDeletion((DivInstrument*)ins);
  }
}

void* DivPlatformYM2609::getChanState(int ch) {
  return &chan[ch];
}

DivMacroInt* DivPlatformYM2609::getChanMacroInt(int ch) {
  return &chan[ch].std;
}

DivChannelModeHints DivPlatformYM2609::getModeHints(int ch) {
  DivChannelModeHints ret;
  /*ret.count=1;
  ret.hint[0]=ICON_FA_BELL_SLASH_O;
  ret.type[0]=0;
  if (!chan[ch].gate) {
    ret.type[0]=4;
  }*/

  return ret;
}

DivDispatchOscBuffer* DivPlatformYM2609::getOscBuffer(int ch) {
  return oscBuf[ch];
}

unsigned short DivPlatformYM2609::getPan(int ch) {
  return (chan[ch].panLeft<<8)|chan[ch].panRight;
}

unsigned char* DivPlatformYM2609::getRegisterPool() {
  return regPool;
}

int DivPlatformYM2609::getRegisterPoolSize() {
  return YM2609_NUM_REGISTERS;
}

float DivPlatformYM2609::getPostAmp() {
  return 1.0f;
}

void DivPlatformYM2609::reset() {
  while (!writes.empty()) writes.pop();
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) {
    
  }

  ym2609->Init(YM2609_CLOCK, YM2609_CLOCK);
  ym2609->Reset();

  //ws.setEngine(parent);
  //ws.init(NULL,256,255,false);

  //sid3_reset(sid3);
  memset(regPool,0,YM2609_NUM_REGISTERS);
}

int DivPlatformYM2609::getOutputCount() {
  return 2;
}

bool DivPlatformYM2609::hasSoftPan(int ch) {
  return true;
}

bool DivPlatformYM2609::getDCOffRequired()
{
  return false;
}

void DivPlatformYM2609::poke(unsigned int addr, unsigned short val) {
  rWrite(addr,val);
}

void DivPlatformYM2609::poke(std::vector<DivRegWrite>& wlist) {
  for (DivRegWrite& i: wlist) rWrite(i.addr,i.val);
}

void DivPlatformYM2609::setFlags(const DivConfig& flags) {
    chipClock=1000000;
    rate=YM2609_DSP_RATE;
  
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) {
    oscBuf[i]->setRate(rate);
  }
}

void DivPlatformYM2609::getPaired(int ch, std::vector<DivChannelPair>& ret)
{
  
}

int DivPlatformYM2609::init(DivEngine* p, int channels, int sugRate, const DivConfig& flags) {
  parent=p;
  dumpWrites=false;
  skipRegisterWrites=false;
  writeOscBuf=0;

  output_buf = new int*[2];

  output_buf[0] = new int[1];
  output_buf[1] = new int[1];
  
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) 
  {
    isMuted[i]=false;
    oscBuf[i]=new DivDispatchOscBuffer;
  }

    rev = new reverb(YM2609_DSP_RATE * 4, 39);
    dist = new distortion(YM2609_CLOCK, 39);
    chor = new chorus(YM2609_CLOCK, 39);
    eq = new eq3band(YM2609_DSP_RATE);
    filt = new HPFLPF(YM2609_CLOCK, 39);
    reph = new ReversePhase();
    comp = new Compressor(YM2609_DSP_RATE, 39);

    ym2609 = new OPNA2(0, rev, dist, chor, eq, filt, reph, comp);

    ym2609->Init(YM2609_CLOCK, YM2609_CLOCK);

  setFlags(flags);

  reset();

  return YM2609_NUM_CHANNELS;
}

void DivPlatformYM2609::quit() {
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) 
  {
    delete oscBuf[i];
  }
  if (ym2609!=NULL)
  {
    delete rev;
    delete dist;
    delete chor;
    delete eq;
    delete filt;
    delete reph;
    delete comp;
    delete ym2609;
    ym2609 = NULL;
  }

  delete[] output_buf[0];
  delete[] output_buf[1];
  delete[] output_buf;
}

DivPlatformYM2609::~DivPlatformYM2609() {
}
