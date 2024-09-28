
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
#define rWrite(a,v) if (!skipRegisterWrites) {writes.push(QueuedWrite(a,v)); if (dumpWrites) {addWrite(a,v);} }

#define CHIP_FREQBASE 524288
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
    crapsynth_clock(crap_synth);

    while (!writes.empty()) {
      QueuedWrite w=writes.front();
      //write
      regPool[w.addr&0x7f]=w.val;
      writes.pop();
    }

    for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) 
    {
      oscBuf[i]->data[oscBuf[i]->needle++]=0;
    }

    buf[0][h]=0;
  }
}

void DivPlatformSTM32CRAPSYNTH::updateWave(int ch) {
  
}

void DivPlatformSTM32CRAPSYNTH::tick(bool sysTick) 
{
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) 
  {
    chan[i].std.next();
    
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
    
    if (chan[i].active) {
      if (chan[i].ws.tick()) {
        updateWave(i);
      }
    }
    if (chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff) {
      //DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_PCE);
      //chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,true,0,chan[i].pitch2,chipClock,CHIP_DIVIDER);

      if (chan[i].keyOn) chan[i].keyOn=false;
      if (chan[i].keyOff) chan[i].keyOff=false;
      chan[i].freqChanged=false;
    }
  }
}

int DivPlatformSTM32CRAPSYNTH::dispatch(DivCommand c) {
  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_PCE);
      
      chan[c.chan].active=true;
      chan[c.chan].keyOn=true;
      chan[c.chan].macroInit(ins);
      if (!parent->song.brokenOutVol && !chan[c.chan].std.vol.will) {
        chan[c.chan].outVol=chan[c.chan].vol;
      }
      if (chan[c.chan].wave<0) {
        chan[c.chan].wave=0;
        chan[c.chan].ws.changeWave1(chan[c.chan].wave);
      }
      chan[c.chan].ws.init(ins,256,256,chan[c.chan].insChanged);
      chan[c.chan].insChanged=false;
      break;
    }
    case DIV_CMD_NOTE_OFF:
      chan[c.chan].dacSample=-1;
      if (dumpWrites) addWrite(0xffff0002+(c.chan<<8),0);
      chan[c.chan].pcm=false;
      chan[c.chan].active=false;
      chan[c.chan].keyOff=true;
      chan[c.chan].macroInit(NULL);
      break;
    case DIV_CMD_NOTE_OFF_ENV:
    case DIV_CMD_ENV_RELEASE:
      chan[c.chan].std.release();
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
      chan[c.chan].ws.changeWave1(chan[c.chan].wave);
      chan[c.chan].keyOn=true;
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
    case DIV_CMD_SAMPLE_POS:
      chan[c.chan].dacPos=c.value;
      break;
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
    case DIV_CMD_GET_VOLMAX:
      return 31;
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
    updateWave(i);
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
  if (ch>=6) return DivSamplePos();
  if (!chan[ch].pcm) return DivSamplePos();
  return DivSamplePos(
    chan[ch].dacSample,
    chan[ch].dacPos,
    chan[ch].dacRate
  );
}

DivDispatchOscBuffer* DivPlatformSTM32CRAPSYNTH::getOscBuffer(int ch) {
  return oscBuf[ch];
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
  return 128;
}

void DivPlatformSTM32CRAPSYNTH::reset() {
  writes.clear();
  memset(regPool,0,128);
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    chan[i]=DivPlatformSTM32CRAPSYNTH::Channel();
    chan[i].std.setEngine(parent);
    chan[i].ws.setEngine(parent);
    chan[i].ws.init(NULL,256,256,false);
  }
  if (dumpWrites) 
  {
    addWrite(0xffffffff,0);
  }
  crapsynth_reset(crap_synth);
}

int DivPlatformSTM32CRAPSYNTH::getOutputCount() {
  return 1;
}

bool DivPlatformSTM32CRAPSYNTH::keyOffAffectsArp(int ch) {
  return false;
}

void DivPlatformSTM32CRAPSYNTH::notifyWaveChange(int wave) {
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    if (chan[i].wave==wave) {
      chan[i].ws.changeWave1(wave);
      updateWave(i);
    }
  }
}

void DivPlatformSTM32CRAPSYNTH::notifyInsDeletion(void* ins) {
  for (int i=0; i<6; i++) {
    chan[i].std.notifyInsDeletion((DivInstrument*)ins);
  }
}

void DivPlatformSTM32CRAPSYNTH::setFlags(const DivConfig& flags) {
  chipClock=25000000;
  CHECK_CUSTOM_CLOCK;
  rate=chipClock/100; // 250 kHz
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
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    isMuted[i]=false;
    oscBuf[i]=new DivDispatchOscBuffer;
  }
  crap_synth = crapsynth_create();
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
