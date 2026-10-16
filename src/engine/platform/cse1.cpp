#include "cse1.h"

#include "cse1_sync_utils.h"
#include "../engine.h"

#define CHIP_FREQBASE (1<<27)

#define MEMORY_SIZE (16*1024*1024)

void DivPlatformCSE1::acquire(short** buf, size_t len) {
    for (int i = 0; i < chans; i++) {
        oscBuf[i]->begin(len);
    }

    for (size_t i = 0; i < len; i++) {
        chip.clock(waveTable);
        int out[2] = {};

        for (unsigned char j = 0; j < chans; j++) {
            if (!isMuted[j]) {
              oscBuf[j]->putSample(i, chip.CSE1_GET_SAMPLE(j));

              auto& chip_channel = chip.CHANNELS.CHANNEL[j];

              chip_channel.OUT.PITCH = static_cast<CSE1_PACKED::CSE1_DOUBLE_REG>(chan[j].freq);

              out[0] += chip.CHANNELS.OUTS_L[j];
              out[1] += chip.CHANNELS.OUTS_R[j];
            } else {
              oscBuf[j]->putSample(i, 0);
            }
        }

        if (out[0] < -32768) out[0] = -32768;
        if (out[0] > 32767) out[0] = 32767;
        buf[0][i] = out[0];

        if (out[1] < -32768) out[1] = -32768;
        if (out[1] > 32767) out[1] = 32767;
        buf[1][i] = out[1];
    }

    for (int i = 0; i < chans; i++) {
        oscBuf[i]->end(len);
    }
}

int DivPlatformCSE1::getOutputCount() {
    return 2;
}


void DivPlatformCSE1::muteChannel(int ch, bool mute) {
  isMuted[ch]=mute;
}

