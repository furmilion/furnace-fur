//
// Created by Administrator on 2026/9/12.
//

#include "cse1.hpp"

#include <algorithm>
#include <cmath>

namespace CSE1_PACKED {
    constexpr long double PI = 3.1415926535897932384626433832795028841971L; // 背出的PI

    void CSE1::hard_reset() {
        this->CHANNELS = CSE1_CHANNELS();
    }

    void CSE1::clock(const CSE1_CONFIG_AND_TABLE& wave_table) noexcept {
        this->CHANNELS.clock(wave_table);
    }

    CSE1_DOUBLE_SIG_REG CSE1::CSE1_GET_SAMPLE(uint8_t ch) const noexcept {
        int64_t ret = (static_cast<int64_t>(this->CHANNELS.OUTS_L[ch]) + static_cast<int64_t>(this->CHANNELS.OUTS_R[ch]));
        if (ret < INT16_MIN) ret = INT16_MIN;
        if (ret > INT16_MAX) ret = INT16_MAX;
        return static_cast<int32_t>(ret);
    }

    void CSE1_CHANNELS::clock(const CSE1_CONFIG_AND_TABLE& wave_table) noexcept {
        for (size_t i = 0; i < CSE1_CHANNEL_NUMBER; i++) {
            auto& channel = CHANNEL[i];
            auto& filterState = FILTER_PRIVATE[i];
            OUTS_R[i]=OUTS_L[i]=0;
            channel.clock(wave_table, filterState, OUTS_L[i], OUTS_R[i]);
        }
    }

    void CSE1_CHANNEL_REGISTERS::clock(const CSE1_CONFIG_AND_TABLE& wave_table, std::array<CSE1_FILTER_PRIVATE, 3>& value, CSE1_DOUBLE_SIG_REG& LeftBuf, CSE1_DOUBLE_SIG_REG& RightBuf) noexcept {
        CSE1_DOUBLE_SIG_REG LeftBuf2 = 0;
        CSE1_DOUBLE_SIG_REG RightBuf2 = 0;
        OPS.clock(wave_table, this, LeftBuf2, RightBuf2);
        SPEC.LFO1.PHASE+=SPEC.LFO_CONFIG.GET_FREQ();
        SPEC.LFO2.PHASE+=SPEC.LFO_CONFIG.GET_FREQ2();
        OUT.PITCH += SPEC.SWEEP_FREQS[CSE1_OPER_NUMBER].FREQ_SPEED;
        CSE1_DOUBLE_SIG_REG LeftBuf3 = 0;
        CSE1_DOUBLE_SIG_REG RightBuf3 = 0;
        for (size_t f = 0; f < SPEC.FILTERS.size(); f++) {
            SPEC.FILTERS[f].clock(value[f], value, LeftBuf2, RightBuf2);
            LeftBuf3 += value[f].VFB_L*SPEC.FILTERS[f].OUT_L>>16;
            RightBuf3 += value[f].VFB_R*SPEC.FILTERS[f].OUT_R>>16;
        }

        LeftBuf = (LeftBuf2 * wave_table.volume_line[OUT.OUT_L] >> 16) + LeftBuf3;
        RightBuf = (RightBuf2 * wave_table.volume_line[OUT.OUT_R] >> 16) + RightBuf3;
    }

