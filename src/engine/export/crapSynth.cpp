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

#include "crapSynth.h"
#include "../engine.h"
#include "../platform/stm32crapsynth.h"

unsigned char chan_base_addr[20];

#define CMD_AD9833_VOL 0
#define CMD_AD9833_WAVE_TYPE 1
#define CMD_AD9833_FREQ 2 /*28 bits*/
#define CMD_AD9833_PHASE_RESET 3
#define CMD_AD9833_PWM_FREQ 4 /*8 bits prescaler and 16 bits autoreload*/
#define CMD_AD9833_PWM_DUTY 5
#define CMD_AD9833_ZERO_CROSS_ENABLE 6
#define CMD_AD9833_ZERO_CROSS_DISABLE 7

#define CMD_NOISE_VOL 0
#define CMD_NOISE_CLOCK_INTERNAL 1
#define CMD_NOISE_CLOCK_EXTERNAL 2
#define CMD_NOISE_ZERO_CROSS_ENABLE 3
#define CMD_NOISE_ZERO_CROSS_DISABLE 4
#define CMD_NOISE_FREQ 5
#define CMD_NOISE_RESET 6

#define CMD_DAC_VOLUME 0
#define CMD_DAC_PLAY_SAMPLE 1
#define CMD_DAC_PLAY_SAMPLE_LOOPED 2
#define CMD_DAC_PLAY_WAVETABLE 3
#define CMD_DAC_STOP 4
#define CMD_DAC_START_ADDR_FLASH 5
#define CMD_DAC_START_ADDR_RAM 6
#define CMD_DAC_RESET 7
#define CMD_DAC_TIMER_FREQ 8
#define CMD_DAC_ZERO_CROSS_ENABLE 9
#define CMD_DAC_ZERO_CROSS_DISABLE 10
#define CMD_DAC_LOOP_POINT_FLASH 11
#define CMD_DAC_LOOP_POINT_RAM 12
#define CMD_DAC_LENGTH_FLASH 13
#define CMD_DAC_LENGTH_RAM 14
#define CMD_DAC_WAVE_TYPE 15
#define CMD_DAC_DUTY 16
#define CMD_DAC_WAVETABLE_DATA 17
#define CMD_DAC_NOISE_TRI_AMP 18

#define CMD_TIMER_CHANNEL_BITMASK 0
#define CMD_TIMER_FREQ 1
#define CMD_TIMER_RESET 2
#define CMD_TIMER_ENABLE 3
#define CMD_TIMER_DISABLE 4

#define CMD_NEXT_FRAME 0xfb
#define CMD_SET_RATE 0xfc
#define CMD_LOOP_POINT 0xfd
#define CMD_END 0xfe
#define CMD_NOP 0xff

#define STM32_CLOCK 72000000
#define EMUL_CLOCK 12500000
#define RTC_WAKEUP_CLOCK 250000

#define BUFFER_LEN 8192 /*buffer is divided into two halves, and we need to check if we cross the boundary of 2nd and "next" 1st half (which wraps to the beginning of array in memory)*/

typedef struct 
{
  uint32_t ad9833_freq[4];
  uint16_t duty[4];
  uint16_t pwm_autoreload[4];
  uint8_t pwm_prescaler[4];
  bool pwm[4];
  int dac_wave_type[2];
  int dac_duty[2];
} CrapSynthState;

uint32_t calc_autoreload_eng_tick(float hz)
{
  return (uint32_t)((double)STM32_CLOCK / (double)hz);
}

uint32_t calc_prescaler_and_autoreload(uint32_t freq) //(prescaler << 16) | autoreload
{
  double freq_in_hz = (double)(EMUL_CLOCK / 4) * (double)freq / double(1 << 29);

  //find prescaler
  int prescaler = 1;

  while((double)(STM32_CLOCK) / (double)freq_in_hz / (double)prescaler > 65536.0)
  {
    prescaler++;
  }

  if(prescaler > 256) prescaler = 256;

  //find autoreload
  int autoreload = (int)((double)(STM32_CLOCK) / (double)freq_in_hz / (double)prescaler);

  if(autoreload > 65536) autoreload = 65536;

  return ((prescaler - 1) << 16) | (autoreload - 1);
}