void DivPlatformCSE1::tick(bool sysTick) {
  for (unsigned char i=0; i<chans; i++) {
    chan[i].std.next();
    DivInstrument* ins = parent->getIns(chan[i].ins,DIV_INS_CSE1);
    auto scaleVolume = 1;
    switch (ins->type) {
      case DIV_INS_QSOUND: {
        scaleVolume = 0x0004; // 0x3fff
        break;
      }
      case DIV_INS_ES5506:
      case DIV_INS_AMIGA: {
        scaleVolume = 0x0202;
        break;
      }
      default: break;
    }
    if (chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff) {
      chan[i].freqChanged=false;
      chan[i].freq=chan[i].calcFreq();

      if (chan[i].keyOn) {
        for (auto& op : chip.CHANNELS.CHANNEL[i].OPS.OP)  {
          op.ENV_STATE.SET_ENV_ENUM(0);
        }
        chan[i].m_outL = 0xffff;
        chan[i].m_outR = 0xffff;
        chan[i].m_outA = 0xffff;
        chan[i].keyOn=false;
      }
      if (chan[i].keyOff) {
        for (auto& op : chip.CHANNELS.CHANNEL[i].OPS.OP)  {
          op.ENV_STATE.SET_ENV_ENUM(3);
        }
        chan[i].keyOff=false;
      }
    }
    if (NEW_ARP_STRAT) {
      chan[i].handleArp();
    } else if (chan[i].std.arp.had && !chan[i].rawFreq) {
      if (!chan[i].inPorta) {
        chan[i].baseFreq=chan[i].calcBaseFreq(parent->calcArp(chan[i].note,chan[i].std.arp.val));
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
    if (chan[i].std.panL.had) {
      const int val=chan[i].std.panL.val&0xffff;
      chan[i].m_outL = scaleVolume * (val < 0xffff ? val : 0x10000);
    }
    if (chan[i].std.panR.had) {
      const int val=chan[i].std.panR.val&0xffff;
      chan[i].m_outR = scaleVolume * (val < 0xffff ? val : 0x10000);
    }
    if (chan[i].std.vol.had) {
      const int val=chan[i].std.vol.val&0xffff;
      chan[i].m_outA = scaleVolume * (val < 0xffff ? val : 0x10000);
    }

    for (size_t opi = 0; opi < CSE1_OPER_NUMBER ; opi++) {
      switch (chan[i].state.instrument.op[opi].pitchMode) {
        case 2: {
          const CSE1_PACKED::CSE1_DOUBLE_REG freq = pitchTable.get(
            chan[i].baseFreq, chan[i].pitch+chan[i].state.instrument.op[opi].lpitch-32768, chan[i].pitch2
            );
          chip.CHANNELS.CHANNEL[i].OPS.OP[opi].PITCH = freq;
          break;
        }
        case 3: {
          const auto pT = samplePitchTable.get(chan[i].state.instrument.op[opi].sample_tables.sampleIndex);
          if (pT == nullptr) break;
          const CSE1_PACKED::CSE1_DOUBLE_REG freq = pT->get(
            chan[i].baseFreq, chan[i].pitch+chan[i].state.instrument.op[opi].lpitch-32768, chan[i].pitch2
          );
          chip.CHANNELS.CHANNEL[i].OPS.OP[opi].PITCH = freq;
          break;
        }
        default: break;
      }
    }

    const auto originOutL = static_cast<uint64_t>(chan[i].outL) * chan[i].m_outA>>16;
    const auto originOutR = static_cast<uint64_t>(chan[i].outR) * chan[i].m_outA>>16;
    const auto outLbuf = chan[i].vol < 0xff ? (originOutL*chan[i].m_outL>>16)*chan[i].vol>>8 : (originOutL*chan[i].m_outL>>16);
    const auto outRbuf = chan[i].vol < 0xff ? (originOutR*chan[i].m_outR>>16)*chan[i].vol>>8 : (originOutR*chan[i].m_outR>>16);
    chip.CHANNELS.CHANNEL[i].OUT.OUT_L = chan[i].outL2 < 0xff ? outLbuf * chan[i].outL2>>8 : outLbuf;
    chip.CHANNELS.CHANNEL[i].OUT.OUT_R= chan[i].outR2 < 0xff ? outRbuf * chan[i].outR2>>8 : outRbuf;
  }
}

SharedChannel* DivPlatformCSE1::getChanState(int ch) {
  return &chan[ch];
}

DivDispatchOscBuffer* DivPlatformCSE1::getOscBuffer(int ch) {
  return oscBuf[ch];
}

int DivPlatformCSE1::dispatch(DivCommand c) {
  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_CSE1);
      chan[c.chan].active=true;
      chan[c.chan].keyOn=true;
      {
        switch (ins->type) {
          case DIV_INS_YMZ280B:
          case DIV_INS_ES5506:
          case DIV_INS_QSOUND:
          case DIV_INS_AMIGA: {
            auto& amiga = ins->amiga;

            if (c.value!=DIV_NOTE_NULL) {
              chan[c.chan].sample=amiga.getSample(c.value);
              chan[c.chan].pitchTable=samplePitchTable.get(chan[c.chan].sample);
              chan[c.chan].sampleNote=c.value;
              c.value=amiga.getFreq(c.value);
              chan[c.chan].sampleNoteDelta=c.value-chan[c.chan].sampleNote;
            } else if (chan[c.chan].sampleNote!=DIV_NOTE_NULL) {
              chan[c.chan].sample=amiga.getSample(chan[c.chan].sampleNote);
              chan[c.chan].pitchTable=samplePitchTable.get(chan[c.chan].sample);
              c.value=amiga.getFreq(chan[c.chan].sampleNote);
            }

            if (c.value!=DIV_NOTE_NULL) {
              chan[c.chan].baseFreq=chan[c.chan].calcBaseFreq(c.value);
            }

            if (c.value!=DIV_NOTE_NULL) {
              chan[c.chan].freqChanged=true;
              chan[c.chan].note=c.value;
            }

            chan[c.chan].keyOn=true;
            chan[c.chan].insChanged=false;

            auto cse1 = DivInstrumentCSE1();

            DivSample* s = parent->getSample(chan[c.chan].sample);
            if (s && s->samples > 0) {
              for (int op = 0; op < 2; op++) {
                if (s->loop) {
                  cse1.op[op].startP = sampleOff[chan[c.chan].sample] + s->getLoopStartPosition(DIV_SAMPLE_DEPTH_16BIT)/2;
                  cse1.op[op].endP = sampleOff[chan[c.chan].sample] + s->getLoopEndPosition(DIV_SAMPLE_DEPTH_16BIT)/2-1;
                  cse1.op[op].phase = sampleOff[chan[c.chan].sample];
                  cse1.op[op].wave = CSE1_PACKED::OPER_LOOP_SAMPLE;
                } else {
                  cse1.op[op].startP = sampleOff[chan[c.chan].sample];
                  cse1.op[op].endP = sampleOff[chan[c.chan].sample] + (s->getCurBufLen() + 1) / 2-1;
                  cse1.op[op].phase = cse1.op[op].startP;
                  cse1.op[op].wave = CSE1_PACKED::OPER_ONESHOT_SAMPLE;
                }
                cse1.op[op].adsr.ar = 0xffff;
                cse1.op[op].adsr.rr = 0xffff;
                cse1.op[op].env_divider = 0x1;
                cse1.out.inLeft[op] = 0xffff;
                cse1.out.inRight[op] = 0xffff;
                cse1.out.outLeft = 0xffff;
                cse1.out.outRight = 0xffff;
              }
            }
            chan[c.chan].outL = cse1.out.outLeft;
            chan[c.chan].outR = cse1.out.outRight;
            this->chan[c.chan].state.instrument = cse1;
            CSE1_REG_INS_SYNC::ins_to_reg(&cse1, &chip.CHANNELS.CHANNEL[c.chan]);
            break;
          }

          case DIV_INS_CSE1:
          default: {
            auto cse1 = ins->cse1;
            chan[c.chan].pitchTable = &pitchTable;
            chan[c.chan].outL = cse1.out.outLeft;
            chan[c.chan].outR = cse1.out.outRight;
            if (c.value!=DIV_NOTE_NULL) {
              chan[c.chan].note = c.value;
              chan[c.chan].baseFreq=chan[c.chan].calcBaseFreq(c.value);
              chan[c.chan].freqChanged=true;
            }

            for (auto & op : cse1.op) {
              const uint16_t sampleIndex = op.sample_tables.sampleIndex;
              if (op.useSample && sampleIndex < parent->song.sampleLen) {
                DivSample* s = parent->getSample(sampleIndex);
                if (s && s->samples > 0) {
                  const uint32_t base = sampleOff[sampleIndex];
                  if (op.wave == CSE1_PACKED::WAVE_TABLE_TYPE::OPER_WAVETABLE_SAMPLE) {
                    op.startP = base;
                    continue;
                  }
                  if (s->loop) {
                    op.startP = base + s->getLoopStartPosition(DIV_SAMPLE_DEPTH_16BIT) / 2;
                    op.endP = base + s->getLoopEndPosition(DIV_SAMPLE_DEPTH_16BIT) / 2 - 1;
                    op.phase = base;
                  } else {
                    op.startP = base;
                    op.endP = base + (s->getCurBufLen() + 1) / 2 - 1;
                    op.phase = base;
                  }
                }
              }
            }

            this->chan[c.chan].state.instrument = cse1;
            CSE1_REG_INS_SYNC::ins_to_reg(&cse1, &chip.CHANNELS.CHANNEL[c.chan]);
            break;
          }
        }
        chan[c.chan].macroInit(ins);
      }
      break;
    }
    case DIV_CMD_NOTE_OFF_ENV:
    case DIV_CMD_ENV_RELEASE:
      chan[c.chan].std.release();
      break;
    case DIV_CMD_NOTE_OFF: {
      chan[c.chan].active=false;
      chan[c.chan].sample=-1;
      chan[c.chan].active=false;
      chan[c.chan].keyOff=true;
      break;
    }
    case DIV_CMD_INSTRUMENT:
      chan[c.chan].ins=c.value;
      break;
    case DIV_CMD_VOLUME:
      chan[c.chan].vol=c.value;
      if (chan[c.chan].vol>255) chan[c.chan].vol=255;
      break;
    case DIV_CMD_HINT_VOLUME:
      break;
    case DIV_CMD_PANNING:
      chan[c.chan].outL2=c.value;
      chan[c.chan].outR2=c.value2;
      break;
    case DIV_CMD_GET_VOLUME:
      return chan[c.chan].vol;
      break;
    case DIV_CMD_PITCH:
      chan[c.chan].pitch=c.value;
      chan[c.chan].freqChanged=true;
      break;
    case DIV_CMD_NOTE_PORTA: {
      int destFreq=chan[c.chan].calcBaseFreq(c.value2);
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
      if (return2) return 2;
      break;
    }
    case DIV_CMD_LEGATO:
      chan[c.chan].baseFreq=chan[c.chan].calcBaseFreq(c.value);
      chan[c.chan].freqChanged=true;
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

void DivPlatformCSE1::notifyInsDeletion(void* ins) {
  for (int i=0; i<chans; i++) {
    chan[i].std.notifyInsDeletion(static_cast<DivInstrument*>(ins));
  }
}

DivMacroInt *DivPlatformCSE1::getChanMacroInt(int ch) {
  if (ch>=chans) return nullptr;
  return &chan[ch].std;
}


void DivPlatformCSE1::forceIns() {
  for (int i=0; i<chans; i++) {
    chan[i].insChanged=true;
    chan[i].freqChanged=true;
    chan[i].sample=-1;
  }
}

unsigned char *DivPlatformCSE1::getRegisterPool() {
  return reinterpret_cast<uint8_t*>(&chip.CHANNELS.CHANNEL);
}

int DivPlatformCSE1::getRegisterPoolDepth() {
  return 16;
}

int DivPlatformCSE1::getRegisterPoolSize() {
  return sizeof(CSE1_PACKED::CSE1_CHANNEL_REGISTERS) * CSE1_CHANNEL_NUMBER / 2;
}

void DivPlatformCSE1::notifyInsChange(int ins) {
  for (int i=0; i<chans; i++) {
    if (chan[i].ins==ins) {
      chan[i].insChanged=true;
    }
  }
}

const DivMemoryComposition *DivPlatformCSE1::getMemCompo(int index) {
  if (index!=0) return nullptr;
  return &memCompo;
}

const void *DivPlatformCSE1::getSampleMem(int index) {
  return index == 0 ? pcmMem : nullptr;
}

size_t DivPlatformCSE1::getSampleMemCapacity(int index) {
  return index == 0 ? MEMORY_SIZE : 0;
}

size_t DivPlatformCSE1::getSampleMemUsage(int index) {
  return index == 0 ? sampleMemLen : 0;
}

bool DivPlatformCSE1::isSampleLoaded(int index, int sample) {
  if (index!=0) return false;
  if (sample<0 || sample>32767) return false;
  return sampleLoaded[sample];
}

void DivPlatformCSE1::renderSamples(int sysID) {
    memset(pcmMem, 0, MEMORY_SIZE * sizeof(CSE1_PACKED::CSE1_REG));
    memset(sampleOff, 0, 32768 * sizeof(uint32_t));
    memset(sampleLoaded, 0, 32768 * sizeof(bool));

    memCompo = DivMemoryComposition();
    memCompo.name = "CSE-1 RAM (1 Byte = 16 Bit)";

  size_t memPos=0;
  for (int i=0; i<parent->song.sampleLen; i++) {
    DivSample* s=parent->song.sample[i];
    if (!s->renderOn[0][sysID]) {
      sampleOff[i]=0;
      continue;
    }

    const uint32_t lengthByte = s->getCurBufLen();
    const uint32_t lengthCSE1_REG = lengthByte>>1;
    const uint8_t* originData = static_cast<uint8_t*>(s->getCurBuf());
    const uint32_t length = s->depth == DIV_SAMPLE_DEPTH_16BIT ? lengthCSE1_REG : lengthByte;
    const size_t maxSize = getSampleMemCapacity(0);
    if (memPos+length>maxSize) {
      sampleLoaded[i]=false;
      continue;
    }

    if (length>0) {
      if (s->depth==DIV_SAMPLE_DEPTH_16BIT) {
        memcpy(&pcmMem[memPos], originData, lengthByte);
      } else {
        for (unsigned int si = 0; si < length; si++) {
          pcmMem[memPos+si] = (static_cast<uint16_t>(originData[si])<<8)|static_cast<uint16_t>(originData[si]);
        }
      }
      sampleOff[i]=memPos;
      memCompo.entries.push_back(DivMemoryEntry(DIV_MEMORY_SAMPLE,"Sample",i,memPos,memPos+length));
      memPos += length;
    }
    sampleLoaded[i]=true;
  }

    sysIDCache = sysID;

    sampleMemLen = memPos;
    memCompo.used = sampleMemLen;
    memCompo.capacity = getSampleMemCapacity(0);
}
void DivPlatformCSE1::reset() {
  chip.hard_reset();
  for (int i=0; i<chans; i++) {
    chan[i]=Channel(parent->song.compatFlags.linearPitch);
    chan[i].pitchTable=&pitchTable;
    chan[i].pitchTable=samplePitchTable.get(-1);
    chan[i].vol=255;
    chan[i].std.setEngine(parent);
  }
}

void DivPlatformCSE1::notifyPitchTable(int sample) {
  pitchTable.init(
    parent->song.tuning,chipClock,
    CHIP_FREQBASE,
    0x7fffffff,
    false,
    parent->song.compatFlags.linearPitch);
  samplePitchTable.update<Channel>(
    chan,chans,parent->song.tuning,chipClock,
    1<<16,
    0x7fffffff,
    false,
    parent->song.compatFlags.linearPitch,
    sample);
}

unsigned int DivPlatformCSE1::getMaxFreq(int ch) {
  return 0xffffffff;
}

void DivPlatformCSE1::setFlags(const DivConfig& flags) {
  chipClock=192000;
  CHECK_CUSTOM_CLOCK
  chipClock = flags.getBool("quarterClock",false) ? chipClock / 4 : chipClock;
  rate=chipClock;
  for (int i = 0; i < chans; i++) {
    oscBuf[i]->setRate(rate);
  }
  waveTable.default_volume_line = flags.getInt("defaultVolumeTableType",1);
  waveTable.fastSpeed = flags.getInt("defaultVolumeTableSpeed",0x4000);
  waveTable.not_fm = flags.getBool("not_fm",false);
  waveTable.chipType = flags.getInt("revision",1);
  waveTable.reset();
  notifyPitchTable();
}

int DivPlatformCSE1::init(DivEngine* p, int channels, int sugRate, const DivConfig& flags) {
  parent=p;
  samplePitchTable.init(parent);
  dumpWrites=false;
  skipRegisterWrites=false;
  for (int i=0; i<DIV_MAX_CHANS; i++) {
    isMuted[i]=false;
    if (i<channels) {
      oscBuf[i]=new DivDispatchOscBuffer;
      oscBuf[i]->setRate(192000);
    }
  }
  rate=192000;
  chipClock=192000;
  notifyPitchTable();
  chans=channels;

  sampleOff=new unsigned int[32768];
  sampleLoaded=new bool[32768];
  pcmMem=new CSE1_PACKED::CSE1_REG[getSampleMemCapacity(0)];
  sampleMemLen=0;
  waveTable.reset();

  waveTable.memPCM=pcmMem;

  setFlags(flags);
  reset();

  return channels;
}

void DivPlatformCSE1::quit() {
  for (int i=0; i<chans; i++) {
    delete oscBuf[i];
  }
  delete[] pcmMem;
  waveTable.memPCM = nullptr;
  samplePitchTable.destroy<Channel>(chan,CSE1_CHANNEL_NUMBER);
}

DivPlatformCSE1::~DivPlatformCSE1() = default;