    void CSE1_FILTER::clock(
        CSE1_FILTER_PRIVATE& filter_private,
        const std::array<CSE1_FILTER_PRIVATE, 3>& value,
        const CSE1_DOUBLE_SIG_REG in1,
        const CSE1_DOUBLE_SIG_REG in2) const {
        CSE1_DOUBLE_SIG_REG inL = 0;
        CSE1_DOUBLE_SIG_REG inR = 0;
        if (this->GET_INPUTS() & 1) {
            inL += in1;
            inR += in2;
        }
        if (this->GET_INPUTS() & 2) {
            inL += value[0].VFB_L;
            inR += value[0].VFB_R;
        }
        if (this->GET_INPUTS() & 4) {
            inL += value[1].VFB_L;
            inR += value[1].VFB_R;
        }
        if (this->GET_INPUTS() & 8) {
            inL += value[2].VFB_L;
            inR += value[2].VFB_R;
        }

        const CSE1_REG f = this->CUTOFF;
        const CSE1_REG q = 0x100 - this->GET_RES();

        const auto lowL = filter_private.BUF_LOW_L;
        const auto bandL = filter_private.BUF_BAND_L;
        const auto highL = inL - lowL - ((q * bandL) >> 8);
        const auto newLowL = lowL + ((f * bandL) >> 16);
        const auto newBandL = bandL + ((f * highL) >> 16);


        const auto lowR = filter_private.BUF_LOW_R;
        const auto bandR = filter_private.BUF_BAND_R;
        const auto highR = inR - lowR - ((q * bandR) >> 8);
        const auto newLowR = lowR + ((f * bandR) >> 16);
        const auto newBandR = bandR + ((f * highR) >> 16);


        filter_private.BUF_LOW_L = newLowL;
        filter_private.BUF_BAND_L = newBandL;
        filter_private.BUF_LOW_R = newLowR;
        filter_private.BUF_BAND_R = newBandR;


        CSE1_DOUBLE_SIG_REG outL = 0;
        CSE1_DOUBLE_SIG_REG outR = 0;
        if (this->GET_TYPES() & 1) outL += newLowL;
        if (this->GET_TYPES() & 2) outL += highL;
        if (this->GET_TYPES() & 4) outL += newBandL;
        if (this->GET_TYPES() & 1) outR += newLowR;
        if (this->GET_TYPES() & 2) outR += highR;
        if (this->GET_TYPES() & 4) outR += newBandR;
        if (this->GET_TYPES() == 0) {
            outL += inL;
            outR += inR;
        }

        filter_private.VFB_L = outL;
        filter_private.VFB_R = outR;
    }

    void CSE1_OPS::clock(const CSE1_CONFIG_AND_TABLE &wave_table, const CSE1_CHANNEL_REGISTERS* State, CSE1_DOUBLE_SIG_REG &LeftBuf, CSE1_DOUBLE_SIG_REG &RightBuf) noexcept {

        const auto fm1pitch = static_cast<uint64_t>(static_cast<CSE1_SIG_REG>(CSE1_LFO_CONFIG::option_wavetable(State->SPEC.LFO_CONFIG.GET_SHAPE(), wave_table, State->SPEC.LFO1.PHASE)-0x8000)
                    *State->SPEC.LFO_CONFIG.GET_DEPTH());
        const auto fm2pitch = static_cast<uint64_t>(static_cast<CSE1_SIG_REG>(CSE1_LFO_CONFIG::option_wavetable(State->SPEC.LFO_CONFIG.GET_SHAPE2(), wave_table, State->SPEC.LFO2.PHASE)-0x8000)
                    *State->SPEC.LFO_CONFIG.GET_DEPTH2());

        for (size_t opi = 0; opi < CSE1_OPER_NUMBER; opi++) {
            CSE1_OPER_STATE& op = OP[opi];
            op.clock(wave_table, State, fm1pitch, fm2pitch);
            op.PITCH+=State->SPEC.SWEEP_FREQS[opi].FREQ_SPEED;
            const auto ov = !op.FLAGS_A.GET_NOT_DEFAULT_OUT_AND_WAVE_TABLE_TABLE() ? op.VFB : wave_table.memPCM[op.OUT_TABLE+op.VFB];
            const CSE1_DOUBLE_SIG_REG LB = (static_cast<int16_t>(ov)*State->OUT.IN_L.MI[opi]>>16)>>3;
            const CSE1_DOUBLE_SIG_REG RB = (static_cast<int16_t>(ov)*State->OUT.IN_R.MI[opi]>>16)>>3;

            LeftBuf += (State->OUT.FLAGS_A.GET_NEG_LEFT() >> opi)&1 ? -LB : LB;
            RightBuf += (State->OUT.FLAGS_A.GET_NEG_RIGHT() >> opi)&1 ? -RB : RB;
        }
    }

    CSE1_DOUBLE_REG MULT_CALC(const CSE1_DOUBLE_REG left, const uint8_t right) noexcept {
        return right == 0 ? left >> 1 : left * right;
    }