uint32_t calc_systick_autoreload(uint32_t freq) //systick just has 24 bits counter without prescalers
{
  double freq_in_hz = (double)(EMUL_CLOCK / 4) * (double)freq / double(1 << 29);

  //find autoreload
  int autoreload = (int)((double)(STM32_CLOCK) / (double)freq_in_hz);

  if(autoreload > (1 << 23)) autoreload = (1 << 23);

  return (autoreload - 1);
}

uint32_t calc_rtc_wakeup_autoreload(uint32_t freq) //wakeup timer has 16-bit counter with optional /2 freq divider (effective 17 bits)
{
  double freq_in_hz = (double)(EMUL_CLOCK / 4) * (double)freq / double(1 << 29);

  //find autoreload
  int autoreload = (int)((double)RTC_WAKEUP_CLOCK / (double)freq_in_hz);

  if(autoreload > (1 << 16)) autoreload = (1 << 16);

  return (autoreload - 1);
}

uint32_t calc_next_buffer_boundary(SafeWriter* w, uint32_t regdump_offset)
{
  uint32_t pos = 0;

  while(pos < (uint32_t)w->tell() - regdump_offset)
  {
    pos += BUFFER_LEN;
  }

  return pos;
}

void write_command(SafeWriter* w, unsigned int addr, unsigned int val, uint32_t regdump_offset, CrapSynthState& state, int* curr_write, std::vector<DivRegWrite>& writes)
{
  int channel = addr >> 8;
  int cmd_type = addr & 0xff;

  if(channel < 4)
  {
    switch(cmd_type)
    {
      case 0: //volume
      {
        w->writeC(chan_base_addr[channel] + CMD_AD9833_VOL);
        w->writeC(val & 0xff);
        break;
      }
      case 1: //waveform
      {
        w->writeC(chan_base_addr[channel] + CMD_AD9833_WAVE_TYPE);
        w->writeC(val & 0xff);
        state.pwm[channel] = (val == 5);
        break;
      }
      case 2: //AD9833 freq
      {
        w->writeC(chan_base_addr[channel] + CMD_AD9833_FREQ);
        w->writeI(val);
        state.ad9833_freq[channel] = val;
        break;
      }
      case 3: //phase reset
      {
        w->writeC(chan_base_addr[channel] + CMD_AD9833_PHASE_RESET);
        break;
      }
      case 4: //PWM freq
      {
        w->writeC(chan_base_addr[channel] + CMD_AD9833_PWM_FREQ);
        uint32_t values = calc_prescaler_and_autoreload(val);
        w->writeC(values >> 16);
        w->writeS(values & 0xffff);
        state.pwm_autoreload[channel] = values & 0xffff;
        state.pwm_prescaler[channel] = values >> 16;
        break;
      }
      case 5: //duty
      {
        w->writeC(chan_base_addr[channel] + CMD_AD9833_PWM_DUTY);
        w->writeS((val & 0xffff) * state.pwm_autoreload[channel] / 0xffff);
        break;
      }
      case 6: //zero cross
      {
        w->writeC(chan_base_addr[channel] + (val ? CMD_AD9833_ZERO_CROSS_ENABLE : CMD_AD9833_ZERO_CROSS_DISABLE));
        break;
      }
      default: break;
    }
  }

  if(channel == 4) //noise
  {
    switch(cmd_type)
    {
      case 0: //volume
      {
        w->writeC(chan_base_addr[channel] + CMD_NOISE_VOL);
        w->writeC(val & 0xff);
        break;
      }
      case 1: //clock source
      {
        w->writeC(chan_base_addr[channel] + (val ? CMD_NOISE_CLOCK_INTERNAL : CMD_NOISE_CLOCK_EXTERNAL));
        break;
      }
      case 4: //timer freq
      {
        w->writeC(chan_base_addr[channel] + CMD_NOISE_FREQ);
        uint32_t values = calc_prescaler_and_autoreload(val);
        w->writeC(values >> 16);
        w->writeS(values & 0xffff);
        break;
      }
      case 3: //phase reset
      {
        w->writeC(chan_base_addr[channel] + CMD_NOISE_RESET);
        break;
      }
      case 6: //zero cross
      {
        w->writeC(chan_base_addr[channel] + (val ? CMD_NOISE_ZERO_CROSS_ENABLE : CMD_NOISE_ZERO_CROSS_DISABLE));
        break;
      }
      default: break;
    }
  }

  if(channel == 5 || channel == 6) //DAC chans...
  {
    bool ram = val & 0x1000000;

    switch(cmd_type)
    {
      case 0: //volume
      {
        w->writeC(chan_base_addr[channel] + CMD_DAC_VOLUME);
        w->writeC(val & 0xff);
        break;
      }
      case 1: //start/stop playback of wavetable/sample
      {
        if(val & 1) //playing
        {
          if(val & 2)
          {
            w->writeC(chan_base_addr[channel] + CMD_DAC_PLAY_WAVETABLE);
          }
          else
          {
            if(val & 4)
            {
              w->writeC(chan_base_addr[channel] + CMD_DAC_PLAY_SAMPLE_LOOPED);
            }
            else
            {
              w->writeC(chan_base_addr[channel] + CMD_DAC_PLAY_SAMPLE);
            }
          }
        }
        else //stop
        {
          w->writeC(chan_base_addr[channel] + CMD_DAC_STOP);
        }
        break;
      }
      case 2: //start addr
      {
        w->writeC(chan_base_addr[channel] + (ram ? CMD_DAC_START_ADDR_RAM : CMD_DAC_START_ADDR_FLASH));

        if(ram) w->writeS(val & 0xffff);
        else 
        {
          w->writeC(val & 0xff);
          w->writeC((val >> 8) & 0xff);
          w->writeC((val >> 16) & 0xff);
        }
        break;
      }
      case 3: //reset
      {
        w->writeC(chan_base_addr[channel] + CMD_DAC_RESET);
        break;
      }
      case 4: //DAC timer freq
      {
        w->writeC(chan_base_addr[channel] + CMD_DAC_TIMER_FREQ);
        uint32_t values = calc_prescaler_and_autoreload(val);
        w->writeC(values >> 16);
        w->writeS(values & 0xffff);
        break;
      }
      case 6: //zero cross vol upd
      {
        w->writeC(chan_base_addr[channel] + (val ? CMD_DAC_ZERO_CROSS_ENABLE : CMD_DAC_ZERO_CROSS_DISABLE));
        break;
      }
      case 7: //loop point
      {
        w->writeC(chan_base_addr[channel] + (ram ? CMD_DAC_LOOP_POINT_RAM : CMD_DAC_LOOP_POINT_FLASH));

        if(ram) w->writeS(val & 0xffff);
        else 
        {
          w->writeC(val & 0xff);
          w->writeC((val >> 8) & 0xff);
          w->writeC((val >> 16) & 0xff);
        }
        break;
      }
      case 8: //length
      {
        w->writeC(chan_base_addr[channel] + (ram ? CMD_DAC_LENGTH_RAM : CMD_DAC_LENGTH_FLASH));

        if(ram) w->writeS(val & 0xffff);
        else 
        {
          w->writeC(val & 0xff);
          w->writeC((val >> 8) & 0xff);
          w->writeC((val >> 16) & 0xff);
        }
        break;
      }
      case 9: //wave type & duty
      {
        if((unsigned int)state.dac_wave_type[channel - 5] != (val & 7))
        {
          state.dac_wave_type[channel - 5] = val & 7;
          w->writeC(chan_base_addr[channel] + CMD_DAC_WAVE_TYPE);
          w->writeC(val & 7);
        }
        if(state.dac_wave_type[channel - 5] == 6) //pulse wave
        {
          if((unsigned int)state.dac_duty[channel - 5] != val >> 8)
          {
            state.dac_duty[channel - 5] = val >> 8;
            w->writeC(chan_base_addr[channel] + CMD_DAC_DUTY);
            w->writeC((val >> 4) & 0xff);
          }
        }
        break;
      }
      case 10: /* sigh... wavetable madness */
      {
        uint32_t buffer_boundary = calc_next_buffer_boundary(w, regdump_offset);
        uint32_t distance_to_next_boundary = buffer_boundary - ((uint32_t)w->tell() - regdump_offset);

        if(distance_to_next_boundary < 256) //wavetable should lie in receive buffer as solid chunk, otherwise we can't transfer it with DMA
        {
          for(uint32_t i = 0; i < distance_to_next_boundary; i++)
          {
            w->writeC(CMD_NOP); //ugly but should work and save CPU work in general case
          }
        }

        //w->writeText("WAVETABLE"); //todo: remove

        w->writeC(chan_base_addr[channel] + CMD_DAC_WAVETABLE_DATA);

        for(int i = 0; i < 256; i++)
        {
          DivRegWrite write = writes[*curr_write];
          w->writeC(write.val & 0xff); //ignore address
          (*curr_write)++;
        }
        break;
      }
      case 11:
      {
        w->writeC(CMD_DAC_NOISE_TRI_AMP);
        w->writeC(val % 12);
        break;
      }
      default: break;
    }
  }

  if(channel > 6) //phase reset timers
  {
    switch(cmd_type)
    {
      case 0: //bitmask
      {
        w->writeC(chan_base_addr[channel] + CMD_TIMER_CHANNEL_BITMASK);
        w->writeC(val & 0x7f);
        break;
      }
      case 1: //timer freq
      {
        if(channel == 7 || channel == 8 || channel == 11) //usual timers
        {
          w->writeC(chan_base_addr[channel] + CMD_TIMER_FREQ);
          uint32_t values = calc_prescaler_and_autoreload(val);
          w->writeC(values >> 16);
          w->writeS(values & 0xffff);
        }

        if(channel == 9) //systick
        {
          w->writeC(chan_base_addr[channel] + CMD_TIMER_FREQ);
          uint32_t value = calc_systick_autoreload(val);
          w->writeC(value >> 16);
          w->writeS(value & 0xffff);
        }

        if(channel == 10) //RTC wakeup timer, clock is 8000000 / 32 = 250000 Hz
        {
          w->writeC(chan_base_addr[channel] + CMD_TIMER_FREQ);
          uint32_t value = calc_rtc_wakeup_autoreload(val);
          w->writeC(value >> 16);
          w->writeS(value & 0xffff);
        }

        break;
      }
      case 2: //phase reset
      {
        w->writeC(chan_base_addr[channel] + CMD_TIMER_RESET);
        break;
      }
      case 3: //enable
      {
        w->writeC(chan_base_addr[channel] + (val ? CMD_TIMER_ENABLE : CMD_TIMER_DISABLE));
        break;
      }
      default: break;
    }
  }
}

