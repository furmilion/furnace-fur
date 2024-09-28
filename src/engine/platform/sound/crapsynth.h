#ifndef CRAPSYNTH_H
#define CRAPSYNTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define STM32CRAPSYNTH_NUM_CHANNELS 11

typedef struct
{
    bool muted[STM32CRAPSYNTH_NUM_CHANNELS];
    uint32_t clock_rate; //in Hz. 25 MHz default
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
