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

#define PLEASE_HELP_ME(_targetChan,blk) \
  int boundaryBottom=parent->calcBaseFreq(chipClock,CHIP_FREQBASE,0,false); \
  int boundaryTop=parent->calcBaseFreq(chipClock,CHIP_FREQBASE,12,false); \
  int destFreq=NOTE_FNUM_BLOCK(c.value2,11,blk); \
  int newFreq; \
  bool return2=false; \
  if (_targetChan.portaPause) { \
    if (parent->song.compatFlags.oldOctaveBoundary) { \
      if ((_targetChan.portaPauseFreq&0xf800)>(_targetChan.baseFreq&0xf800)) { \
        _targetChan.baseFreq=((_targetChan.baseFreq&0x7ff)>>1)|(_targetChan.portaPauseFreq&0xf800); \
      } else { \
        _targetChan.baseFreq=((_targetChan.baseFreq&0x7ff)<<1)|(_targetChan.portaPauseFreq&0xf800); \
      } \
      c.value*=2; \
    } else { \
      _targetChan.baseFreq=_targetChan.portaPauseFreq; \
    } \
  } \
  if (destFreq>_targetChan.baseFreq) { \
    newFreq=_targetChan.baseFreq+c.value; \
    if (newFreq>=destFreq) { \
      newFreq=destFreq; \
      return2=true; \
    } \
  } else { \
    newFreq=_targetChan.baseFreq-c.value; \
    if (newFreq<=destFreq) { \
      newFreq=destFreq; \
      return2=true; \
    } \
  } \
  /* check for octave boundary */ \
  /* what the heck! */ \
  if (!_targetChan.portaPause) { \
    if ((newFreq&0x7ff)>boundaryTop && (newFreq&0xf800)<0x3800) { \
      if (parent->song.compatFlags.fbPortaPause) { \
        _targetChan.portaPauseFreq=(boundaryBottom)|((newFreq+0x800)&0xf800); \
        _targetChan.portaPause=true; \
        break; \
      } else { \
        newFreq=((newFreq&0x7ff)>>1)|((newFreq+0x800)&0xf800); \
      } \
    } \
    if ((newFreq&0x7ff)<boundaryBottom && (newFreq&0xf800)>0) { \
      if (parent->song.compatFlags.fbPortaPause) { \
        _targetChan.portaPauseFreq=newFreq=(boundaryTop-1)|((newFreq-0x800)&0xf800); \
        _targetChan.portaPause=true; \
        break; \
      } else { \
        newFreq=((newFreq&0x7ff)<<1)|((newFreq-0x800)&0xf800); \
      } \
    } \
  } \
  _targetChan.portaPause=false; \
  _targetChan.freqChanged=true; \
  _targetChan.baseFreq=newFreq; \
  if (return2) { \
    _targetChan.inPorta=false; \
    return 2; \
  }

#define KVS(x,y) ((chan[x].state.op[y].kvs==2 && isOutput[chan[x].state.alg][y]) || chan[x].state.op[y].kvs==1)

#define rWrite(a,v) if (!skipRegisterWrites) {writes.push(QueuedWrite(a,v)); if (dumpWrites) {addWrite(a,v);} }
//#define rWrite(a,v) ym2609->SetReg(a, v);
//#define immWrite(a,v) if (!skipRegisterWrites) {writes.push_back(QueuedWrite(a,v)); if (dumpWrites) {addWrite(a,v);} }
#define immWrite(a,v) ym2609->SetReg(a, v); regPool[a % YM2609_NUM_REGISTERS]=v;

//TODO: replace with custom clock

#define YM2609_CLOCK 8000000
#define YM2609_DSP_RATE 48000

