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

#ifndef _YM2609_H
#define _YM2609_H

#include "../dispatch.h"
#include "../../fixedQueue.h"
#include "../waveSynth.h"
#include "sound/ym2609/src/opna2.h"

#define YM2609_NUM_CHANNELS 39
#define YM2609_NUM_REGISTERS 0x400

class DivPlatformYM2609: public DivDispatch {
  struct Channel: public SharedChannel<signed short> {
    DivInstrumentFM state;
    DivInstrumentYM2609FM state_ym2609fm;
    unsigned char opMask;
    unsigned char panLeft, panRight; //soft pan
    unsigned char pan; //hard pan
    bool gate;
    bool opMaskChanged;
    bool ac_switch;
    int portaPauseFreq;
    unsigned char duty;

    struct Operator_YM2609 
    {
      unsigned char wave_type; //0-3, WT L + WT H
      DivWaveSynth ws;

      bool operator==(const Operator_YM2609& other);
      bool operator!=(const Operator_YM2609& other) {
        return !(*this==other);
      }
      Operator_YM2609():
        wave_type(0) {}
    } op_ym2609[4];

    struct PSGMode {
      // bit 4: timer FX
      // bit 3: DAC
      // bit 2: envelope
      // bit 1: noise
      // bit 0: tone
      unsigned char val;

      unsigned char getTone() {
        return (val&8)?0:(val&1);
      }

      unsigned char getNoise() {
        return (val&8)?0:(val&2);
      }

      unsigned char getEnvelope() {
        return (val&8)?0:(val&4);
      }

      unsigned char getTimerFX() {
        return (val&8)?0:(val&16);
      }

      PSGMode(unsigned char v=1):
        val(v) {}
    };
    PSGMode curPSGMode;
    PSGMode nextPSGMode;

    unsigned char autoEnvNum, autoEnvDen;
    unsigned short fixedFreq;
    unsigned short wavetable;

    Channel():
      SharedChannel<signed short>(0xff),
      opMask(15),
      panLeft(0), //0 = max vol, 3(FM/ADPCM-B) 7(PSG) = min vol
      panRight(0), //0 = max vol, 3(FM/ADPCM-B) 7(PSG) = min vol
      pan(3),
      gate(false),
      opMaskChanged(false),
      ac_switch(false),
      portaPauseFreq(0),
      duty(0)
      {
        for(int i = 0; i < 4; i++)
        {
          op_ym2609[i].wave_type = i;
        }
        curPSGMode.val = 0;
        nextPSGMode.val = 1;
        autoEnvNum = 0;
        autoEnvDen = 0;
        fixedFreq = 0;
      }
  };

  unsigned char psg_offset, rhythm_offset, adpcma_offset, adpcmb_offset;
  bool extMode;

  unsigned char ayEnvMode[4];
  unsigned short ayEnvPeriod[4];
  short ayEnvSlideLow[4];
  short ayEnvSlide[4];

  Channel chan[YM2609_NUM_CHANNELS];
  DivDispatchOscBuffer* oscBuf[YM2609_NUM_CHANNELS];
  struct QueuedWrite {
      unsigned short addr;
      unsigned char val;
      QueuedWrite(): addr(0), val(0) {}
      QueuedWrite(unsigned short a, unsigned char v): addr(a), val(v) {}
  };
  FixedQueue<QueuedWrite,YM2609_NUM_REGISTERS * 4> writes;
  //DivWaveSynth ws;

  int clocks_per_sample; //to make one reg write per clock cycle
  unsigned int fmDivBase;

  const unsigned short ADDR_MULT_DT=0x30;
  const unsigned short ADDR_TL=0x40;
  const unsigned short ADDR_RS_AR=0x50;
  const unsigned short ADDR_AM_DR=0x60;
  const unsigned short ADDR_DT2_D2R=0x70;
  const unsigned short ADDR_SL_RR=0x80;
  const unsigned short ADDR_SSG=0x90;
  const unsigned short ADDR_FREQ=0xa0;
  const unsigned short ADDR_FREQH=0xa4;
  const unsigned short ADDR_FB_ALG=0xb0;
  const unsigned short ADDR_LRAF=0xb4;

