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

#include "gp.h"
#include "../engine.h"
#include "../../ta-log.h"
#include <math.h>

// the register file is 64 bytes, and which voice a write lands on depends on
// the channel select at 0x3e
#define rWrite(a,v) {if(!skipRegisterWrites) {GPPCM_Write(pcm,a,v); regPool[(a)&0x3f]=v; if(dumpWrites) addWrite(a,v);}}

// one increment of 0x4000 plays a sample back at the chip's own pass rate.
// this lines the chip up with the rest of Furnace's sample chips, where a
// sample plays at its own rate four octaves above the internal base note.
#define CHIP_FREQBASE (16384*GP_CYCLES_PER_PASS)


// the mix accumulator is 20 bits and wraps rather than clips, so the only way
// to be sure it never tears is to keep every voice small enough that all 28 of
// them together still fit. 128 would be unity for one voice; a 32nd of that
// leaves 28 voices summing to about seven eighths of full scale.
#define GP_VOICE_LEVEL 4

// and then some of the gain comes back on the way out. a shift of 16 would be
// the plain conversion from the 20-bit accumulator; 13 is three bits hotter
// than that, which puts one voice at about a fifth of full scale. that is
// roughly where Furnace's other sample chips sit, so a chord behaves the way
// you would expect rather than hitting the limiter on the second note.
#define GP_OUTPUT_SHIFT 13

// one block to walk the accumulator back to zero, two blocks of silence for a
// one-shot to sit on once it has finished
#define GP_TAIL_LEN 16
#define GP_QUIET_LEN 32

// each 1MB bank keeps its block shift codes in its first 32K, exactly the way
// a real wave ROM is laid out
#define GP_BANK_SIZE 0x100000
#define GP_BANK_COUNT 8
#define GP_CODE_TABLE_SIZE 0x8000

const char* regCheatSheetGP[]={
  "VoiceMask", "00",
  "WaveAddr", "20",
  "Config", "3C",
  "Slots", "3D",
  "ChanSel", "3E",
  "WaveData", "3F",
  "CHx_Addr", "04",
  "CHx_Loop", "08",
  "CHx_End", "0C",
  "CHx_Pitch", "10",
  "CHx_Pan", "12",
  "CHx_Send", "14",
  "CHx_TVA0", "16",
  "CHx_TVA1", "18",
  "CHx_TVF", "1A",
  "CHx_Mode", "1C",
  "CHx_Flags", "1E",
  "CHx_Ref", "24",
  "CHx_FiltB", "28",
  "CHx_FiltA", "2C",
  "CHx_Phase", "30",
  "CHx_TVA0Lvl", "32",
  "CHx_TVA1Lvl", "34",
  "CHx_TVFLvl", "36",
  NULL
};

// which register file address each ram1 slot is reached through
static const unsigned int ram1Addr[6]={0x0c,0x2c,0x08,0x28,0x04,0x24};

// measured off the chip itself: envelope speed bytes ordered slowest to
// fastest, spread over roughly 19 seconds down to half a millisecond. the
// register encoding is not remotely linear, hence the table.
// the chip takes a speed byte rather than a time. these two are all that is
// needed now that there is no user facing envelope: one that moves the level
// in a single step, and one that takes a few milliseconds so note off is not
// a click. both measured off the chip.
static const unsigned char gpEnvInstant=0xac;
static const unsigned char gpEnvRelease=0x80;

// same shift ladder the encoder in sample.cpp uses
static const double fceStep[16]={
  1.0,2.0,4.0,8.0,16.0,32.0,64.0,128.0,256.0,512.0,1024.0,
  1.0/32.0,1.0/16.0,1.0/8.0,1.0/4.0,1.0/2.0
};

static inline int fceSx20(int v) {
  return (v<<12)>>12;
}

static inline int fceRefStep(signed char d, int code) {
  int shift=(10-code)&15;
  int scaled=((((int)d)<<10)<<1)>>shift;
  return (scaled>>1)+(scaled&1);
}

const char** DivPlatformGP::getRegisterSheet() {
  return regCheatSheetGP;
}

uint8_t DivPlatformGP::romReadStatic(void* user, uint32_t address) {
  return ((DivPlatformGP*)user)->romRead(address);
}

