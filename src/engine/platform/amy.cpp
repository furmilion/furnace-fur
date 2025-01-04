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

#include "amy.h"
#include "../engine.h"
#include "IconsFontAwesome4.h"
#include <math.h>
#include "../../ta-log.h"

#define rWrite(a,v) if (!skipRegisterWrites) {writes.push(QueuedWrite(a,v)); if (dumpWrites) {addWrite(a,v);} }

#define CHIP_FREQBASE 524288
#define CHIP_DIVIDER 1

const char* regCheatSheetAMY[]={
  NULL
};

const char** DivPlatformAMY::getRegisterSheet() {
  return regCheatSheetAMY;
}

void DivPlatformAMY::acquire(short** buf, size_t len) 
{
  for (size_t i=0; i<len; i++) 
  {
    amy_clock(amy);

    buf[0][i]=amy->output;

    for(int j = 0; j < AMY_NUM_CHANNELS; j++)
    {
      oscBuf[j]->data[oscBuf[j]->needle++] = amy->muted[j] ? 0 : (amy->channel_output[j] / 4);
    }
  }
}

void DivPlatformAMY::tick(bool sysTick) 
{
  bool doUpdateWave = false;

  for (int i=0; i<AMY_NUM_CHANNELS; i++) 
  {
    chan[i].std.next();

    DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_AMY);

    if (chan[i].std.vol.had) 
    {
      chan[i].outVol=VOL_SCALE_LINEAR(chan[i].vol&255,MIN(255,chan[i].std.vol.val),255);
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
    if (chan[i].std.wave.had) {
      DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_AMY);

      
    }

    if (chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff) 
    {
      chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock,CHIP_FREQBASE);

      if (chan[i].keyOn) 
      {
        
      }
      if (chan[i].keyOff) 
      {
        
      }

      if (chan[i].keyOn) chan[i].keyOn=false;
      if (chan[i].keyOff) chan[i].keyOff=false;
      chan[i].freqChanged=false;
    }
  }
}

int DivPlatformAMY::dispatch(DivCommand c) {
  if (c.chan>AMY_NUM_CHANNELS - 1) return 0;

  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_AMY);
      if (c.value!=DIV_NOTE_NULL) {
        chan[c.chan].baseFreq=NOTE_FREQUENCY(c.value);
        chan[c.chan].freqChanged=true;
        chan[c.chan].note=c.value;
      }
      chan[c.chan].active=true;
      chan[c.chan].keyOn=true;

      if (chan[c.chan].insChanged) 
      {
        
      }
      if (chan[c.chan].insChanged) {
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
          chan[c.chan].outVol=c.value;
          chan[c.chan].vol=chan[c.chan].outVol;
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
        if (parent->song.resetMacroOnPorta || parent->song.preNoteNoEffect) {
          chan[c.chan].macroInit(parent->getIns(chan[c.chan].ins,DIV_INS_AMY));
          chan[c.chan].keyOn=true;
        }
      }
      if (!chan[c.chan].inPorta && c.value && !parent->song.brokenPortaArp && chan[c.chan].std.arp.will && !NEW_ARP_STRAT) chan[c.chan].baseFreq=NOTE_FREQUENCY(chan[c.chan].note);
      chan[c.chan].inPorta=c.value;
      break;
    case DIV_CMD_GET_VOLMAX:
      return AMY_MAX_VOL;
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

void DivPlatformAMY::muteChannel(int ch, bool mute) {
  isMuted[ch]=mute;
  amy_set_is_muted(amy,ch,mute);
}

void DivPlatformAMY::forceIns() {
  for (int i=0; i<AMY_NUM_CHANNELS; i++) {
    chan[i].insChanged=true;
    if (chan[i].active) {
      chan[i].keyOn=true;
      chan[i].freqChanged=true;
    }
  }
}

void DivPlatformAMY::notifyInsChange(int ins) {
  for (int i=0; i<AMY_NUM_CHANNELS; i++) {
    if (chan[i].ins==ins) {
      chan[i].insChanged=true;
    }
  }
}

void DivPlatformAMY::notifyInsDeletion(void* ins) {
  for (int i=0; i<AMY_NUM_CHANNELS; i++) {
    chan[i].std.notifyInsDeletion((DivInstrument*)ins);
  }
}

void* DivPlatformAMY::getChanState(int ch) {
  return &chan[ch];
}

DivMacroInt* DivPlatformAMY::getChanMacroInt(int ch) {
  return &chan[ch].std;
}

DivDispatchOscBuffer* DivPlatformAMY::getOscBuffer(int ch) {
  return oscBuf[ch];
}

unsigned char* DivPlatformAMY::getRegisterPool() {
  return regPool;
}

int DivPlatformAMY::getRegisterPoolSize() {
  return AMY_NUM_REGISTERS;
}

float DivPlatformAMY::getPostAmp() {
  return 1.0f;
}

void DivPlatformAMY::reset() {
  while (!writes.empty()) writes.pop();

  for (int i=0; i<AMY_NUM_CHANNELS; i++) 
  {
    chan[i]=DivPlatformAMY::Channel();
    chan[i].std.setEngine(parent);
    chan[i].vol = AMY_MAX_VOL;
  }

  amy_reset(amy);
  memset(regPool,0,AMY_NUM_REGISTERS);
}

int DivPlatformAMY::getOutputCount() {
  return 1;
}

bool DivPlatformAMY::getDCOffRequired()
{
  return false;
}

void DivPlatformAMY::poke(unsigned int addr, unsigned short val) {
  rWrite(addr,val);
}

void DivPlatformAMY::poke(std::vector<DivRegWrite>& wlist) {
  for (DivRegWrite& i: wlist) rWrite(i.addr,i.val);
}

void DivPlatformAMY::setFlags(const DivConfig& flags) {
  chipClock=4000000;
  CHECK_CUSTOM_CLOCK;

  rate=chipClock / 128;
  for (int i=0; i<AMY_NUM_CHANNELS; i++) {
    oscBuf[i]->rate=rate;
  }
}

int DivPlatformAMY::init(DivEngine* p, int channels, int sugRate, const DivConfig& flags) {
  parent=p;
  dumpWrites=false;
  skipRegisterWrites=false;
  
  for (int i=0; i<AMY_NUM_CHANNELS; i++) 
  {
    isMuted[i]=false;
    oscBuf[i]=new DivDispatchOscBuffer;
  }

  amy = amy_create();

  setFlags(flags);

  reset();

  return AMY_NUM_CHANNELS;
}

void DivPlatformAMY::quit() {
  for (int i=0; i<AMY_NUM_CHANNELS; i++) 
  {
    delete oscBuf[i];
  }
  if (amy!=NULL)
  {
    amy_free(amy);
    amy = NULL;
  }
}

DivPlatformAMY::~DivPlatformAMY() {
}