  const unsigned short chanOffs[12]={
    0x00, 0x01, 0x02, 0x100, 0x101, 0x102, 
    0x200, 0x201, 0x202, 0x300, 0x301, 0x302
  };

  const unsigned short opOffs[4]={
    0x00, 0x04, 0x08, 0x0c
  };

  const bool isOutput[8][4]={
    // 1     3     2    4
    {false,false,false,true},
    {false,false,false,true},
    {false,false,false,true},
    {false,false,false,true},
    {false,false,true ,true},
    {false,true ,true ,true},
    {false,true ,true ,true},
    {true ,true ,true ,true},
  };
  const unsigned char dtTable[8]={
    7,6,5,0,1,2,3,4
  };

  const int orderedOps[4]={
    0,2,1,3
  };

  const unsigned char konOffs[6]={
    0, 1, 2, 4, 5, 6
  };

  const unsigned short ssg_offsets[4] = { 0, 0x120, 0x200, 0x210 };

  double fmFreqBase;

  unsigned char writeOscBuf;

  reverb* rev;
  distortion* dist;
  chorus* chor;
  eq3band* eq;
  HPFLPF* filt;
  ReversePhase* reph;
  Compressor* comp;

  OPNA2* ym2609;

  uint8_t* adpcma_buf;
  uint32_t adpcma_buf_size;

  size_t adpcmAMemLen;
  size_t adpcmBMemLen[3];
  bool* sampleLoaded[4];

  unsigned int* sampleOffA;
  unsigned int* sampleOffB[3];

  unsigned char regPool[YM2609_NUM_REGISTERS];

  uint8_t lfoValue[2];

  bool isMuted[YM2609_NUM_CHANNELS];

  int** output_buf;

  unsigned char prescale;

  DivMemoryComposition memCompoA;
  DivMemoryComposition memCompoB[3];
  
  friend void putDispatchChip(void*,int);
  friend void putDispatchChan(void*,int,int);

  /*void updateFlags(int channel, bool gate);
  void updateFilter(int channel, int filter);
  void updateFreq(int channel);
  void updateNoiseFreq(int channel);
  void updateNoiseLFSRMask(int channel);
  void updateDuty(int channel);
  void updateEnvelope(int channel);
  void updatePanning(int channel);
  void updateWave();*/

  public:
    void acquire(short** buf, size_t len);
    void commitState(int ch, DivInstrument* ins);
    int dispatch(DivCommand c);
    void* getChanState(int chan);
    DivDispatchOscBuffer* getOscBuffer(int chan);
    unsigned char* getRegisterPool();
    int getRegisterPoolSize();
    void reset();
    void forceIns();
    void tick(bool sysTick=true);
    void muteChannel(int ch, bool mute);
    void setFlags(const DivConfig& flags);
    void notifyInsChange(int ins);
    void notifyWaveChange(int wave); 
    float getPostAmp();
    bool getDCOffRequired();
    unsigned short getPan(int chan);
    DivMacroInt* getChanMacroInt(int ch);
    DivChannelModeHints getModeHints(int chan);
    void notifyInsDeletion(void* ins);
    void poke(unsigned int addr, unsigned short val);
    void poke(std::vector<DivRegWrite>& wlist);
    const char** getRegisterSheet();
    int init(DivEngine* parent, int channels, int sugRate, const DivConfig& flags);
    int getOutputCount();
    bool hasSoftPan(int ch);
    void getPaired(int ch, std::vector<DivChannelPair>& ret);
    const DivMemoryComposition* getMemCompo(int index);
    void renderSamples(int sysID);
    const void* getSampleMem(int index);
    size_t getSampleMemCapacity(int index);
    const char* getSampleMemName(int index=0);
    size_t getSampleMemUsage(int index);
    bool isSampleLoaded(int index, int sample);
    void quit();
    ~DivPlatformYM2609();
};

#endif
