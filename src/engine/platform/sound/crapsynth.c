#include "crapsynth.h"

#include <math.h>

#ifndef M_PI
#  define M_PI    3.14159265358979323846
#endif

void crapsynth_recalc_luts(STM32CrapSynth* crapsynth)
{
    for(int i = 0; i < 1024; i++)
    {
        crapsynth->sine_table[i] = (uint16_t)(sin((double)i * 2.0 * M_PI / (double)1024) * (double)511 + (double)511);
    }

    for(int i = 0; i < 256; i++)
    {
        if(i == 0)
        {
            crapsynth->volume_table[i] = 0.0; //mute
        }
        else
        {
            float gain_dB = 31.5 - (0.5 * (255.0 - (float)i));
            crapsynth->volume_table[i] = pow(10.0, gain_dB / 10.0);
        }
    }
}

STM32CrapSynth* crapsynth_create()
{
    STM32CrapSynth* crapsynth = (STM32CrapSynth*)malloc(sizeof(STM32CrapSynth));

    memset(crapsynth, 0, sizeof(STM32CrapSynth));

    crapsynth_recalc_luts(crapsynth);

    return crapsynth;
}

void crapsynth_reset(STM32CrapSynth* crapsynth)
{
    uint8_t* temp_memory = (uint8_t*)calloc(1, STM32CRAPSYNTH_SAMPLE_MEM_SIZE);
    memcpy(temp_memory, crapsynth->sample_mem, STM32CRAPSYNTH_SAMPLE_MEM_SIZE);
    memset(crapsynth, 0, sizeof(STM32CrapSynth));
    memcpy(crapsynth->sample_mem, temp_memory, STM32CRAPSYNTH_SAMPLE_MEM_SIZE);

    crapsynth_recalc_luts(crapsynth);

    free(temp_memory);
}

void crapsynth_write(STM32CrapSynth* crapsynth, uint8_t channel, uint32_t data_type, uint32_t data)
{
    if(channel < 4) //AD9833 chans
    {
        int chan = channel;

        switch(data_type)
        {
            case 0:
            {
                crapsynth->volume[channel] = data & 0xff;
                break;
            }
            case 1:
            {
                crapsynth->ad9833[chan].wave_type = data % 5;
                break;
            }
            case 2:
            {
                crapsynth->ad9833[chan].freq = data;
                break;
            }
            case 3: //reset
            {
                crapsynth->ad9833[chan].acc = 0;
                break;
            }
            default: break;
        }
    }
}

int32_t crapsynth_ad9833_get_wave(STM32CrapSynth* crapsynth, uint32_t acc, uint8_t wave_type)
{
    //todo: confirm if sine & tri are lower in amplitude than square and change it there
    switch(wave_type) //all waves have 10-bit resolution because DAC is 10-bit
    {
        case 0: //none
        {
            return 0;
            break;
        }
        case 1: //sine
        {
            return crapsynth->sine_table[acc >> (STM32CRAPSYNTH_ACC_BITS - 10)];
            break;
        }
        case 2: //triangle
        {
            return (((acc > (1 << (STM32CRAPSYNTH_ACC_BITS - 1))) ? ~acc : acc) >> (STM32CRAPSYNTH_ACC_BITS - 11)) & 1023;
            break;
        }
        case 3: //square, full volume
        {
            return ((acc > (1 << (STM32CRAPSYNTH_ACC_BITS - 1))) ? 1023 : 0);
            break;
        }
        case 4: //square, half volume
        {
            return ((acc > (1 << (STM32CRAPSYNTH_ACC_BITS - 1))) ? 511 : 0);
            break;
        }
        default: return 0; break;
    }
}

void crapsynth_clock(STM32CrapSynth* crapsynth)
{
    crapsynth->final_output = 0;

    for(int i = 0; i < 4; i++)
    {
        AD9833Chan* ch = &crapsynth->ad9833[i];

        if(ch->freq > 0)
        {
            ch->acc += ch->freq;
            ch->acc &= (1 << STM32CRAPSYNTH_ACC_BITS) - 1;
            int32_t wave = crapsynth_ad9833_get_wave(crapsynth, ch->acc, ch->wave_type);
            
            crapsynth->chan_outputs[i] = (wave - (511)) * 8 * crapsynth->volume_table[crapsynth->volume[i]];

            wave = (int32_t)((float)wave * crapsynth->volume_table[crapsynth->volume[i]]);
            crapsynth->final_output += wave * 4;
        }
    }
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