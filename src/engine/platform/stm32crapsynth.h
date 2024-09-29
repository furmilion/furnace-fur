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

#ifndef _STM32CRAPSYNTH_H
#define _STM32CRAPSYNTH_H

#include "../dispatch.h"
#include "../../fixedQueue.h"
#include "../waveSynth.h"
#include "sound/crapsynth.h"

class DivPlatformSTM32CRAPSYNTH: public DivDispatch {
  struct Channel: public SharedChannel<signed short> {
    int wave;
    bool pcm;
    int wavetable;
    int dacSample;
    DivWaveSynth ws;
    Channel():
      SharedChannel<signed short>(255),
      wave(-1),
      pcm(false),
      wavetable(-1),
      dacSample(-1) {}
  };
  Channel chan[STM32CRAPSYNTH_NUM_CHANNELS];
  DivDispatchOscBuffer* oscBuf[STM32CRAPSYNTH_NUM_CHANNELS];
  bool isMuted[STM32CRAPSYNTH_NUM_CHANNELS];
  struct QueuedWrite {
    unsigned int addr;
    unsigned int val;
    QueuedWrite(): addr(0), val(0) {}
    QueuedWrite(unsigned int a, unsigned int v): addr(a), val(v) {}
  };
  FixedQueue<QueuedWrite,16384> writes;

  STM32CrapSynth* crap_synth;
  unsigned int regPool[128];
  unsigned int writeOscBuf;
  void updateWave(int ch);
  friend void putDispatchChip(void*,int);
  friend void putDispatchChan(void*,int,int);
  public:
    void acquire(short** buf, size_t len);
    int dispatch(DivCommand c);
    void* getChanState(int chan);
    DivMacroInt* getChanMacroInt(int ch);
    void getPaired(int ch, std::vector<DivChannelPair>& ret);
    DivChannelModeHints getModeHints(int chan);
    DivSamplePos getSamplePos(int ch);
    DivDispatchOscBuffer* getOscBuffer(int chan);
    int mapVelocity(int ch, float vel);
    float getGain(int ch, int vol);
    unsigned char* getRegisterPool();
    int getRegisterPoolSize();
    int getRegisterPoolDepth();
    void reset();
    void forceIns();
    void tick(bool sysTick=true);
    void muteChannel(int ch, bool mute);
    int getOutputCount();
    bool keyOffAffectsArp(int ch);
    void setFlags(const DivConfig& flags);
    void notifyWaveChange(int wave);
    void notifyInsDeletion(void* ins);
    void poke(unsigned int addr, unsigned short val);
    void poke(std::vector<DivRegWrite>& wlist);
    const char** getRegisterSheet();
    int init(DivEngine* parent, int channels, int sugRate, const DivConfig& flags);
    void quit();
    ~DivPlatformSTM32CRAPSYNTH();
};

#endif