uint8_t DivPlatformGP::romRead(uint32_t address) {
  if (sampleMem==NULL) return 0;
  if (address>=getSampleMemCapacity()) return 0;
  return sampleMem[address];
}

void DivPlatformGP::writeRam1(int slot, int index, unsigned int val) {
  if (index<0 || index>5) return;
  rWrite(0x3e,slot&0x1f);
  unsigned int b=ram1Addr[index];
  rWrite(b+1,(val>>16)&0x0f);
  rWrite(b+2,(val>>8)&0xff);
  rWrite(b+3,val&0xff);
}

void DivPlatformGP::writeRam2(int slot, int index, unsigned short val) {
  if (index<0 || index>11) return;
  rWrite(0x3e,slot&0x1f);
  unsigned int b=(index<8)?(0x10+index*2):(0x30+(index-8)*2);
  rWrite(b,(val>>8)&0xff);
  rWrite(b+1,val&0xff);
}

void DivPlatformGP::acquire(short** buf, size_t len) {
  for (int i=0; i<GP_VOICES; i++) {
    oscBuf[i]->begin(len);
  }

  for (size_t h=0; h<len; h++) {
    if (fifoPos>=fifoLen) {
      GPPCM_Tick(pcm);
      fifoLen=pcm.out_count;
      fifoPos=0;
      for (int i=0; i<fifoLen; i++) {
        fifo[i][0]=pcm.out[i][0];
        fifo[i][1]=pcm.out[i][1];
      }
    }
    // the core taps each voice before panning, so muting a channel has to be
    // applied here as well or its scope keeps drawing
    for (int i=0; i<GP_VOICES; i++) {
      oscBuf[i]->putSample(h,isMuted[i]?0:CLAMP(pcm.voice_out[i]>>4,-32768,32767));
    }

    int lout=0;
    int rout=0;
    if (fifoPos<fifoLen) {
      lout=fifo[fifoPos][0]>>GP_OUTPUT_SHIFT;
      rout=fifo[fifoPos][1]>>GP_OUTPUT_SHIFT;
      fifoPos++;
    }
    buf[0][h]=CLAMP(lout,-32768,32767);
    buf[1][h]=CLAMP(rout,-32768,32767);
  }

  for (int i=0; i<GP_VOICES; i++) {
    oscBuf[i]->end(len);
  }
}

void DivPlatformGP::updatePanning(int ch) {
  int l=isMuted[ch]?0:chan[ch].panL;
  int r=isMuted[ch]?0:chan[ch].panR;
  writeRam2(ch,1,((l&0x3f)<<8)|(r&0x3f));
}

// the chip only ever heads towards one level at one speed, so the stages get
// stepped along here the way the SC-55 firmware does it
void DivPlatformGP::updateEnvelope(int ch) {
  Channel& c=chan[ch];
  if (c.tvaStage!=3) return;
  // envelope 1, not envelope 0. both head for a target at a speed, but only
  // envelope 1 checks whether it has overshot before it writes the volume
  // multiplier. envelope 0 wraps past zero and comes back out at full scale,
  // which is a loud click at the end of every release.
  if ((pcm.ram2[ch][10]&0x7fff)<0x40) {
    c.tvaStage=0;
    voiceMask&=~(1u<<ch);
    rWrite(0x00,(voiceMask>>24)&0x0f);
    rWrite(0x01,(voiceMask>>16)&0xff);
    rWrite(0x02,(voiceMask>>8)&0xff);
    rWrite(0x03,voiceMask&0xff);
  }
}