void DivExportCrapSynth::run() {
  SafeWriter* crapwriter=new SafeWriter;

  DivPlatformSTM32CRAPSYNTH* crapsynth=(DivPlatformSTM32CRAPSYNTH*)e->getDispatch(0);

  running=false;

  CrapSynthState state;

  memset((void*)&state, 0, sizeof(CrapSynthState));

  state.dac_duty[0] = state.dac_duty[1] = -1;
  state.dac_wave_type[0] = state.dac_wave_type[1] = -1;

  double origRate = e->got.rate;
  e->stop();
  e->repeatPattern=false;
  e->setOrder(0);

  logAppend("playing and logging register writes...");

  bool loop = true;
  uint32_t loop_point_addr = 0;
  uint32_t last_frame_addr = 0;

  uint32_t regdump_offset = 0;
  uint32_t size_offset = 0;

  float hz = e->getCurHz();

  for(int i = 0; i < 5; i++) //AD9833 chans & noise chan
  {
    chan_base_addr[i] = 8 * i;
  }
  for(int i = 5; i < 7; i++) //DAC chans
  {
    chan_base_addr[i] = 8 * 5 + 32 * (i - 5);
  }
  for(int i = 7; i < 12; i++) //phase reset timer chans
  {
    chan_base_addr[i] = 8 * 5 + 32 * 2 + 8 * (i - 7);
  }

  //file header

  crapwriter->init();

  crapwriter->writeText("CRAP");
  crapwriter->writeI(1); //version

  size_offset = (uint32_t)crapwriter->tell();
  crapwriter->writeI(0); //size, will return here later

  logAppend("writing samples...");

  crapwriter->writeText("FLASH SAMPLES");
  crapwriter->writeI((unsigned int)crapsynth->getSampleMemUsage(0));

  for(int i = 0; i < (int)crapsynth->getSampleMemUsage(0); i++)
  {
    crapwriter->writeC(((unsigned char*)crapsynth->getSampleMem(0))[i]);
    progress[0].amount = 0.2f * i / (float)crapsynth->getSampleMemUsage(0);
  }

  crapwriter->writeText("RAM SAMPLES");
  crapwriter->writeS((unsigned short)crapsynth->getSampleMemUsage(1));

  for(int i = 0; i < (int)crapsynth->getSampleMemUsage(1); i++)
  {
    crapwriter->writeC(((unsigned char*)crapsynth->getSampleMem(1))[i]);
    progress[0].amount = 0.2f + 0.05f * i / (float)crapsynth->getSampleMemUsage(1);
  }

  logAppend("writing wavetables/synth info dump...");

  crapwriter->writeText("REGISTERS DUMP");

  regdump_offset = crapwriter->tell();
  crapwriter->writeI(0); //size, will return here later
  
  crapwriter->writeI(calc_autoreload_eng_tick(hz));

  e->synchronizedSoft([&]() {

    // Determine loop point.
    int loopOrder=0;
    int loopRow=0;
    int loopEnd=0;
    e->walkSong(loopOrder,loopRow,loopEnd);
    logAppendf("loop point: %d %d",loopOrder,loopRow);
    e->warnings="";

    //walk song to find how many frames it is to loop point
    DivSubSong* s = e->curSubSong;
    //DivGroovePattern curSpeeds=s->speeds;

    //int groove_counter = 0;

    for(int o = 0; o < s->ordersLen; o++)
    {
      for(int r = 0; r < s->patLen; r++)
      {
        //begin:;

        for(int ch = 0; ch < e->chans; ch++)
        {
          DivPattern* p = s->pat[ch].getPattern(s->orders.ord[ch][o],false);

          for(int eff = 0; eff < DIV_MAX_EFFECTS; eff++)
          {
            //short effectVal = p->data[r][5+(eff<<1)];

            if (p->data[r][4 + (eff << 1)] == 0xff)
            {
                loop = false;
                goto finish;
            }
          }
        }

        //groove_counter++;
        //groove_counter %= curSpeeds.len;

        //loop_point_addr += curSpeeds.val[groove_counter];

        if(o == loopOrder && r == loopRow && (loopOrder != 0 || loopRow != 0))
        {
          //goto finish;
        }
      }
    }

    finish:;

    // Reset the playback state.
    e->curOrder=0;
    e->freelance=false;
    e->playing=false;
    e->extValuePresent=false;
    e->remainingLoops=-1;

    // Prepare to write song data.
    e->playSub(false);
    bool done=false;
    e->disCont[0].dispatch->toggleRegisterDump(true);

    bool got_loop_point = false;

    while (!done) {
      if (e->nextTick(false,true) || !e->playing) {
        done=true;
        for (int i=0; i<e->song.systemLen; i++) {
          e->disCont[i].dispatch->getRegisterWrites().clear();
        }
        break;
      }

      // write wait
      last_frame_addr = (uint32_t)crapwriter->tell() - regdump_offset;
      crapwriter->writeC(CMD_NEXT_FRAME); // next frame

      int row = 0;
      int order = 0;
      e->getCurSongPos(row, order);

      progress[0].amount = 0.25f + (float)(order * e->curSubSong->patLen + row) / (float)(e->curSubSong->ordersLen * e->curSubSong->patLen) * 0.70f;

      if(row == loopRow && order == loopOrder && !got_loop_point)
      {
        for(int i = 0; i < e->song.systemLen; i++)
        {
          if(row == 0 && order == 0 && loop)
          {
            loop_point_addr = 0;
          }
          else if(loop)
          {
            loop_point_addr = (uint32_t)crapwriter->tell() - regdump_offset;
          }
        }

        got_loop_point = true;
      }
      // check if engine rate changed
      if(hz != e->getCurHz())
      {
        crapwriter->writeC(CMD_SET_RATE);
        hz = e->getCurHz();
        crapwriter->writeI(calc_autoreload_eng_tick(hz));
      }
      // get register dumps
      std::vector<DivRegWrite>& writes=e->disCont[0].dispatch->getRegisterWrites();
      if (writes.size() > 0) 
      {
        for (int curr_write = 0; curr_write < (int)writes.size(); curr_write++)
        {
          DivRegWrite write = writes[curr_write];
          //crapwriter->writeI(write.addr); //TODO replace with actual commands
          //crapwriter->writeI(write.val);
          write_command(crapwriter, write.addr, write.val, regdump_offset, state, &curr_write, writes);
        }

        writes.clear();
      }
    }
    // end of song

    if(!loop) //after end mark store loop point
    {
      crapwriter->writeC(CMD_END);
      //crapwriter->writeI(last_frame_addr);
    }
    else
    {
      crapwriter->writeC(CMD_LOOP_POINT);
      crapwriter->writeI(loop_point_addr);
    }

    uint32_t curr_pos = (uint32_t)crapwriter->tell();
    crapwriter->seek(size_offset, SEEK_SET);
    crapwriter->writeI(curr_pos); //file size (without excluding anything!)

    crapwriter->seek(regdump_offset, SEEK_SET);
    crapwriter->writeI(curr_pos - regdump_offset - sizeof(unsigned int)); //file size

    // done - close out.
    e->got.rate=origRate;

    e->disCont[0].dispatch->toggleRegisterDump(false);

    e->remainingLoops=-1;
    e->playing=false;
    e->freelance=false;
    e->extValuePresent=false;
  });

  logAppend("writing data...");
  progress[0].amount=0.95f;

  // finish
  output.push_back(DivROMExportOutput("playback.scs",crapwriter));

  progress[0].amount=1.0f;

  logAppend("finished!");

  running=false;
}

bool DivExportCrapSynth::go(DivEngine* eng) {
  progress[0].name="Progress";
  progress[0].amount=0.0f;
  progress[1].name="";
  e=eng;
  running=true;
  failed=false;
  exportThread=new std::thread(&DivExportCrapSynth::run,this);
  return true;
}

void DivExportCrapSynth::wait() {
  if (exportThread!=NULL) {
    logV("waiting for export thread...");
    exportThread->join();
    delete exportThread;
  }
}

void DivExportCrapSynth::abort() {
  mustAbort=true;
  wait();
}

bool DivExportCrapSynth::isRunning() {
  return running;
}

bool DivExportCrapSynth::hasFailed() {
  return false;
}

DivROMExportProgress DivExportCrapSynth::getProgress(int index) {
  if(index > 1) return progress[1];
  return progress[index];
}
