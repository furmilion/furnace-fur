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

    crapsynth->noise.lfsr = rand();
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
                if(crapsynth->ad9833[chan].zero_cross)
                {
                    crapsynth->ad9833[chan].pending_vol = data & 0xff;
                }
                else
                {
                    crapsynth->volume[channel] = data & 0xff;
                }
                
                break;
            }
            case 1:
            {
                crapsynth->ad9833[chan].wave_type = data % 6;

                if(crapsynth->ad9833[chan].wave_type == 5) crapsynth->ad9833[chan].freq = 0;
                else crapsynth->ad9833[chan].timer_freq = 0;
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
            case 4: //PWM timer freq
            {
                crapsynth->ad9833[chan].timer_freq = data;
                break;
            }
            case 5: //PWM timer duty
            {
                crapsynth->ad9833[chan].pw = data & 0xffff;
                break;
            }
            case 6: //zero cross
            {
                crapsynth->ad9833[chan].zero_cross = data & 1;

                if(chan & 1) //1 zero cross setting for 2 channels
                {
                    crapsynth->ad9833[chan - 1].zero_cross = data & 1;
                }
                else
                {
                    crapsynth->ad9833[chan + 1].zero_cross = data & 1;
                }
                break;
            }
            default: break;
        }
    }

    if(channel == 4) //noise chan
    {
        int chan = channel;

        switch(data_type)
        {
            case 0:
            {
                if(crapsynth->noise.zero_cross)
                {
                    crapsynth->noise.pending_vol = data & 0xff;
                }
                else
                {
                    crapsynth->volume[channel] = data & 0xff;
                }
                
                break;
            }
            case 4:
            {
                crapsynth->noise.timer_freq = data;
                break;
            }
            case 3: //reset
            {
                crapsynth->noise.lfsr = (data == 0 ? 1 : data);
                break;
            }
            case 6: //zero cross
            {
                crapsynth->noise.zero_cross = data & 1;
                break;
            }
            default: break;
        }
    }
}

int32_t crapsynth_ad9833_get_wave(STM32CrapSynth* crapsynth, uint32_t acc, uint16_t pw, uint8_t wave_type)
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
        case 5: //pulse wave from MCU
        {
            return (((acc >> (((STM32CRAPSYNTH_ACC_BITS + 2) - 16))) >= ((pw == 0xffff ? pw + 1 : pw)) ? (1023) : 0));
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

        if(ch->freq > 0 || ch->timer_freq > 0)
        {
            ch->acc += ch->freq;
            ch->acc &= (1 << STM32CRAPSYNTH_ACC_BITS) - 1;

            if(ch->wave_type == 5)
            {
                ch->timer_acc += ch->timer_freq;
                ch->timer_acc &= (1 << (STM32CRAPSYNTH_ACC_BITS + 2)) - 1;
            }

            int32_t wave = crapsynth_ad9833_get_wave(crapsynth, ch->wave_type == 5 ? ch->timer_acc : ch->acc, ch->pw, ch->wave_type);

            if(ch->zero_cross && wave < 10) //approx of zero cross
            {
                crapsynth->volume[i] = ch->pending_vol;
            }
            
            if(!crapsynth->muted[i])
            {
                crapsynth->chan_outputs[i] = (wave - (511)) * 8 * crapsynth->volume_table[crapsynth->volume[i]];
            }

            wave = (int32_t)((float)wave * crapsynth->volume_table[crapsynth->volume[i]]);

            if(!crapsynth->muted[i])
            {
                crapsynth->final_output += wave * 4;
            }
        }
    }

    if(crapsynth->noise.timer_freq > 0)
    {
        crapsynth->noise.timer_acc += crapsynth->noise.timer_freq;

        if(crapsynth->noise.timer_acc & (1 << (STM32CRAPSYNTH_ACC_BITS + 2))) //overflow
        {
            //shift lfsr
            bool bit0 = ((crapsynth->noise.lfsr >> 22) ^ (crapsynth->noise.lfsr >> 17)) & 0x1;
            crapsynth->noise.lfsr <<= 1;
            crapsynth->noise.lfsr &= 0x7fffff;
            crapsynth->noise.lfsr |= bit0;

            crapsynth->noise.output = (crapsynth->noise.lfsr & 0x400000) ? 1023 : 0;

            if(crapsynth->noise.zero_cross && crapsynth->noise.output == 0) //approx of zero cross
            {
                crapsynth->volume[4] = crapsynth->noise.pending_vol;
            }
        }

        crapsynth->noise.timer_acc &= (1 << (STM32CRAPSYNTH_ACC_BITS + 2)) - 1;
    }

    if(!crapsynth->muted[4])
    {
        crapsynth->chan_outputs[4] = (crapsynth->noise.output - (511)) * 8 * crapsynth->volume_table[crapsynth->volume[4]];
        crapsynth->final_output += crapsynth->noise.output * 4;
    }
}

void crapsynth_set_is_muted(STM32CrapSynth* crapsynth, uint8_t ch, bool mute)
{
    crapsynth->muted[ch] = mute;
}

void crapsynth_set_clock_rate(STM32CrapSynth* crapsynth, uint32_t clock)
{

}

void crapsynth_free(STM32CrapSynth* crapsynth)
{
    free(crapsynth);
}