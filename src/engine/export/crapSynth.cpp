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

void DivExportCrapSynth::run() {
  SafeWriter* crapwriter=new SafeWriter;

  DivPlatformSTM32CRAPSYNTH* crapsynth=(DivPlatformSTM32CRAPSYNTH*)e->getDispatch(0);

  running=false;

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

  /*crapwriter->writeText("RAM SAMPLES");
  crapwriter->writeI((unsigned int)crapsynth->getSampleMemUsage(1));

  for(int i = 0; i < (int)crapsynth->getSampleMemUsage(1); i++)
  {
    crapwriter->writeC(((unsigned char*)crapsynth->getSampleMem(1))[i]);
    progress[0].amount = 0.2f + 0.05f * i / (float)crapsynth->getSampleMemUsage(1);
  }*/

  logAppend("writing wavetables/synth info dump...");

  crapwriter->writeText("REGISTERS DUMP");

  regdump_offset = crapwriter->tell();
  crapwriter->writeI(0); //size, will return here later
  
  crapwriter->writeF(e->getCurHz()); //todo: replace with actual autoreload val calc

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
    DivGroovePattern curSpeeds=s->speeds;

    //int groove_counter = 0;

    for(int o = 0; o < s->ordersLen; o++)
    {
      for(int r = 0; r < s->patLen; r++)
      {
        begin:;

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

      // write wait
      for(int i = 0; i < e->song.systemLen; i++)
      {
        last_frame_addr = (uint32_t)crapwriter->tell() - regdump_offset;
        crapwriter->writeC(0xff); // next frame
      }
      // get register dumps
      std::vector<DivRegWrite>& writes=e->disCont[0].dispatch->getRegisterWrites();
      if (writes.size() > 0) 
      {
        for (DivRegWrite& write: writes)
        {
            crapwriter->writeI(write.addr); //TODO replace with actual commands
            crapwriter->writeI(write.val);
        }

        writes.clear();
      }
    }
    // end of song

    crapwriter->writeC(0xfe);

    if(!loop) //after end mark store loop point
    {
      crapwriter->writeI(last_frame_addr);
    }
    else
    {
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