void DivPlatformGP::keyOn(int ch) {
  Channel& c=chan[ch];
  DivSample* s=NULL;
  if (c.sample>=0 && c.sample<parent->song.sampleLen) {
    s=parent->getSample(c.sample);
  }
  if (s==NULL || !sampleLoaded[c.sample] || sampleLen[c.sample]==0) {
    keyOff(ch);
    return;
  }

  unsigned int start=sampleOff[c.sample];
  unsigned int len=sampleLen[c.sample];
  unsigned int bank=start>>20;
  unsigned int base=start&0xfffff;
  unsigned int end=base+len-1;
  unsigned int loop=base;

  if (s->isLoopable()) {
    loop=base+MAX(0,s->loopStart);
    end=base+MIN((int)len-1,MAX(1,s->loopEnd)-1);
  } else {
    // one-shots park on the silent block the renderer appended
    loop=base+len-GP_QUIET_LEN;
    end=base+len-1;
  }
  if (c.audPos>0) {
    base=MIN(base+(unsigned int)c.audPos,end);
  }

  writeRam1(ch,0,end&0xfffff);
  writeRam1(ch,2,loop&0xfffff);
  writeRam1(ch,4,base&0xfffff);
  writeRam1(ch,1,0);
  writeRam1(ch,3,0);
  writeRam1(ch,5,0);

  writeRam2(ch,0,CLAMP(c.freq,0,0xffff));
  updatePanning(ch);
  writeRam2(ch,2,0); // no effect sends, the onboard DSP is not used

  // envelope 1 carries the level and envelope 0 the channel volume. 128 is
  // unity for one voice, and the mix accumulator is only 20 bits and wraps
  // rather than clips, so the level is pulled down to leave room for chords.
  c.tvaStage=2;
  writeRam2(ch,4,(GP_VOICE_LEVEL<<8)|gpEnvInstant);
  writeRam2(ch,3,((c.outVol&0x7f)<<8)|gpEnvInstant);

  // the per voice filter is bypassed in the core, so these are left alone
  writeRam2(ch,5,0);
  writeRam2(ch,6,0);

  unsigned short flags=(unsigned short)(ch&0x1f);
  if (c.bidir) flags|=0x40;
  if (c.reverse) flags|=0x80;
  flags|=(unsigned short)((bank&0x0f)<<8);
  writeRam2(ch,7,flags);
  writeRam2(ch,8,0);
  writeRam2(ch,9,0);
  writeRam2(ch,10,0);
  writeRam2(ch,11,0);

  voiceMask|=(1u<<ch);
  rWrite(0x00,(voiceMask>>24)&0x0f);
  rWrite(0x01,(voiceMask>>16)&0xff);
  rWrite(0x02,(voiceMask>>8)&0xff);
  rWrite(0x03,voiceMask&0xff);
  GPPCM_Read(pcm,0x00); // latches the pending mask, which is the actual key on
}

void DivPlatformGP::keyOff(int ch) {
  Channel& c=chan[ch];
  if (c.tvaStage==0) return;
  // hand it to the release stage and let updateEnvelope cut the voice
  c.tvaStage=3;
  writeRam2(ch,4,gpEnvRelease);
}

void DivPlatformGP::tick(bool sysTick) {
  for (int i=0; i<GP_VOICES; i++) {
    Channel& c=chan[i];
    c.std.next();

    if (c.std.vol.had) {
      c.outVol=((c.vol&0x7f)*MIN(c.macroVolMul,c.std.vol.val))/c.macroVolMul;
      if (c.tvaStage!=0) {
        writeRam2(i,3,((c.outVol&0x7f)<<8)|gpEnvInstant);
      }
    }
    if (NEW_ARP_STRAT) {
      c.handleArp();
    } else if (c.std.arp.had && !c.rawFreq) {
      if (!c.inPorta) {
        c.baseFreq=c.calcBaseFreq(parent->calcArp(c.note,c.std.arp.val));
      }
      c.freqChanged=true;
    }
    if (c.std.pitch.had) {
      if (c.std.pitch.mode) {
        c.pitch2+=c.std.pitch.val;
        CLAMP_VAR(c.pitch2,-32768,32767);
      } else {
        c.pitch2=c.std.pitch.val;
      }
      c.freqChanged=true;
    }
    if (c.std.panL.had) {
      c.panL=(c.std.panL.val&0x7f)>>1;
      updatePanning(i);
    }
    if (c.std.panR.had) {
      c.panR=(c.std.panR.val&0x7f)>>1;
      updatePanning(i);
    }
    if (c.std.phaseReset.had) {
      if (c.std.phaseReset.val==1 && c.active) {
        c.audPos=0;
        c.setPos=true;
      }
    }

    if (c.setPos) {
      c.keyOn=true;
      c.setPos=false;
    } else {
      c.audPos=0;
    }

    if (c.keyOff) {
      keyOff(i);
      c.keyOff=false;
    }
    if (c.keyOn) {
      if (c.freqChanged) {
        c.freq=c.calcFreq();
        c.freqChanged=false;
      }
      keyOn(i);
      c.keyOn=false;
    } else if (c.freqChanged) {
      c.freq=c.calcFreq();
      writeRam2(i,0,CLAMP(c.freq,0,0xffff));
      c.freqChanged=false;
    }

    updateEnvelope(i);
  }
}