    CSE1_REG BIT_EX(const bool v) noexcept {
        return ~(static_cast<CSE1_REG>(v)-1u);
    }

    CSE1_DOUBLE_REG BIT_EX_32(const bool v) noexcept {
        return  ~(static_cast<CSE1_DOUBLE_REG>(v)-1u);
    }

    void CSE1_OPER_STATE::clock(const CSE1_CONFIG_AND_TABLE &wave_table, const CSE1_CHANNEL_REGISTERS *state, const uint64_t ExtFM1AddPitch, const uint64_t ExtFM2AddPitch) noexcept {
        const auto o_pitch = this->FLAGS_A.GET_FIXED() ? this->PITCH : state->OUT.PITCH+this->PITCH;
        const auto pitch = MULT_CALC(
                o_pitch +
                (this->FLAGS_A.GET_OPN2_DETUNE()-4) * (o_pitch >> 10) +
                (FLAGS_A.GET_FM1() ? ExtFM1AddPitch * o_pitch >> 24: 0) +
                (FLAGS_A.GET_FM2() ? ExtFM2AddPitch * o_pitch >> 24: 0),
                this->FLAGS_A.GET_ML()
                );
        if (FLAGS_A.GET_WAVE() < OPER_ONESHOT_SAMPLE) {
            PHASE += this->FLAGS_A.GET_REV() ? -pitch : pitch;
        } else {
            const auto in_pitch = pitch;
            const auto next_phase = static_cast<uint64_t>(PHASE)+
                static_cast<uint64_t>(in_pitch>>16)+
                ((static_cast<uint64_t>(DUTY)+static_cast<uint64_t>(in_pitch&0xffff))>>16);

            if (next_phase < static_cast<uint64_t>(END_PHASE)+1) {
                PHASE = next_phase;
                DUTY += in_pitch & 0xffff;
            } else if (this->FLAGS_A.GET_WAVE() == OPER_LOOP_SAMPLE) {
                PHASE = next_phase - static_cast<uint64_t>(END_PHASE) + static_cast<uint64_t>(START_PHASE) - 1;
                if (PHASE >= static_cast<uint64_t>(END_PHASE)+1) PHASE = static_cast<uint64_t>(START_PHASE);
                DUTY += in_pitch & 0xffff;
            } else {
                PHASE = END_PHASE;
                DUTY = 0;
            }
        }

        switch (wave_table.chipType) {
            case 0: {
                this->ENV_STATE.clock_1(this, this->ADSR);
                break;
            }
            case 1: {
                this->ENV_STATE.clock_1_1(this, this->ADSR);
                break;
            }
            default: break;
        }

        CSE1_REG add_phase = 0;
        if (!wave_table.not_fm) for (size_t i = 0; i < CSE1_OPER_NUMBER; i++) {
            auto& op = state->OPS.OP[i];
            add_phase += static_cast<int32_t>(static_cast<int16_t>(op.VFB)) * this->MIS.MI[i] >> 13;
        }
        add_phase += PHASE>>16;

        if (FLAGS_A.GET_WAVE() < 6) {
            const CSE1_REG wv = option_wavetable(wave_table, add_phase&0xffff);
            VFB = ((!FLAGS_A.GET_NOT_DEFAULT_OUT_AND_WAVE_TABLE_TABLE() ? wv : wave_table.memPCM[WAVE_TABLE_TABLE+wv]^0x8000)^0x8000)+1;
        } else {
            VFB = wave_table.memPCM[PHASE];
        }
        VFB = static_cast<int16_t>(VFB) * (!FLAGS_A.GET_NOT_DEFAULT_ADSR_TABLE() ? wave_table.volume_line[ENV_STATE.ENV_VP] : wave_table.memPCM[ADSR_WAVE_TABLE+ENV_STATE.ENV_VP]) >> 16;
    }