#define CHIP_FREQBASE fmFreqBase
#define CHIP_DIVIDER fmDivBase

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

    /*if(ym2609->fm6[0].ch[0].op[0].eg_phase_ != fmvgen::Operator::EGPhase::off)
    {
      logD("ch0 op0 eg stage %d acc (level) %d eg_out_ %d tl_out_ %d", (int)ym2609->fm6[0].ch[0].op[0].eg_phase_, (int)ym2609->fm6[0].ch[0].op[0].eg_level_, (int)ym2609->fm6[0].ch[0].op[0].eg_out_, (int)ym2609->fm6[0].ch[0].op[0].tl_out_);
    }*/

    if (!writes.empty()) 
    {
      for(int i = 0; i < clocks_per_sample; i++)
      {
        QueuedWrite w=writes.front();
        ym2609->SetReg(w.addr, w.val);
        regPool[w.addr % YM2609_NUM_REGISTERS]=w.val;
        writes.pop();

        if(writes.empty()) break;
      }
    }

    buf[0][samp] = output_buf[0][0];
    buf[1][samp] = output_buf[1][0];
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

    DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_YM2609_FM);
    
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
        chan[i].baseFreq=NOTE_FNUM_BLOCK(parent->calcArp(chan[i].note,chan[i].std.arp.val),11,chan[i].state.block);
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
  }

  for (int i=0; i<12; i++) {
    //if (i==2 && extMode) continue;
    if (chan[i].keyOff) {
      immWrite(((i > 5) ? 0x228 : 0x28),0x00|konOffs[i % 6]);
      chan[i].keyOff=false;
    }
  }
    
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) 
  {
    if (chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff) 
    {
      if (chan[i].freqChanged) {
        if (parent->song.compatFlags.linearPitch) {
          chan[i].freq=parent->calcFreq(chan[i].baseFreq,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,1,chan[i].pitch2,chipClock,CHIP_FREQBASE,11,chan[i].state.block);
        } else {
          int fNum=parent->calcFreq(chan[i].baseFreq&0x7ff,chan[i].pitch,chan[i].fixedArp?chan[i].baseNoteOverride:chan[i].arpOff,chan[i].fixedArp,false,1,chan[i].pitch2,chipClock,CHIP_FREQBASE,11);
          int block=(chan[i].baseFreq&0xf800)>>11;
          if (fNum<0) fNum=0;
          if (fNum>2047) {
            while (block<7) {
              fNum>>=1;
              block++;
            }
            if (fNum>2047) fNum=2047;
          }
          chan[i].freq=(block<<11)|fNum;
        }
        if (chan[i].freq>0x3fff) chan[i].freq=0x3fff;
        //if (i<6) {
          immWrite(chanOffs[i]+ADDR_FREQH,(chan[i].freq>>8)|((chan[i].panLeft&3)<<6));
          immWrite(chanOffs[i]+ADDR_FREQ,chan[i].freq&0xff);
        //}
        chan[i].freqChanged=false;
      }
      if ((chan[i].keyOn || chan[i].opMaskChanged)) {
        if (i<6) {
          immWrite(0x28,(chan[i].opMask<<4)|konOffs[i]);
        }
        else
        {
          immWrite(0x228,(chan[i].opMask<<4)|konOffs[i-6]);
        }
        chan[i].opMaskChanged=false;
        chan[i].keyOn=false;
      }
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

void DivPlatformYM2609::commitState(int ch, DivInstrument* ins) {
  if (chan[ch].insChanged) 
  {
    chan[ch].state=ins->fm;
    chan[ch].state_ym2609fm=ins->ym2609.ym2609fm;

    chan[ch].opMask=
      (chan[ch].state.op[0].enable?1:0)|
      (chan[ch].state.op[2].enable?2:0)|
      (chan[ch].state.op[1].enable?4:0)|
      (chan[ch].state.op[3].enable?8:0);
  }
  
  for (int i=0; i<4; i++) 
  {
    unsigned short baseAddr=chanOffs[ch]|opOffs[i];
    DivInstrumentFM::Operator& op=chan[ch].state.op[i];
    DivInstrumentYM2609FM::Operator& op_ym2609=chan[ch].state_ym2609fm.op[i];
    if (isMuted[ch] || !op.enable) {
      if (chan[ch].insChanged) {
        rWrite(baseAddr+ADDR_TL,127|(((chan[ch].op_ym2609[i].wave_type >> 1) & 1) << 7));
      }
    } else {
      if (KVS(ch,i)) {
        if (!chan[ch].active || chan[ch].insChanged) {
          rWrite(baseAddr+ADDR_TL,(127-VOL_SCALE_LOG_BROKEN(127-op.tl,chan[ch].outVol&0x7f,127))|(((chan[ch].op_ym2609[i].wave_type >> 1) & 1) << 7));
        }
      } else {
        if (chan[ch].insChanged) {
          rWrite(baseAddr+ADDR_TL,op.tl|(((chan[ch].op_ym2609[i].wave_type >> 1) & 1) << 7));
        }
      }
    }
    if (chan[ch].insChanged) {
      rWrite(baseAddr+ADDR_MULT_DT,(op.mult&15)|(dtTable[op.dt&7]<<4)|((chan[ch].op_ym2609[i].wave_type & 1) << 7));
      rWrite(baseAddr+ADDR_RS_AR,(op.ar&31)|(op.rs<<6)|((op_ym2609.phase_reset & 1) << 5));
      rWrite(baseAddr+ADDR_AM_DR,(op.dr&31)|(op.am<<7)|(op.dt2<<5));

      /*if(i != 0)
      {
        rWrite(baseAddr+ADDR_DT2_D2R,(op.d2r&31)|(op_ym2609.feedback<<5));
      }
      else
      {
        rWrite(baseAddr+ADDR_DT2_D2R,(op.d2r&31));
      }*/

      rWrite(baseAddr+ADDR_DT2_D2R,(op.d2r&31)|(op_ym2609.feedback<<5));
      
      rWrite(baseAddr+ADDR_SL_RR,(op.rr&15)|(op.sl<<4));
      rWrite(baseAddr+ADDR_SSG,(op.ssgEnv&15)|(op_ym2609.alg_link<<4));
    }
  }
  if (chan[ch].insChanged) {
    rWrite(chanOffs[ch]+ADDR_FB_ALG,(chan[ch].state.alg&7)|(chan[ch].state.fb<<3)|((chan[ch].panRight&3)<<6));
    rWrite(chanOffs[ch]+ADDR_LRAF,(isMuted[ch]?0:(chan[ch].pan<<6))|(chan[ch].state.fms&7)|((chan[ch].state.ams&3)<<4)|(chan[ch].state_ym2609fm.alg_construct_switch<<3));
  }
}

int DivPlatformYM2609::dispatch(DivCommand c) {
  if (c.chan>YM2609_NUM_CHANNELS - 1) return 0;

  //bool updEnv = false;
  //DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_YM2609_FM);
  //int filter = 0;

  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      if(c.chan < 12) //FM
      {
        DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_YM2609_FM);

        chan[c.chan].insChanged=true;
        commitState(c.chan,ins);
        chan[c.chan].insChanged=false;

        chan[c.chan].macroInit(ins);

        if (!chan[c.chan].std.vol.will) {
          chan[c.chan].outVol=chan[c.chan].vol;
        }

        if (c.value!=DIV_NOTE_NULL) {
          chan[c.chan].baseFreq=NOTE_FNUM_BLOCK(c.value,11,chan[c.chan].state.block);
          chan[c.chan].portaPause=false;
          chan[c.chan].freqChanged=true;
          chan[c.chan].note=c.value;
        }

        chan[c.chan].active=true;
        chan[c.chan].keyOn=true;
      }
      
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
      PLEASE_HELP_ME(chan[c.chan],chan[c.chan].state.block);
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
          chan[c.chan].macroInit(parent->getIns(chan[c.chan].ins,DIV_INS_YM2609_FM));
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

  //ym2609->Init(YM2609_CLOCK, YM2609_CLOCK);
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
  chipClock=YM2609_CLOCK;
  rate=YM2609_DSP_RATE; //TODO: map somehow?

  clocks_per_sample = (int)ceil((double)chipClock / (double)rate);
  
    // Prescaler flags
  switch (flags.getInt("prescale",0)) {
    case 0x01: // /3
      prescale=0x2e;
      fmFreqBase=9440540.0/2.0;
      /*fmDivBase=36,
      ayDiv=16;
      nukedMult=16;*/
      break;
    case 0x02: // /2
      prescale=0x2f;
      fmFreqBase=9440540.0/3.0;
      /*fmDivBase=24,
      ayDiv=8;
      nukedMult=24;*/
      break;
    default: // /6
      prescale=0x2d;
      fmFreqBase=9440540.0;
      /*fmDivBase=72,
      ayDiv=32;
      nukedMult=8;*/
      break;
  }

  immWrite(0x2d,0xff);
  immWrite(prescale,0xff);

  // enable 6 channel mode
  immWrite(0x29,0x80);

  // enable 6 channel mode
  immWrite(0x229,0x80);
  
  for (int i=0; i<YM2609_NUM_CHANNELS; i++) {
    oscBuf[i]->setRate(rate);
  }
}

void DivPlatformYM2609::getPaired(int ch, std::vector<DivChannelPair>& ret)
{
  
}

const DivMemoryComposition* DivPlatformYM2609::getMemCompo(int index) {
  if (index==0) return &memCompoA;
  if (index==1) return &memCompoB[0];
  if (index==2) return &memCompoB[1];
  if (index==3) return &memCompoB[2];
  return NULL;
}

const void* DivPlatformYM2609::getSampleMem(int index) {
  if(index == 0) return adpcma_buf;
  if(index == 1) return ym2609->get_adpcmb_buf_pointer(0);
  if(index == 2) return ym2609->get_adpcmb_buf_pointer(1);
  if(index == 3) return ym2609->get_adpcmb_buf_pointer(2);
  return NULL;
}

size_t DivPlatformYM2609::getSampleMemCapacity(int index) {
  if(index == 0) return 0x1000000;
  if(index == 1) return 0x40000;
  if(index == 2) return 0x1000000;
  if(index == 3) return 0x1000000;
  return 0;
}

const char* DivPlatformYM2609::getSampleMemName(int index) {
  if(index == 0) return "ADPCM-A";
  if(index == 1) return "ADPCM-B 1";
  if(index == 2) return "ADPCM-B 2";
  if(index == 3) return "ADPCM-B 3";
  return NULL;
}

size_t DivPlatformYM2609::getSampleMemUsage(int index) {
  if(index == 0) return adpcmAMemLen;
  if(index == 1) return adpcmBMemLen[0];
  if(index == 2) return adpcmBMemLen[1];
  if(index == 3) return adpcmBMemLen[2];
  return NULL;
}

bool DivPlatformYM2609::isSampleLoaded(int index, int sample) {
  if (index<0 || index>3) return false;
  if (sample<0 || sample>32767) return false;
  return sampleLoaded[index][sample];
}

void DivPlatformYM2609::renderSamples(int sysID)
{
  memset(adpcma_buf,0,getSampleMemCapacity(0));
  memset(ym2609->get_adpcmb_buf_pointer(0),0,getSampleMemCapacity(1));
  memset(ym2609->get_adpcmb_buf_pointer(1),0,getSampleMemCapacity(2));
  memset(ym2609->get_adpcmb_buf_pointer(2),0,getSampleMemCapacity(3));
  memset(sampleOffA,0,32768*sizeof(unsigned int));
  memset(sampleOffB[0],0,32768*sizeof(unsigned int));
  memset(sampleOffB[1],0,32768*sizeof(unsigned int));
  memset(sampleOffB[2],0,32768*sizeof(unsigned int));
  memset(sampleLoaded[0],0,32768*sizeof(bool));
  memset(sampleLoaded[1],0,32768*sizeof(bool));
  memset(sampleLoaded[2],0,32768*sizeof(bool));
  memset(sampleLoaded[3],0,32768*sizeof(bool));

  memCompoA=DivMemoryComposition();
  memCompoA.name="ADPCM-A";

  memCompoB[0]=DivMemoryComposition();
  memCompoB[0].name="ADPCM-B 1";
  memCompoB[1]=DivMemoryComposition();
  memCompoB[1].name="ADPCM-B 2";
  memCompoB[2]=DivMemoryComposition();
  memCompoB[2].name="ADPCM-B 3";

  size_t memPos=0;
  for (int i=0; i<parent->song.sampleLen; i++) 
  {
    DivSample* s=parent->song.sample[i];
    if (!s->renderOn[0][sysID]) {
      sampleOffA[i]=0;
      continue;
    }

    int paddedLen=(s->lengthA+255)&(~0xff);
    if ((memPos&0xf00000)!=((memPos+paddedLen)&0xf00000)) {
      memPos=(memPos+0xfffff)&0xf00000;
    }
    if (memPos>=getSampleMemCapacity(0)) {
      logW("out of ADPCM-A memory for sample %d!",i);
      break;
    }
    if (memPos+paddedLen>=getSampleMemCapacity(0)) {
      memcpy(adpcma_buf+memPos,s->dataA,getSampleMemCapacity(0)-memPos);
      logW("out of ADPCM-A memory for sample %d!",i);
    } else {
      memcpy(adpcma_buf+memPos,s->dataA,paddedLen);
      sampleLoaded[0][i]=true;
    }
    sampleOffA[i]=memPos;
    memCompoA.entries.push_back(DivMemoryEntry(DIV_MEMORY_SAMPLE,"Sample",i,memPos,memPos+paddedLen));
    memPos+=paddedLen;
  }
  adpcmAMemLen=memPos+256;

  memCompoA.used=adpcmAMemLen;
  memCompoA.capacity=getSampleMemCapacity(0);

  for(int m = 0; m < 3; m++)
  {
    unsigned char* adpcmBMem = ym2609->get_adpcmb_buf_pointer(m);

    memset(adpcmBMem,0,getSampleMemCapacity(1+m));

    memPos=0;
    for (int i=0; i<parent->song.sampleLen; i++) {
      DivSample* s=parent->song.sample[i];
      if (!s->renderOn[1+m][sysID]) {
        sampleOffB[m][i]=0;
        continue;
      }

      int paddedLen=(s->lengthB+255)&(~0xff);
      if ((memPos&0xf00000)!=((memPos+paddedLen)&0xf00000)) {
        memPos=(memPos+0xfffff)&0xf00000;
      }
      if (memPos>=getSampleMemCapacity(1+m)) {
        logW("out of ADPCM-B %d memory for sample %d!",m+1,i);
        break;
      }
      if (memPos+paddedLen>=getSampleMemCapacity(1+m)) {
        memcpy(adpcmBMem+memPos,s->dataB,getSampleMemCapacity(1+m)-memPos);
        logW("out of ADPCM-B %d memory for sample %d!",m+1,i);
      } else {
        memcpy(adpcmBMem+memPos,s->dataB,paddedLen);
        sampleLoaded[1+m][i]=true;
      }
      sampleOffB[m][i]=memPos;
      memCompoB[m].entries.push_back(DivMemoryEntry(DIV_MEMORY_SAMPLE,"Sample",i,memPos,memPos+paddedLen));
      memPos+=paddedLen;
    }
    adpcmBMemLen[m]=memPos+256;

    memCompoB[m].used=adpcmBMemLen[m];
    memCompoB[m].capacity=getSampleMemCapacity(1+m);
  }
}

int DivPlatformYM2609::init(DivEngine* p, int channels, int sugRate, const DivConfig& flags) {
  parent=p;
  dumpWrites=false;
  skipRegisterWrites=false;
  writeOscBuf=0;

  output_buf = new int*[2];

  output_buf[0] = new int[1];
  output_buf[1] = new int[1];

  adpcma_buf = new uint8_t[0x1000000];
  adpcma_buf_size = 0x1000000;

  sampleOffA=new unsigned int[32768];
  sampleOffB[0]=new unsigned int[32768];
  sampleOffB[1]=new unsigned int[32768];
  sampleOffB[2]=new unsigned int[32768];
  sampleLoaded[0]=new bool[32768];
  sampleLoaded[1]=new bool[32768];
  sampleLoaded[2]=new bool[32768];
  sampleLoaded[3]=new bool[32768];
  
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

  ym2609->Init(YM2609_CLOCK, YM2609_DSP_RATE, false, adpcma_buf, adpcma_buf_size);

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

  //delete[] adpcma_buf;
  adpcma_buf_size = 0;
}

DivPlatformYM2609::~DivPlatformYM2609() {
  delete[] sampleOffA;
  delete[] sampleOffB[0];
  delete[] sampleOffB[1];
  delete[] sampleOffB[2];
  delete[] sampleLoaded[0];
  delete[] sampleLoaded[1];
  delete[] sampleLoaded[2];
  delete[] sampleLoaded[3];
}
