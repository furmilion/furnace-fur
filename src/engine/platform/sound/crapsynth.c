#include "crapsynth.h"

STM32CrapSynth* crapsynth_create()
{
    STM32CrapSynth* crapsynth = (STM32CrapSynth*)malloc(sizeof(STM32CrapSynth));

    memset(crapsynth, 0, sizeof(STM32CrapSynth));

    return crapsynth;
}

void crapsynth_reset(STM32CrapSynth* crapsynth)
{
    memset(crapsynth, 0, sizeof(STM32CrapSynth));
}

void crapsynth_write(STM32CrapSynth* crapsynth, uint8_t channel, uint32_t data_type, uint32_t data)
{

}

void crapsynth_clock(STM32CrapSynth* crapsynth)
{

}

void crapsynth_set_is_muted(STM32CrapSynth* crapsynth, uint8_t ch, bool mute)
{

}

void crapsynth_set_clock_rate(STM32CrapSynth* crapsynth, uint32_t clock)
{

}

void crapsynth_free(STM32CrapSynth* crapsynth)
{
    free(crapsynth);
}