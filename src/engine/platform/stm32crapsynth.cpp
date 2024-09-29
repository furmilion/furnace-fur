
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
    for(int i = 0; i < 100; i++)
    {
        crapsynth_clock(crap_synth);
    }
    
    for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) 
    {
        oscBuf[i]->data[oscBuf[i]->needle++]=CLAMP(crap_synth->chan_outputs[i],-32767,32768);
    }

    while (!writes.empty()) {
        QueuedWrite w=writes.front();
        int ch = w.addr >> 8;
        int type = w.addr & 0xff;
        crapsynth_write(crap_synth, ch, type, w.val);
        regPool[ch * 8 + type]=w.val;
        writes.pop();
    }

    buf[0][h]=CLAMP(crap_synth->final_output,-32767,32768);
  }
}

void DivPlatformSTM32CRAPSYNTH::updateWave(int ch) {
  
}

void DivPlatformSTM32CRAPSYNTH::tick(bool sysTick) 
{
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) 
  {
    chan[i].std.next();

    if (chan[i].std.vol.had) {
      if(i < 5)
      {
        chan[i].outVol=VOL_SCALE_LOG(chan[i].vol&255,MIN(255,chan[i].std.vol.val),255);
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
    }

    if (chan[i].std.duty.had) {
      if(i < 4)
      {
        chan[i].duty = chan[i].std.duty.val;
        if(chan[i].wave == 5)
        {
            ad9833_write(i, 5, chan[i].std.duty.val);
        }
      }
    }

    if (chan[i].std.phaseReset.had) {
      if(i < 5 && chan[i].std.phaseReset.val)
      {
        ad9833_write(i, 3, chan[i].lfsr); //LFSR value is ignored for all chans except noise
      }
    }

    if (chan[i].std.ex1.had) { //zero cross detection
      if(i < 5)
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
    
    if (chan[i].active) {
      if (chan[i].ws.tick()) {
        updateWave(i);
      }
    }
    if (chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff) {
      //DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_PCE);
      if(chan[i].freqChanged && i < 4)
      {
        chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock,CHIP_FREQBASE);
        chan[i].timer_freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock,CHIP_TIMERS_FREQBASE);

        if(chan[i].freq > (1 << 28) - 1) chan[i].freq = (1 << 28) - 1;
        if(chan[i].timer_freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;
        
        if(chan[i].wave != 5)
        {
            ad9833_write(i, 2, chan[i].freq);
        }
        else
        {
            ad9833_write(i, 4, chan[i].timer_freq);
        }
      }
      if(chan[i].freqChanged && i == 4)
      {
        chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,2,chan[i].pitch2,chipClock,CHIP_TIMERS_FREQBASE * 32);
        chan[i].timer_freq=chan[i].freq;

        while((double)chan[i].freq > 333300.0 * (double)(1 << 29) / (double)chipClock) //333333.(3) Hz max MM5437 clock freq
        {
            chan[i].freq = 333300.0 * (double)(1 << 29) / (double)chipClock;
            chan[i].timer_freq=chan[i].freq;
        }

        //if(chan[i].freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;
        //if(chan[i].timer_freq > (1 << 30) - 1) chan[i].freq = (1 << 30) - 1;
        
        ad9833_write(i, 4, chan[i].timer_freq);
      }
      if(chan[i].keyOff && i < 5)
      {
        ad9833_write(i, 0, 0);
      }

      if (chan[i].keyOn) chan[i].keyOn=false;
      if (chan[i].keyOff) chan[i].keyOff=false;
      chan[i].freqChanged=false;
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
      //if (dumpWrites) addWrite(0xffff0002+(c.chan<<8),0);
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
      chan[c.chan].ws.changeWave1(chan[c.chan].wave);
      if(c.chan < 4)
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
    case DIV_CMD_SAMPLE_POS:
      //chan[c.chan].dacPos=c.value;
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
  /*return DivSamplePos(
    chan[ch].dacSample,
    chan[ch].dacPos,
    chan[ch].dacRate
  );*/

  return DivSamplePos();
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
  return 8*11;
}

int DivPlatformSTM32CRAPSYNTH::getRegisterPoolDepth()
{
    return 32; // some of the registers of STM32 timers are 32-bit. or 24 bit. AD9833 phase acc/frequency is 28 bits...
}

void DivPlatformSTM32CRAPSYNTH::reset() {
  writes.clear();
  memset(regPool,0,8*11*sizeof(unsigned int));
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    chan[i]=DivPlatformSTM32CRAPSYNTH::Channel();
    chan[i].std.setEngine(parent);
    chan[i].ws.setEngine(parent);
    chan[i].ws.init(NULL,256,256,false);

    chan[i].duty = 0x7fff;
    chan[i].wave = 0;
  }
  if (dumpWrites) 
  {
    addWrite(0xffffffff,0);
  }
  crapsynth_reset(crap_synth);
  writeOscBuf = 0;
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

const void* DivPlatformSTM32CRAPSYNTH::getSampleMem(int index) {
  return index == 0 ? (unsigned char*)crap_synth->sample_mem : NULL;
}

size_t DivPlatformSTM32CRAPSYNTH::getSampleMemCapacity(int index) {
  return index == 0 ? STM32CRAPSYNTH_SAMPLE_MEM_SIZE : 0;
}

size_t DivPlatformSTM32CRAPSYNTH::getSampleMemUsage(int index) {
  return index == 0 ? sampleMemLen : 0;
}

bool DivPlatformSTM32CRAPSYNTH::isSampleLoaded(int index, int sample) {
  if (index!=0) return false;
  if (sample<0 || sample>255) return false;
  return sampleLoaded[sample];
}

const DivMemoryComposition* DivPlatformSTM32CRAPSYNTH::getMemCompo(int index) {
  if (index!=0) return NULL;
  return &sampleMemCompo;
}

void DivPlatformSTM32CRAPSYNTH::renderSamples(int sysID) {
  size_t maxPos=getSampleMemCapacity();
  memset(crap_synth->sample_mem,0,maxPos);
  sampleMemCompo.entries.clear();
  sampleMemCompo.capacity=maxPos;

  // dummy zero-length samples are at pos 0 as the engine still outputs them
  size_t memPos=1;
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
      memcpy(&crap_synth->sample_mem[memPos],s->data8,actualLength);
      memPos+=actualLength;
      sampleMemCompo.entries.push_back(DivMemoryEntry(DIV_MEMORY_SAMPLE,"PCM",i,sampleOff[i],memPos));
    }
    if (actualLength<length) 
    {
      logW("out of STM32CrapSynth sample memory for sample %d!",i);
      break;
    }
    sampleLoaded[i]=true;
  }
  sampleMemLen=memPos;
  sampleMemCompo.used=sampleMemLen;
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
  skipRegisterWrites=false;
  for (int i=0; i<STM32CRAPSYNTH_NUM_CHANNELS; i++) {
    isMuted[i]=false;
    oscBuf[i]=new DivDispatchOscBuffer;
  }
  crap_synth = crapsynth_create();
  sampleMemCompo=DivMemoryComposition();
  sampleMemCompo.name=_("Sample memory");
  sampleMemCompo.capacity=STM32CRAPSYNTH_SAMPLE_MEM_SIZE;
  sampleMemCompo.memory=(unsigned char*)crap_synth->sample_mem;
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
