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

#ifndef _GP_H
#define _GP_H

#include "../dispatch.h"
#include "sound/gp/gp_pcm.h"

// the chip walks 32 register slots per pass. the last four hold the reverb and
// chorus state, which leaves 28 for voices, same as a real SC-55.
#define GP_VOICES 28
#define GP_CYCLES_PER_PASS ((GP_VOICES+1)*25)

class DivPlatformGP: public DivDispatch {
  public:
    struct Channel: public SharedChannel {
      unsigned int audPos;
      int sample;
      int panL, panR;
      // 0 idle, 2 sounding, 3 releasing. there is no user facing envelope, the
      // release exists only so note off is not a click.
      int tvaStage;
      bool setPos, reverse, bidir;
      int macroVolMul;
      Channel(bool linear=true):
        SharedChannel(127,linear),
        audPos(0),
        sample(-1),
        panL(32),
        panR(32),
        tvaStage(0),
        setPos(false),
        reverse(false),
        bidir(false),
        macroVolMul(127) {}
    };

  private:
    Channel chan[GP_VOICES];
    DivDispatchOscBuffer* oscBuf[GP_VOICES];
    bool isMuted[GP_VOICES];
    DivPitchTableManager samplePitchTable;

    unsigned int* sampleOff;
    unsigned int* sampleLen;
    bool* sampleLoaded;
    unsigned char* sampleMem;
    size_t sampleMemLen;

    gppcm_t pcm;
    DivMemoryComposition memCompo;
    unsigned char regPool[64];

    unsigned int voiceMask;
    bool isMk1, oversample;

    // the chip hands back one or two frames per pass, so they queue up here
    int32_t fifo[2][2];
    int fifoLen, fifoPos;

    void writeRam1(int slot, int index, unsigned int val);
    void writeRam2(int slot, int index, unsigned short val);
    void updateEnvelope(int ch);
    void keyOn(int ch);
    void keyOff(int ch);
    void updatePanning(int ch);

    friend void putDispatchChip(void*,int);
    friend void putDispatchChan(void*,int,int);

  public:
    static uint8_t romReadStatic(void* user, uint32_t address);
    uint8_t romRead(uint32_t address);

    virtual void acquire(short** buf, size_t len) override;
    virtual int dispatch(DivCommand c) override;
    virtual SharedChannel* getChanState(int chan) override;
    virtual DivMacroInt* getChanMacroInt(int ch) override;
    virtual unsigned short getPan(int chan) override;
    virtual DivDispatchOscBuffer* getOscBuffer(int chan) override;
    virtual unsigned char* getRegisterPool() override;
    virtual int getRegisterPoolSize() override;
    virtual void reset() override;
    virtual void forceIns() override;
    virtual void tick(bool sysTick=true) override;
    virtual void muteChannel(int ch, bool mute) override;
    virtual int getOutputCount() override;
    virtual bool hasSoftPan(int ch) override;
    virtual void notifyInsChange(int ins) override;
    virtual void notifyWaveChange(int wave) override;
    virtual void notifyInsDeletion(void* ins) override;
    virtual void notifyPitchTable(int sample=-1) override;
    virtual unsigned int getMaxFreq(int ch) override;
    virtual void setFlags(const DivConfig& flags) override;
    virtual void poke(unsigned int addr, unsigned short val) override;
    virtual void poke(std::vector<DivRegWrite>& wlist) override;
    virtual const char** getRegisterSheet() override;
    virtual const void* getSampleMem(int index = 0) override;
    virtual size_t getSampleMemCapacity(int index = 0) override;
    virtual size_t getSampleMemUsage(int index = 0) override;
    virtual size_t getSampleMemOffset(int index = 0) override;
    virtual bool isSampleLoaded(int index, int sample) override;
    virtual const DivMemoryComposition* getMemCompo(int index) override;
    virtual void renderSamples(int chipID) override;
    virtual int init(DivEngine* parent, int channels, int sugRate, const DivConfig& flags) override;
    virtual void quit() override;
    DivPlatformGP();
    ~DivPlatformGP();
};

#endif
