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
            crapsynth->volume_table[i] = pow(10.0, gain_dB / 20.0);
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
    bool muted[STM32CRAPSYNTH_NUM_CHANNELS];
    memcpy(muted, crapsynth->muted, STM32CRAPSYNTH_NUM_CHANNELS * sizeof(bool));
    uint8_t* temp_memory = (uint8_t*)calloc(1, STM32CRAPSYNTH_FLASH_SAMPLE_MEM_SIZE);
    uint8_t* temp_memory_ram = (uint8_t*)calloc(1, STM32CRAPSYNTH_RAM_SAMPLE_MEM_SIZE);
    memcpy(temp_memory, crapsynth->sample_mem_flash, STM32CRAPSYNTH_FLASH_SAMPLE_MEM_SIZE);
    memcpy(temp_memory_ram, crapsynth->sample_mem_ram, STM32CRAPSYNTH_RAM_SAMPLE_MEM_SIZE);
    memset(crapsynth, 0, sizeof(STM32CrapSynth));
    memcpy(crapsynth->sample_mem_flash, temp_memory, STM32CRAPSYNTH_FLASH_SAMPLE_MEM_SIZE);
    memcpy(crapsynth->sample_mem_ram, temp_memory_ram, STM32CRAPSYNTH_RAM_SAMPLE_MEM_SIZE);

    crapsynth_recalc_luts(crapsynth);

    free(temp_memory);
    free(temp_memory_ram);

    crapsynth->noise.lfsr = rand();
    memcpy(crapsynth->muted, muted, STM32CRAPSYNTH_NUM_CHANNELS * sizeof(bool));
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
                crapsynth->ad9833[chan].freq = data / 10;
                break;
            }
            case 3: //reset
            {
                crapsynth->ad9833[chan].acc = 0;
                crapsynth->ad9833[chan].timer_acc = 0;
                break;
            }
            case 4: //PWM timer freq
            {
                crapsynth->ad9833[chan].timer_freq = data / 10;
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
        if(crapsynth->noise.lfsr == 0) crapsynth->noise.lfsr = 1;

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
            case 1:
            {
                crapsynth->noise.internal_clock = data & 1;
                
                if(data & 1)
                {
                    crapsynth->noise.timer_freq_memory = crapsynth->noise.timer_freq;
                    crapsynth->noise.timer_freq = 2500000 / 4; // around 100 kHz with internal clock?

                    crapsynth->timer[4].enable = true;
                }
                else
                {
                    crapsynth->noise.timer_freq = crapsynth->noise.timer_freq_memory;
                    crapsynth->timer[4].enable = false;
                }
                break;
            }
            case 4:
            {
                if(!crapsynth->noise.internal_clock)
                {
                    crapsynth->noise.timer_freq = data / 10;
                }
                if(crapsynth->noise.internal_clock)
                {
                    crapsynth->noise.timer_freq_memory = data / 10;
                }
                
                break;
            }
            case 3: //reset
            {
                crapsynth->noise.lfsr = (data == 0 ? 1 : data);
                crapsynth->noise.lfsr_reload = (data == 0 ? 1 : data);
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

    if(channel > 4 && channel < 7) //DAC chans
    {
        int chan = channel - 5;

        switch(data_type)
        {
            case 0:
            {
                if(crapsynth->dac[chan].zero_cross)
                {
                    crapsynth->dac[chan].pending_vol = data & 0xff;
                }
                else
                {
                    crapsynth->volume[channel] = data & 0xff;
                }
                
                break;
            }
            case 1:
            {
                int prev = (crapsynth->dac[chan].playing ? 1 : 0) | (crapsynth->dac[chan].play_wavetable ? 2 : 0);
                crapsynth->dac[chan].playing = data & 1;
                crapsynth->dac[chan].play_wavetable = data & 2;
                crapsynth->dac[chan].loop = data & 4;
                crapsynth->dac[chan].ram = data & 8;

                if(!(prev & 2) && (data & 2))
                {
                    crapsynth->dac[chan].curr_pos = 0;
                }
                if(!(prev & 1) && (data & 1))
                {
                    if(crapsynth->dac[chan].play_wavetable)
                    {
                        crapsynth->dac[chan].curr_pos = 0;
                    }
                    else
                    {
                        crapsynth->dac[chan].curr_pos = crapsynth->dac[chan].start_addr;
                    }
                }
                break;
            }
            case 2:
            {
                crapsynth->dac[chan].start_addr = data & 0xffffff;
                break;
            }
            case 3: //reset
            {
                crapsynth->dac[chan].curr_pos = 0;
                crapsynth->dac[chan].timer_acc = 0;

                crapsynth->dac[chan].triangle_counter = 0;
                crapsynth->dac[chan].triangle_counter_dir = 0;
                crapsynth->dac[chan].lfsr = 1;
                break;
            }
            case 4:
            {
                crapsynth->dac[chan].timer_freq = data / 10;
                break;
            }
            case 6: //zero cross
            {
                crapsynth->dac[chan].zero_cross = data & 1;

                if(chan & 1) //1 zero cross setting for 2 channels
                {
                    crapsynth->dac[chan + 1].zero_cross = data & 1;
                }
                else
                {
                    crapsynth->dac[chan - 1].zero_cross = data & 1;
                }
                break;
            }
            case 7:
            {
                crapsynth->dac[chan].loop_point = data & 0xffffff;
                break;
            }
            case 8:
            {
                crapsynth->dac[chan].length = data & 0xffffff;
                break;
            }
            case 9:
            {
                if(crapsynth->dac[chan].wave_type != 2 && crapsynth->dac[chan].wave_type != 3 && (data & 7) == 2 || (data & 7) == 3)
                {
                    crapsynth->dac[chan].triangle_counter = 0;
                    crapsynth->dac[chan].triangle_counter_dir = 0;
                }
                crapsynth->dac[chan].wave_type = data & 7;

                int pw = data >> 4;

                if((data & 7) == 6) //pulse
                {
                    if(!crapsynth->dac[chan].play_wavetable) crapsynth->dac[chan].curr_pos = 0;
                    crapsynth->dac[chan].play_wavetable = true;
                    for(int i = 0; i < STM32CRAPSYNTH_WAVETABLE_SIZE; i++)
                    {
                        if(i < pw)
                        {
                            crapsynth->dac[chan].wavetable[i] = 255;
                        }
                        else
                        {
                            crapsynth->dac[chan].wavetable[i] = 0;
                        }
                    }
                }
                if((data & 7) == 7) //saw
                {
                    if(!crapsynth->dac[chan].play_wavetable) crapsynth->dac[chan].curr_pos = 0;
                    crapsynth->dac[chan].play_wavetable = true;
                    for(int i = 0; i < STM32CRAPSYNTH_WAVETABLE_SIZE; i++)
                    {
                        crapsynth->dac[chan].wavetable[i] = i;
                    }
                }
                break;
            }
            case 10:
            {
                int address = data >> 8;
                int wave_data = data & 0xff;

                crapsynth->dac[chan].wavetable[address] = wave_data;
                break;
            }
            case 11:
            {
                crapsynth->dac[chan].noise_tri_amp = data % 12;
                break;
            }
            default: break;
        }
    }

    if(channel >= 7 && channel < STM32CRAPSYNTH_NUM_CHANNELS) //phase reset timer chans
    {
        int chan = channel - 7;

        if(channel == 11 && !crapsynth->noise.internal_clock) return;

        switch(data_type)
        {
            case 0:
            {
                crapsynth->timer[chan].chan_bitmask = data;
                break;
            }
            case 1:
            {
                crapsynth->timer[chan].timer_freq = data;
                break;
            }
            case 2: //reset
            {
                crapsynth->timer[chan].timer_acc = 0;
                break;
            }
            case 3:
            {
                crapsynth->timer[chan].enable = data;
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

        int prev_output = crapsynth->chan_outputs[i];

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

        int prev_output = crapsynth->chan_outputs[4];

        if(crapsynth->noise.timer_acc & (1 << (STM32CRAPSYNTH_ACC_BITS + 2))) //overflow
        {
            //shift lfsr
            bool bit0 = ((crapsynth->noise.lfsr >> 22) ^ (crapsynth->noise.lfsr >> 17)) & 0x1;
            crapsynth->noise.lfsr <<= 1;
            crapsynth->noise.lfsr &= 0x7fffff;
            crapsynth->noise.lfsr |= bit0;

            crapsynth->noise.output = (crapsynth->noise.lfsr & 0x400000) ? 1023 : 0;

            //if(crapsynth->noise.zero_cross && abs((int)crapsynth->noise.output - 511) < 10) //approx of zero cross
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
        crapsynth->final_output += crapsynth->noise.output * crapsynth->volume_table[crapsynth->volume[4]];
    }

    for(int i = 0; i < 2; i++)
    {
        DACChan* ch = &crapsynth->dac[i];

        if(ch->timer_freq > 0)
        {
            ch->timer_acc += ch->timer_freq;

            if((ch->timer_acc & (1 << (STM32CRAPSYNTH_ACC_BITS + 2))) && ch->playing) //overflow
            {
                int prev_output = crapsynth->chan_outputs[i + 5];

                if(ch->wave_type == 2 || ch->wave_type == 3)
                {
                    if(ch->triangle_counter_dir == 0)
                    {
                        ch->triangle_counter++;

                        if(ch->triangle_counter >= (1 << ch->noise_tri_amp))
                        {
                            ch->triangle_counter_dir = 1;
                            goto next;
                        }
                    }
                    if(ch->triangle_counter_dir == 1)
                    {
                        ch->triangle_counter--;

                        if(ch->triangle_counter < 0)
                        {
                            ch->triangle_counter_dir = 0;
                        }
                    }
                }

                if(ch->wave_type == 4 || ch->wave_type == 5) // noise
                {
                    if(ch->lfsr == 0) ch->lfsr = 1;
                    uint16_t bit = ((ch->lfsr >> 6) ^ (ch->lfsr >> 4) ^ (ch->lfsr >> 1) ^ (ch->lfsr >> 0)) & 1u;
                    ch->lfsr = (ch->lfsr >> 1) | (bit << 11);
                }

                next:;
                
                if(ch->wave_type >= 2 && ch->wave_type <= 5)
                {
                    ch->output = 0;
                }

                switch(ch->wave_type)
                {
                    case 0:
                    {
                        break; //none
                    }
                    case 1:
                    case 3:
                    case 5:
                    {
                        if(ch->play_wavetable)
                        {
                            ch->curr_pos &= 0xff;
                            ch->output = (uint16_t)ch->wavetable[ch->curr_pos] << 4;
                        }
                        else
                        {
                            if(ch->curr_pos >= ch->length && ch->loop)
                            {
                                ch->curr_pos = ch->loop_point;
                            }

                            if(ch->curr_pos >= ch->length && !ch->loop)
                            {
                                ch->curr_pos = ch->start_addr;
                                ch->timer_acc = 0;
                                ch->timer_freq = 0;
                                ch->playing = false;
                                //ch->output = crapsynth->sample_mem_flash[ch->length + ch->start_addr - 1] << 4;
                                break;
                            }

                            ch->output = ch->ram ? ((uint16_t)crapsynth->sample_mem_ram[ch->curr_pos] << 4) : ((uint16_t)crapsynth->sample_mem_flash[ch->curr_pos] << 4);
                        }
                        break;
                    }
                    case 6:
                    case 7:
                    {
                        ch->curr_pos &= 0xff;
                        ch->output = (uint16_t)ch->wavetable[ch->curr_pos] << 4;
                        break;
                    }
                    default: break;
                }

                if(ch->wave_type == 2 || ch->wave_type == 3)
                {
                    ch->output += ch->triangle_counter;
                }
                if(ch->wave_type == 4 || ch->wave_type == 5)
                {
                    ch->output += ch->lfsr & ((1 << (ch->noise_tri_amp + 1)) - 1);
                }

                if(ch->output < 0) ch->output = 0;
                if(ch->output > 4095) ch->output = 4095;

                ch->curr_pos++;

                if(ch->zero_cross && (prev_output & 0x80000000) != (((int)ch->output - 2047) & 0x80000000)) //approx of zero cross
                //if(ch->zero_cross && (prev_output & 0x80000000) != ((wave - 511) & 0x80000000)) //approx of zero cross
                {
                    crapsynth->volume[i + 5] = ch->pending_vol;
                }
            }

            ch->timer_acc &= (1 << (STM32CRAPSYNTH_ACC_BITS + 2)) - 1;
        }

        if(!crapsynth->muted[i + 5])
        {
            crapsynth->chan_outputs[i + 5] = (crapsynth->dac[i].output - (2047)) * 2 * crapsynth->volume_table[crapsynth->volume[i + 5]];
            crapsynth->final_output += crapsynth->dac[i].output * crapsynth->volume_table[crapsynth->volume[i + 5]];
        }
    }

    for(int i = 0; i < 5; i++)
    {
        PhaseResetTimer* ch = &crapsynth->timer[i];

        if(ch->enable)
        {
            ch->timer_acc += ch->timer_freq;

            if((ch->timer_acc & (1 << (STM32CRAPSYNTH_ACC_BITS + 2)))) //overflow
            {
                for(int j = 0; j < 7; j++)
                {
                    if(ch->chan_bitmask & (1 << j))
                    {
                        switch(j)
                        {
                            case 0:
                            case 1:
                            case 2:
                            case 3: //AD9833
                            {
                                AD9833Chan* ch = &crapsynth->ad9833[j];
                                ch->acc = 0;
                                break;
                            }
                            case 4: //noise
                            {
                                NoiseChan* ch = &crapsynth->noise;
                                ch->lfsr = ch->lfsr_reload;
                                break;
                            }
                            case 5:
                            case 6: //DACs
                            {
                                DACChan* ch = &crapsynth->dac[j - 5];

                                ch->timer_acc = 0;

                                if(ch->play_wavetable)
                                {
                                    ch->curr_pos = 0;
                                }
                                else
                                {
                                    ch->curr_pos = ch->start_addr;
                                }

                                break;
                            }
                            default: break;
                        }
                    }
                }
            }

            ch->timer_acc &= (1 << (STM32CRAPSYNTH_ACC_BITS + 2)) - 1;
        }
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