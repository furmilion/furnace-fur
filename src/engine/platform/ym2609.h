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
    unsigned char panLeft, panRight;
    bool gate;
    Channel():
      SharedChannel<signed short>(0xff),
      panLeft(0x3),
      panRight(0x3),
      gate(false) {}
  };
  Channel chan[YM2609_NUM_CHANNELS];
  DivDispatchOscBuffer* oscBuf[YM2609_NUM_CHANNELS];
  struct QueuedWrite {
      unsigned short addr;
      unsigned char val;
      QueuedWrite(): addr(0), val(0) {}
      QueuedWrite(unsigned short a, unsigned char v): addr(a), val(v) {}
  };
  FixedQueue<QueuedWrite,YM2609_NUM_REGISTERS * 4> writes;
  DivWaveSynth ws;

  unsigned char writeOscBuf;

  reverb* rev;
  distortion* dist;
  chorus* chor;
  eq3band* eq;
  HPFLPF* filt;
  ReversePhase* reph;
  Compressor* comp;

  OPNA2* ym2609;

  unsigned char regPool[YM2609_NUM_REGISTERS];

  bool isMuted[YM2609_NUM_CHANNELS];
  
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
    void quit();
    ~DivPlatformYM2609();
};

#endif
