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
    unsigned int timer_freq;
    unsigned short duty;
    unsigned int lfsr;

    unsigned char noise_tri_amp;

    int dacPeriod, dacRate, dacOut;
    unsigned int dacPos;
    int dacSample;
    bool do_wavetable;
    bool updateWave;

    bool sampleInRam;
    bool extNoiseClk;

    bool apply_sample;
    bool set_sample_pos;

    int sample_offset;

    DivWaveSynth ws;
    Channel():
      SharedChannel<signed short>(255),
      wave(-1),
      pcm(false),
      wavetable(-1),
      timer_freq(0),
      duty(0x7fff),
      lfsr(1),
      noise_tri_amp(0),
      dacPeriod(0),
      dacRate(0),
      dacOut(0),
      dacPos(0),
      dacSample(-1),
      do_wavetable(false),
      updateWave(false),
      sampleInRam(false),
      extNoiseClk(true),
      apply_sample(false),
      set_sample_pos(false),
      sample_offset(0) {}
  };
  Channel chan[STM32CRAPSYNTH_NUM_CHANNELS];
  DivDispatchOscBuffer* oscBuf[STM32CRAPSYNTH_NUM_CHANNELS];
  bool isMuted[STM32CRAPSYNTH_NUM_CHANNELS];

  //filter
  float w0_ceil_1;
  float Vhp;
  float Vbp;
  float Vlp;
  float _1024_div_Q;

  struct QueuedWrite {
    unsigned int addr;
    unsigned int val;
    QueuedWrite(): addr(0), val(0) {}
    QueuedWrite(unsigned int a, unsigned int v): addr(a), val(v) {}
  };
  FixedQueue<QueuedWrite,16384> writes;
  
  unsigned int regPool[8*11+8*3];
  unsigned int writeOscBuf;
  DivMemoryComposition sampleMemFlashCompo;
  DivMemoryComposition sampleMemRamCompo;
  unsigned int sampleOff[256];
  unsigned int sampleOffRam[256];
  bool sampleLoaded[2][256];
  size_t sampleMemLenFlash;
  size_t sampleMemLenRam;
  void updateWave(int ch);
  friend void putDispatchChip(void*,int);
  friend void putDispatchChan(void*,int,int);
  public:
    STM32CrapSynth* crap_synth;
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
    const void* getSampleMem(int index = 0);
    size_t getSampleMemCapacity(int index = 0);
    size_t getSampleMemUsage(int index = 0);
    bool isSampleLoaded(int index, int sample);
    const DivMemoryComposition* getMemCompo(int index);
    void renderSamples(int chipID);
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
