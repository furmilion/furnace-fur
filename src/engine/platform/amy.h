/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2025 tildearrow and contributors
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

#ifndef _AMY_H
#define _AMY_H

#include "../dispatch.h"
#include "../../fixedQueue.h"
#include "../../../extern/AMYlator/amy.h"

#define AMY_REGPOOL_SIZE (4 * AMY_NUM_HARMONIC_OSCILLATORS + 8 * AMY_NUM_NOISE_GENERATORS + 8 * AMY_NUM_CHANNELS + 2) /*system options and system control*/
#define AMY_MAX_VOL 0xff

class DivPlatformAMY: public DivDispatch {
  struct Channel: public SharedChannel<signed short> {

    Channel():
      SharedChannel<signed short>(AMY_MAX_VOL)
       {}
  };
  Channel chan[AMY_NUM_CHANNELS];
  DivDispatchOscBuffer* oscBuf[AMY_NUM_CHANNELS];
  struct QueuedWrite {
      unsigned char addr;
      unsigned int val;
      QueuedWrite(): addr(0), val(0) {}
      QueuedWrite(unsigned char a, unsigned int v): addr(a), val(v) {}
  };
  FixedQueue<QueuedWrite,AMY_NUM_REGISTERS * (AMY_NUM_HARMONIC_OSCILLATORS + AMY_NUM_CHANNELS) * 4> writes;

  AMY* amy;
  unsigned char regPool[AMY_REGPOOL_SIZE];

  bool isMuted[AMY_NUM_CHANNELS];

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
    float getPostAmp();
    bool getDCOffRequired();
    DivMacroInt* getChanMacroInt(int ch);
    void notifyInsDeletion(void* ins);
    void poke(unsigned int addr, unsigned short val);
    void poke(std::vector<DivRegWrite>& wlist);
    const char** getRegisterSheet();
    int init(DivEngine* parent, int channels, int sugRate, const DivConfig& flags);
    int getOutputCount();
    void quit();
    ~DivPlatformAMY();
};

#endif