int DivPlatformGP::dispatch(DivCommand c) {
  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_AMIGA);
      chan[c.chan].macroVolMul=(ins->type==DIV_INS_AMIGA)?64:127;
      if (c.value!=DIV_NOTE_NULL) {
        chan[c.chan].sample=ins->amiga.getSample(c.value);
        chan[c.chan].pitchTable=samplePitchTable.get(chan[c.chan].sample);
        chan[c.chan].sampleNote=c.value;
        c.value=ins->amiga.getFreq(c.value);
        chan[c.chan].sampleNoteDelta=c.value-chan[c.chan].sampleNote;
      }
      // the chip can bounce a loop back and forth in hardware, so take that
      // from the sample. straight reverse is not wired up: the address layout
      // keyOn builds only ever counts upwards.
      if (chan[c.chan].sample>=0 && chan[c.chan].sample<parent->song.sampleLen) {
        DivSample* s=parent->getSample(chan[c.chan].sample);
        chan[c.chan].bidir=(s->loopMode==DIV_SAMPLE_LOOP_PINGPONG);
      }
      if (c.value!=DIV_NOTE_NULL) {
        chan[c.chan].baseFreq=chan[c.chan].calcBaseFreq(c.value);
      }
      if (chan[c.chan].sample<0 || chan[c.chan].sample>=parent->song.sampleLen) {
        chan[c.chan].sample=-1;
      }
      if (c.value!=DIV_NOTE_NULL) {
        chan[c.chan].freqChanged=true;
        chan[c.chan].note=c.value;
      }
      chan[c.chan].active=true;
      chan[c.chan].keyOn=true;
      chan[c.chan].macroInit(ins);
      if (!parent->song.compatFlags.brokenOutVol && !chan[c.chan].std.vol.will) {
        chan[c.chan].outVol=chan[c.chan].vol;
      }
      break;
    }
    case DIV_CMD_NOTE_OFF:
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
      }
      break;
    case DIV_CMD_VOLUME:
      if (chan[c.chan].vol!=c.value) {
        chan[c.chan].vol=c.value;
        if (!chan[c.chan].std.vol.has) {
          chan[c.chan].outVol=c.value;
          if (chan[c.chan].tvaStage!=0) {
            writeRam2(c.chan,3,((chan[c.chan].outVol&0x7f)<<8)|gpEnvInstant);
          }
        }
      }
      break;
    case DIV_CMD_GET_VOLUME:
      if (chan[c.chan].std.vol.has) {
        return chan[c.chan].vol;
      }
      return chan[c.chan].outVol;
      break;
    case DIV_CMD_PANNING:
      chan[c.chan].panL=c.value>>2;
      chan[c.chan].panR=c.value2>>2;
      updatePanning(c.chan);
      break;
    case DIV_CMD_PITCH:
      chan[c.chan].pitch=c.value;
      chan[c.chan].freqChanged=true;
      break;
    case DIV_CMD_NOTE_PORTA: {
      int destFreq=chan[c.chan].calcBaseFreq(c.value2+chan[c.chan].sampleNoteDelta);
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
    case DIV_CMD_LEGATO: {
      chan[c.chan].baseFreq=chan[c.chan].calcBaseFreq(c.value+chan[c.chan].sampleNoteDelta+((HACKY_LEGATO_MESS)?(chan[c.chan].std.arp.val):(0)));
      chan[c.chan].freqChanged=true;
      chan[c.chan].note=c.value;
      break;
    }
    case DIV_CMD_PRE_PORTA:
      if (chan[c.chan].active && c.value2) {
        if (parent->song.compatFlags.resetMacroOnPorta) chan[c.chan].macroInit(parent->getIns(chan[c.chan].ins,DIV_INS_AMIGA));
      }
      if (!chan[c.chan].inPorta && c.value && !parent->song.compatFlags.brokenPortaArp && chan[c.chan].std.arp.will && !NEW_ARP_STRAT) chan[c.chan].baseFreq=chan[c.chan].calcBaseFreq(chan[c.chan].note);
      chan[c.chan].inPorta=c.value;
      break;
    case DIV_CMD_SAMPLE_POS:
      chan[c.chan].audPos=c.value;
      chan[c.chan].setPos=true;
      break;
    case DIV_CMD_GET_VOLMAX:
      return 127;
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

void DivPlatformGP::muteChannel(int ch, bool mute) {
  isMuted[ch]=mute;
  updatePanning(ch);
}

void DivPlatformGP::forceIns() {
  for (int i=0; i<GP_VOICES; i++) {
    chan[i].insChanged=true;
    chan[i].freqChanged=true;
    chan[i].sample=-1;
    updatePanning(i);
  }
}

SharedChannel* DivPlatformGP::getChanState(int ch) {
  return &chan[ch];
}

DivMacroInt* DivPlatformGP::getChanMacroInt(int ch) {
  return &chan[ch].std;
}

unsigned short DivPlatformGP::getPan(int ch) {
  return (chan[ch].panL<<10)|(chan[ch].panR<<2);
}

DivDispatchOscBuffer* DivPlatformGP::getOscBuffer(int ch) {
  return oscBuf[ch];
}

int DivPlatformGP::getOutputCount() {
  return 2;
}

bool DivPlatformGP::hasSoftPan(int ch) {
  return true;
}

void DivPlatformGP::notifyInsChange(int ins) {
  for (int i=0; i<GP_VOICES; i++) {
    if (chan[i].ins==ins) {
      chan[i].insChanged=true;
    }
  }
}

void DivPlatformGP::notifyWaveChange(int wave) {
}

void DivPlatformGP::notifyInsDeletion(void* ins) {
  for (int i=0; i<GP_VOICES; i++) {
    chan[i].std.notifyInsDeletion((DivInstrument*)ins);
  }
}

void DivPlatformGP::notifyPitchTable(int sample) {
  samplePitchTable.update<Channel>(chan,GP_VOICES,parent->song.tuning,chipClock,CHIP_FREQBASE,0xffff,false,parent->song.compatFlags.linearPitch,sample);
}

unsigned int DivPlatformGP::getMaxFreq(int ch) {
  return 0xffff;
}

// the block shift codes live in the first 32K of the bank the sample sits in,
// one nibble per 16 samples, the odd block of each pair in the high half
static inline void fcePutCode(unsigned char* mem, unsigned int addr, int code) {
  unsigned int bankBase=addr&0xf00000;
  unsigned int block=(addr&0xfffff)>>4;
  unsigned int at=bankBase|(block>>1);
  if (block&1) {
    mem[at]=(unsigned char)((mem[at]&0x0f)|(code<<4));
  } else {
    mem[at]=(unsigned char)((mem[at]&0xf0)|code);
  }
}

static inline int fceReadCode(const unsigned char* codes, unsigned int block) {
  unsigned char b=codes[block>>1];
  return (block&1)?((b>>4)&15):(b&15);
}

void DivPlatformGP::renderSamples(int sysID) {
  memset(sampleMem,0,getSampleMemCapacity());
  memset(sampleOff,0,32768*sizeof(unsigned int));
  memset(sampleLen,0,32768*sizeof(unsigned int));
  memset(sampleLoaded,0,32768*sizeof(bool));

  memCompo=DivMemoryComposition();
  memCompo.name="Wave ROM";

  // every bank keeps its first 32K for the shift codes
  unsigned int memPos=GP_CODE_TABLE_SIZE;

  for (int i=0; i<parent->song.sampleLen; i++) {
    DivSample* s=parent->song.sample[i];
    if (!s->renderOn[0][sysID]) {
      sampleOff[i]=0;
      continue;
    }
    unsigned int samples=s->samples;
    if (samples<1) {
      // an empty sample occupies nothing, which is not the same as failing
      // to fit. saying otherwise puts an out of memory warning on every
      // freshly created sample.
      sampleLoaded[i]=true;
      continue;
    }

    // the shift codes are shared a nibble at a time across 16-sample blocks,
    // so everything that follows the sample has to start on a block boundary.
    // otherwise the ramp below lands inside the sample's last block and
    // rewrites its code, which turns the end of the sample into noise.
    unsigned int padded=(samples+15)&~15u;
    unsigned int needed=padded+GP_TAIL_LEN+GP_QUIET_LEN;

    // a voice cannot step over a bank boundary, its address counter is 20 bits
    if (needed>GP_BANK_SIZE-GP_CODE_TABLE_SIZE) {
      logW("sample %d does not fit in a GP bank!",i);
      continue;
    }
    memPos=(memPos+31)&~31u;
    if (((memPos&0xfffff)+needed)>GP_BANK_SIZE) {
      memPos=((memPos&0xf00000)+GP_BANK_SIZE)|GP_CODE_TABLE_SIZE;
    }
    if (memPos+needed>getSampleMemCapacity()) {
      logW("out of GP wave memory for sample %d!",i);
      break;
    }

    const unsigned char* deltas=s->dataFCE;
    const unsigned char* codes=s->dataFCE+samples;
    if (deltas==NULL) {
      logW("sample %d was never rendered to the GP format!",i);
      continue;
    }

    sampleOff[i]=memPos;
    sampleLen[i]=needed;
    memCompo.entries.push_back(DivMemoryEntry(DIV_MEMORY_SAMPLE,"Sample",i,memPos,memPos+needed));

    int ref=0;
    for (unsigned int j=0; j<samples; j++) {
      sampleMem[memPos+j]=deltas[j];
      if ((j&15)==0) {
        fcePutCode(sampleMem,memPos+j,fceReadCode(codes,j>>4));
      }
      ref=fceSx20(ref+fceRefStep((signed char)deltas[j],fceReadCode(codes,j>>4)));
    }
    // pad out to the block boundary with deltas that do nothing
    for (unsigned int j=samples; j<padded; j++) {
      sampleMem[memPos+j]=0;
    }

    // walk the accumulator back to zero, otherwise a sample that stops away
    // from zero leaves a DC step sitting in the mix for as long as it is held
    unsigned int tailAt=memPos+padded;
    int tailCode=10;
    static const int codeOrder[16]={11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10};
    for (int cand=0; cand<16; cand++) {
      int cc=codeOrder[cand];
      if (fceStep[cc]*127.0*(double)GP_TAIL_LEN>=fabs((double)ref)) {
        tailCode=cc;
        break;
      }
    }
    fcePutCode(sampleMem,tailAt,tailCode);
    for (unsigned int j=0; j<GP_TAIL_LEN; j++) {
      double want=(0.0-(double)ref)/(double)(GP_TAIL_LEN-j)/fceStep[tailCode];
      int q=(int)((want<0.0)?(want-0.5):(want+0.5));
      if (q>127) q=127;
      if (q<-128) q=-128;
      sampleMem[tailAt+j]=(unsigned char)(q&0xff);
      ref=fceSx20(ref+fceRefStep((signed char)q,tailCode));
    }

    // the silent block a one-shot parks on, every block given a zero code
    unsigned int quietAt=tailAt+GP_TAIL_LEN;
    for (unsigned int j=0; j<GP_QUIET_LEN; j+=16) {
      fcePutCode(sampleMem,quietAt+j,0);
    }
    for (unsigned int j=0; j<GP_QUIET_LEN; j++) {
      sampleMem[quietAt+j]=0;
    }

    sampleLoaded[i]=true;
    memPos+=needed;
  }

  sampleMemLen=memPos;
  memCompo.capacity=getSampleMemCapacity();
  memCompo.used=sampleMemLen;
}

const void* DivPlatformGP::getSampleMem(int index) {
  return (index==0)?sampleMem:NULL;
}

size_t DivPlatformGP::getSampleMemCapacity(int index) {
  return (index==0)?(GP_BANK_SIZE*GP_BANK_COUNT):0;
}

size_t DivPlatformGP::getSampleMemUsage(int index) {
  return (index==0)?sampleMemLen:0;
}

size_t DivPlatformGP::getSampleMemOffset(int index) {
  return (index==0)?GP_CODE_TABLE_SIZE:0;
}

bool DivPlatformGP::isSampleLoaded(int index, int sample) {
  if (index!=0) return false;
  if (sample<0 || sample>32767) return false;
  return sampleLoaded[sample];
}

const DivMemoryComposition* DivPlatformGP::getMemCompo(int index) {
  if (index!=0) return NULL;
  return &memCompo;
}

void DivPlatformGP::reset() {
  memset(regPool,0,64);
  GPPCM_Reset(pcm);
  fifoLen=0;
  fifoPos=0;
  voiceMask=0;

  rWrite(0x3c,0xc3); // the config byte a real SC-55 writes
  rWrite(0x3d,0x20|(GP_VOICES-1));

  for (int i=0; i<GP_VOICES; i++) {
    chan[i]=DivPlatformGP::Channel(parent->song.compatFlags.linearPitch);
    chan[i].pitchTable=samplePitchTable.get(-1);
    chan[i].std.setEngine(parent);
    for (int j=0; j<6; j++) writeRam1(i,j,0);
    for (int j=0; j<12; j++) writeRam2(i,j,0);
    writeRam2(i,7,(unsigned short)i); // each voice reads its own pitch
    updatePanning(i);
  }

  rWrite(0x00,0);
  rWrite(0x01,0);
  rWrite(0x02,0);
  rWrite(0x03,0);
  GPPCM_Read(pcm,0x00);

}

void DivPlatformGP::setFlags(const DivConfig& flags) {
  int model=flags.getInt("clockSel",0);
  isMk1=(model==1);
  switch (model) {
    case 1: chipClock=23200000; break; // SC-55, CM-300, SCC-1
    default: chipClock=24000000; break; // SC-55mkII, SC-55ST
  }
  CHECK_CUSTOM_CLOCK;
  oversample=flags.getBool("oversampling",true);

  pcm.is_mk1=isMk1;
  pcm.enable_oversampling=oversample;
  // the per voice filter is left out of the signal path, this is a plain
  // sample chip
  pcm.filter_bypass=true;

  rate=chipClock/GP_CYCLES_PER_PASS;
  if (oversample) rate*=2;
  for (int i=0; i<GP_VOICES; i++) {
    oscBuf[i]->setRate(rate);
  }

  notifyPitchTable();
}

void DivPlatformGP::poke(unsigned int addr, unsigned short val) {
  rWrite(addr&0x3f,val);
}

void DivPlatformGP::poke(std::vector<DivRegWrite>& wlist) {
  for (DivRegWrite& i: wlist) rWrite(i.addr&0x3f,i.val);
}

unsigned char* DivPlatformGP::getRegisterPool() {
  return regPool;
}

int DivPlatformGP::getRegisterPoolSize() {
  return 64;
}

int DivPlatformGP::init(DivEngine* p, int channels, int sugRate, const DivConfig& flags) {
  parent=p;
  samplePitchTable.init(parent);
  dumpWrites=false;
  skipRegisterWrites=false;

  for (int i=0; i<GP_VOICES; i++) {
    isMuted[i]=false;
    oscBuf[i]=new DivDispatchOscBuffer;
  }
  sampleMem=new unsigned char[getSampleMemCapacity()];
  memset(sampleMem,0,getSampleMemCapacity());
  sampleMemLen=0;

  memset(&pcm,0,sizeof(pcm));
  GPPCM_Init(pcm,DivPlatformGP::romReadStatic,this);

  setFlags(flags);
  reset();

  return GP_VOICES;
}

void DivPlatformGP::quit() {
  delete[] sampleMem;
  for (int i=0; i<GP_VOICES; i++) {
    delete oscBuf[i];
  }
}

DivPlatformGP::DivPlatformGP():
  DivDispatch() {
  sampleOff=new unsigned int[32768];
  sampleLen=new unsigned int[32768];
  sampleLoaded=new bool[32768];
  sampleMem=NULL;
  sampleMemLen=0;
  fifoLen=0;
  fifoPos=0;
  voiceMask=0;
  isMk1=false;
  oversample=true;
  memset(&pcm,0,sizeof(pcm));
  memset(regPool,0,64);
}

DivPlatformGP::~DivPlatformGP() {
  delete[] sampleOff;
  delete[] sampleLen;
  delete[] sampleLoaded;
  samplePitchTable.destroy<Channel>(chan,GP_VOICES);
}