    static uint16_t sat_add_u16(const uint16_t a, const uint16_t b) noexcept {
        const uint32_t sum = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);
        return sum > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(sum);
    }

    static uint16_t sat_sub_u16(const uint16_t a, const uint16_t b) noexcept {
        return a < b ? 0 : a - b;
    }

    void CSE1_OPER_ENV_STATE::clock_1(CSE1_OPER_STATE *state, const CSE1_ADSR &adsr) noexcept {
        if (state->ENV_DIVIDER_COUNT == 0) {
            state->ENV_DIVIDER_COUNT = this->GET_ENV_DIVIDER();
            switch(this->GET_ENV_ENUM()) {
                case 0: {
                    ENV_VP = sat_add_u16(ENV_VP, adsr.AR);
                    if (ENV_VP == 0xffff) { SET_ENV_ENUM(1); }
                    break;
                }
                case 1: {
                    ENV_VP = sat_sub_u16(ENV_VP, adsr.DR);
                    if (ENV_VP <= adsr.SL) { ENV_VP = adsr.SL, SET_ENV_ENUM(2); }
                    break;
                }
                case 2: {
                    ENV_VP = sat_sub_u16(ENV_VP, adsr.SR);
                    break;
                }
                case 3: {
                    ENV_VP = sat_sub_u16(ENV_VP, adsr.RR);
                    break;
                }
                default: ;
            }
        }
        if (state->ENV_DIVIDER_COUNT != 0) {
            state->ENV_DIVIDER_COUNT = state->ENV_DIVIDER_COUNT-1;
        } else {
            state->ENV_DIVIDER_COUNT = 4095;
        }
    }

    void CSE1_OPER_ENV_STATE::clock_1_1(CSE1_OPER_STATE *state, const CSE1_ADSR &adsr) noexcept {
        CSE1_REG div;
        switch (this->GET_ENV_ENUM()) {
            case 0:  div = ~adsr.AR; break;
            case 1:  div = ~adsr.DR; break;
            case 2:  div = ~adsr.SR; break;
            default: div = ~adsr.RR; break;
        }

        if (div == 0) {
            state->ENV_DIVIDER_COUNT = 1;
            switch (this->GET_ENV_ENUM()) {
                case 0: {
                    ENV_VP = 0xffff;
                    SET_ENV_ENUM(1);
                    return;
                }
                case 1: {
                    ENV_VP = adsr.SL;
                    SET_ENV_ENUM(2);
                    return;
                }
                case 2: {
                    ENV_VP = 0;
                    return;
                }
                case 3: {
                    ENV_VP = 0;
                    return;
                }
                default: return;
            }
        }

        if (div == 0xffff) return;

        if (div < state->ENV_DIVIDER_COUNT) { state->ENV_DIVIDER_COUNT = div; }

        if (--state->ENV_DIVIDER_COUNT == 0) {
            state->ENV_DIVIDER_COUNT = div;
            switch (this->GET_ENV_ENUM()) {
                case 0: {
                    ENV_VP = sat_add_u16(ENV_VP, this->GET_ENV_DIVIDER());
                    if (ENV_VP == 0xffff) { SET_ENV_ENUM(1); state->ENV_DIVIDER_COUNT = ~adsr.DR; }
                    break;
                }
                case 1: {
                    ENV_VP = sat_sub_u16(ENV_VP, this->GET_ENV_DIVIDER());
                    if (ENV_VP <= adsr.SL) { ENV_VP = adsr.SL, SET_ENV_ENUM(2); state->ENV_DIVIDER_COUNT = ~adsr.SR; }
                    break;
                }
                case 2: { ENV_VP = sat_sub_u16(ENV_VP, this->GET_ENV_DIVIDER()); break; }
                case 3: { ENV_VP = sat_sub_u16(ENV_VP, this->GET_ENV_DIVIDER()); break; }
                default: ;
            }
        }
    }


    constexpr CSE1_REG CSE1_LFO_CONFIG::option_wavetable(const uint8_t shape, const CSE1_CONFIG_AND_TABLE &wave_table,
        const CSE1_REG index) noexcept {
        switch (shape) {
            case SINE: {
                return wave_table.sine[index];
            }
            case TRIANGLE: {
                return wave_table.triangle[index];
            }
            case SQUARE: {
                return BIT_EX(index&0x8000);
            }
            case SAW: {
                return index;
            }
            default: return 0;
        }
    }

    CSE1_REG CSE1_OPER_STATE::option_wavetable(const CSE1_CONFIG_AND_TABLE &wave_table, const CSE1_REG index) noexcept {
        switch (this->FLAGS_A.GET_WAVE()) {
            case SINE: {
                return wave_table.sine[index];
            }
            case TRIANGLE: {
                return wave_table.triangle[index];
            }
            case SQUARE: {
                return 0xffff-(index>this->DUTY);
            }
            case SAW: {
                return index;
            }
            case OPER_NOISE: {
                if (this->PHASE & 0x10000000) {
                    this->PHASE &= 0x0fffffff;
                    this->DUTY = (this->DUTY >> 1) | (((this->DUTY ^ (this->DUTY >> 2) ^ (this->DUTY >> 3) ^ (this->DUTY >> 5)) & 1) << 15);
                }
                return 0xffff-(this->DUTY & 1);
            }
            case OPER_WAVETABLE_SAMPLE: {
                return (wave_table.memPCM[index+START_PHASE]^0x8000)-1;
            }
            default: return 0;
        }
    }

    CSE1_CONFIG_AND_TABLE::CSE1_CONFIG_AND_TABLE(): default_volume_line(1), memPCM(nullptr), fastSpeed(0x4000), chipType(1), not_fm(false) {
        for (size_t i = 0; i < sine.size(); i++) {
            sine[i] = static_cast<CSE1_REG>(std::sin(static_cast<long double>(i)*2*PI/65536.0L)*32767.0+32767.0)&0xffff;
        }
        for (size_t i = 0; i < triangle.size(); i++) {
            triangle[i] = static_cast<CSE1_REG>(std::asin(std::sin(static_cast<long double>(i)*2*PI/65536.0L))/PI*2*32767.0+32767.0)&0xffff;
        }

        for (size_t i = 0; i < divider_table.size(); i++) {
            divider_table[i] = i != 0 ? 65535/i: 65535;
        }

        for (size_t i = 0; i < tan_table.size(); i++) {
            const long double angle = static_cast<long double>(i) / 65535.0L * (PI / 2.0L);
            const long double t = std::tan(angle);
            if (t > 1.0e10L) {
                tan_table[i] = 65535;
            } else {
                const long double normalized = t / (1.0L + t);
                tan_table[i] = static_cast<CSE1_REG>(normalized * 65535.0L) & 0xffff;
            }
        }
    }

    void CSE1_CONFIG_AND_TABLE::reset() noexcept {
        switch (default_volume_line) {
            case 0: {
                for (size_t i = 0; i < volume_line.size(); i++) {
                    volume_line[i] = fastSpeed != 0 ? static_cast<CSE1_REG>(
                        (std::exp(i/65535.0L * std::log(static_cast<long double>(fastSpeed)+1.0L)) - 1.0L) * 65535.0L / static_cast<long double>(fastSpeed)
                    ) & 0xffff : i;
                }
                break;
            }

            case 1: {
                for (size_t i = 0; i < volume_line.size(); i++) {
                    const long double tl = (65535.0L-i) / 16383.0L*fastSpeed/16384.0L;
                    const long double volume = std::pow(10.0L, -tl);
                    volume_line[i] = static_cast<CSE1_REG>(volume * 65535.0L) & 0xffff;
                }
                break;
            }

            case 3: {
                for (size_t i = 0; i < volume_line.size(); i++) {
                    const long double tl = (65535.0L-i) / 8191.0L*fastSpeed/16384.0L;
                    const long double volume = std::exp(-tl);
                    volume_line[i] = static_cast<CSE1_REG>(volume * 65535.0L) & 0xffff;
                }
                break;
            }

            case 2:
            default: {
                for (size_t i = 0; i < volume_line.size(); i++) {
                    volume_line[i] = static_cast<CSE1_REG>(std::max(std::min(static_cast<long double>(fastSpeed)*(static_cast<long double>(i)-65535.0l)/256.0l+65535.0l, 65535.0l), .0l));
                }
                break;
            }
        }
    }
}
