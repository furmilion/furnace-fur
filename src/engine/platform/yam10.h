/**
 * Furnace platform wrapper for the YAM10.
 */

#ifndef _YAM10_PLATFORM_H
#define _YAM10_PLATFORM_H

#include "../dispatch.h"
#include "../../fixedQueue.h"
#include "sound/yam10.h"

void yam10ApplyInsToParam(YAM10ChanParam& dst, const DivInstrumentYAM10& src, unsigned char vol, bool muted, DivEngine* song, std::vector<int>* waveStore);

class DivPlatformYAM10: public DivDispatch {
  struct Channel: public SharedChannel {
    DivInstrumentYAM10 state;
    unsigned char eqSel;       // the band AFxx picked, indexes eqBand
    Channel(bool linear=true):
      SharedChannel(127,linear), eqSel(0) {}
  };
  Channel chan[YAM10_CHANS];
  DivDispatchOscBuffer* oscBuf[YAM10_CHANS];
  bool isMuted[YAM10_CHANS];
  // our own copy of any custom wavetable. deleting a wavetable does not
  // notify the chip, so pointing straight at song data would dangle.
  std::vector<int> waveCopy[YAM10_CHANS][YAM10_OPS];

  YAM10Chip chip;
  DivPitchTable pitchTable;
  unsigned char regPool[256];

  void applyInstrument(int ch);
  friend void putDispatchChip(void*,int);
  friend void putDispatchChan(void*,int,int);

  public:
    int dispatch(DivCommand c);
    SharedChannel* getChanState(int chan);
    DivDispatchOscBuffer* getOscBuffer(int chan);
    unsigned char* getRegisterPool();
    int getRegisterPoolSize();
    void reset();
    void forceIns();
    void tick(bool sysTick=true);
    void muteChannel(int ch, bool mute);
    int getOutputCount();
    void notifyInsChange(int ins);
    void notifyInsDeletion(void* ins);
    void notifyWaveChange(int wave);
    void setFlags(const DivConfig& flags);
    void poke(unsigned int addr, unsigned short val);
    void poke(std::vector<DivRegWrite>& wlist);
    const char** getRegisterSheet();
    void acquire(short** buf, size_t len);
    int init(DivEngine* parent, int channels, int sugRate, const DivConfig& flags);
    void quit();
    ~DivPlatformYAM10();
};

#endif
