#ifndef CRAPSYNTH_H
#define CRAPSYNTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define STM32CRAPSYNTH_ACC_BITS 28

#define STM32CRAPSYNTH_NUM_CHANNELS 13
#define STM32CRAPSYNTH_FLASH_SAMPLE_MEM_SIZE (2048 * 206) /* Each Flash page is 2 KiB; the size may be changed when I am sure what final firmware size is... */
#define STM32CRAPSYNTH_RAM_SAMPLE_MEM_SIZE (1024 * 50) /* 50 KiB */
#define STM32CRAPSYNTH_WAVETABLE_SIZE 256

typedef struct
{
    uint8_t wave_type;
    uint32_t acc, freq;
    uint8_t pending_vol;
    bool zero_cross;

    uint32_t timer_acc;
    uint32_t timer_freq; //approximation of STM32 timers...
    uint16_t pw;
    //timer frequency and duty will be converted to actual values during export
} AD9833Chan;

typedef struct
{
    uint32_t lfsr;
    uint32_t lfsr_reload;
    bool zero_cross;
    uint8_t pending_vol;

    uint32_t timer_acc;
    uint32_t timer_freq; //approximation of STM32 timers...
    uint16_t output;
    bool internal_clock;
    uint32_t timer_freq_memory;
} NoiseChan;

typedef struct
{
    uint32_t start_addr;
    uint32_t length;
    uint32_t loop_point;
    uint32_t curr_pos;

    uint8_t wavetable[STM32CRAPSYNTH_WAVETABLE_SIZE];

    uint32_t timer_acc;
    uint32_t timer_freq; //approximation of STM32 timers...
    bool zero_cross;
    uint8_t pending_vol;
    int output;

    bool playing;
    bool play_wavetable;

    uint8_t wave_type;

    uint16_t lfsr;
    int triangle_counter;
    uint8_t triangle_counter_dir;
    uint8_t noise_tri_amp;

    bool loop;
    bool ram; //play sample from ram
} DACChan;

typedef struct
{
    uint32_t timer_acc;
    uint32_t timer_freq; //approximation of STM32 timers...
    //TODO: one of the timers is RTC, clocked at merely 250 kHz, so limit freq resolution somehow?

    uint8_t chan_bitmask; //on which channel(s) the timer does phase reset
    bool enable;
} PhaseResetTimer;

typedef struct
{
    AD9833Chan ad9833[4];
    NoiseChan noise;
    DACChan dac[3];
    PhaseResetTimer timer[4+1]; //when noise has internal clock we can use its timer...
    uint8_t sample_mem_flash[STM32CRAPSYNTH_FLASH_SAMPLE_MEM_SIZE];
    uint8_t sample_mem_ram[STM32CRAPSYNTH_RAM_SAMPLE_MEM_SIZE];
    float volume_table[256];
    uint16_t sine_table[1 << (STM32CRAPSYNTH_ACC_BITS - 10)];
    uint8_t volume[9];
    bool muted[STM32CRAPSYNTH_NUM_CHANNELS];
    uint32_t clock_rate; //in Hz. 25 MHz default
    int32_t chan_outputs[STM32CRAPSYNTH_NUM_CHANNELS];
    int32_t final_output;
} STM32CrapSynth;

STM32CrapSynth* crapsynth_create();
void crapsynth_reset(STM32CrapSynth* crapsynth);
void crapsynth_write(STM32CrapSynth* crapsynth, uint8_t channel, uint32_t data_type, uint32_t data);
void crapsynth_clock(STM32CrapSynth* crapsynth);
void crapsynth_set_is_muted(STM32CrapSynth* crapsynth, uint8_t ch, bool mute);
void crapsynth_set_clock_rate(STM32CrapSynth* crapsynth, uint32_t clock);
void crapsynth_free(STM32CrapSynth* crapsynth);

#ifdef __cplusplus
};
#endif
#endif
