/*
 * Copyright (C) 2021, 2024 nukeykt
 * Modified by J.C. Moyer
 * Adapted for Furnace by removing the SC-55 MCU coupling.
 * Original source file: src/pcm.h from Nuked-SC55.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  Thanks:
 *      John McMaster (https://siliconprawn.org):
 *          PCM chip decap
 *
 * What changed for Furnace:
 *  - the wave ROM arrays are gone, replaced by a read callback so the host
 *    can hand the chip whatever it likes (Furnace sample memory, a real ROM)
 *  - MCU_PostSample became a small output FIFO
 *  - the MCU interrupt lines became a single flag
 *  - PCM_Update split into a one-iteration tick so the host can pull frames
 *  - each voice keeps its last post-TVA value for oscilloscopes
 *  - the JV-880 paths are dropped, this is an SC-55 only build
 *  - an optional filter bypass, which the hardware does not have
 */

#ifndef _GP_PCM_H
#define _GP_PCM_H

#include <stdint.h>

#define GP_SLOTS 32
#define GP_ERAM_SIZE 0x4000

struct GPPCM_Config {
  // config_reg_3c
  uint32_t orval;
  int dac_mask; // unused
  uint8_t noise_mask;
  uint8_t write_mask;
  bool oversampling;

  // config_reg_3d
  // important that this starts at 1, see derivation in GPPCM_Write
  uint8_t reg_slots;
};

// address is a flat 24-bit wave address. the host decides what lives there.
typedef uint8_t (*GPPCM_ReadROM)(void* user, uint32_t address);

struct gppcm_t {
  uint32_t ram1[GP_SLOTS][8];
  uint16_t ram2[GP_SLOTS][16];
  uint64_t cycles;
  uint32_t voice_mask;
  uint32_t voice_mask_pending;
  uint32_t write_latch; // 20 bits wide?
  uint32_t read_latch;  // 20 bits wide?
  uint32_t wave_read_address;
  uint16_t tv_counter; // 14 bits wide?
  uint8_t wave_byte_latch;
  uint8_t select_channel; // 5 bits wide?
  uint8_t config_reg_3c;
  uint8_t config_reg_3d;
  uint8_t irq_channel; // range 1..32
  bool irq_assert;
  bool voice_mask_updating;
  bool nfs;
  int32_t accum_l;
  int32_t accum_r;
  int32_t rcsum[2];

  GPPCM_Config config;

  uint16_t eram[GP_ERAM_SIZE];

  // host hooks
  GPPCM_ReadROM rom_read;
  void* rom_user;
  bool is_mk1;
  bool enable_oversampling;
  // take the interpolated sample straight out, before the per voice filter.
  // the filter sits in the signal path on real hardware, so this is a
  // deliberate departure, made so the chip can be used as a plain PCM voice.
  bool filter_bypass;

  // the chip emits one or two frames per tick depending on oversampling
  int32_t out[2][2];
  int out_count;

  // last post-TVA, pre-pan value of each voice, for the oscilloscope
  int32_t voice_out[GP_SLOTS];
};

void GPPCM_Init(gppcm_t& pcm, GPPCM_ReadROM romRead, void* romUser);
void GPPCM_Reset(gppcm_t& pcm);
void GPPCM_Write(gppcm_t& pcm, uint32_t address, uint8_t data);
uint8_t GPPCM_Read(gppcm_t& pcm, uint32_t address);

// runs exactly one chip iteration. fills pcm.out with pcm.out_count frames.
void GPPCM_Tick(gppcm_t& pcm);

// original cycle-driven entry point, kept for reference and for callers that
// want to drive the chip by its clock
void GPPCM_Update(gppcm_t& pcm, uint64_t cycles);

uint32_t GPPCM_GetOutputFrequency(const gppcm_t& pcm);
void GPPCM_GetConfig(GPPCM_Config& config, uint8_t config_byte);

#endif
