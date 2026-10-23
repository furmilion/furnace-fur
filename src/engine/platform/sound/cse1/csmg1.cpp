//
// Created by Administrator on 2026/10/6.
//

#include "csmg1.h"

namespace CSMG1_PACKED {
    constexpr long double PI = 3.1415926535897932384626433832795028841971L; // 背出的PI

    void CSMG1::clock(const CSMG1_CONFIG_AND_TABLE &config_and_table) noexcept {
        this->CHANNELS.clock(config_and_table);
    }

    // 一毛一样
    CSMG1_DOUBLE_SIG_REG CSMG1::CSE1_GET_SAMPLE(uint8_t ch) const noexcept {
        int64_t ret = (static_cast<int64_t>(this->CHANNELS.OUT_L[ch]) + static_cast<int64_t>(this->CHANNELS.OUT_R[ch]));
        if (ret < INT16_MIN) ret = INT16_MIN;
        if (ret > INT16_MAX) ret = INT16_MAX;
        return static_cast<int32_t>(ret);
    }

    void CSMG1::hard_reset() {
        this->CHANNELS = CSMG1_CHANNELS();
    }

    void CSMG1_CHANNELS::clock(const CSMG1_CONFIG_AND_TABLE &config_and_table) noexcept {
        for (size_t i = 0; i < config_and_table.CHANNEL_SIZE && i < CSMG1_MAX_CHANNELS; i++) {
            this->REGISTER[i].clock(config_and_table, OUT_L[i], OUT_R[i]);
        }
    }

    void CSMG1_CHANNEL_REGISTER::clock(const CSMG1_CONFIG_AND_TABLE &wave_table, CSMG1_DOUBLE_SIG_REG &LeftBuf, CSMG1_DOUBLE_SIG_REG &RightBuf) noexcept {
        const auto pitch = this->PITCH;
        if (GET_WAVE() <= NOISE) {
            PHASE += this->GET_REV() ? -pitch : pitch;
        } else {
            const auto in_pitch = pitch;
            const auto next_phase = static_cast<uint64_t>(PHASE)+
                static_cast<uint64_t>(in_pitch>>16)+
                    ((static_cast<uint64_t>(DUTY)+static_cast<uint64_t>(in_pitch&0xffff))>>16);

            if (next_phase < static_cast<uint64_t>(END_PHASE)+1) {
                PHASE = next_phase;
                DUTY += in_pitch & 0xffff;
            } else if (this->GET_WAVE() == LOOP_SAMPLE) {
                PHASE = next_phase - static_cast<uint64_t>(END_PHASE) + static_cast<uint64_t>(START_PHASE) - 1;
                if (PHASE >= static_cast<uint64_t>(END_PHASE)+1) PHASE = static_cast<uint64_t>(START_PHASE);
                DUTY += in_pitch & 0xffff;
            } else {
                PHASE = END_PHASE;
                DUTY = 0;
            }
        }

        CSMG1_REG out;
        if (GET_WAVE() <= NOISE) {
            out = option_wave(wave_table, (PHASE>>16)&0xffff);
        } else {
            out = wave_table.memPCM[PHASE];
        }
        const auto tl_table = GET_N_DTL() ? &wave_table.memPCM[TL_TABLE] : wave_table.volume_line.data();
        out = GET_N_DOT() ? wave_table.memPCM[OUT_TABLE+out] : out;
        const auto left_out = (static_cast<CSMG1_DOUBLE_SIG_REG>(out)-0x8000)*tl_table[OUT_L]>>16;
        const auto right_out = (static_cast<CSMG1_DOUBLE_SIG_REG>(out)-0x8000)*tl_table[OUT_R]>>16;
        LeftBuf += GET_NEG_L() ? -left_out : left_out;
        RightBuf += GET_NEG_R() ? -right_out: right_out;
    }

    // 一毛一样
    CSMG1_REG BIT_EX(const bool v) noexcept {
        return ~(static_cast<CSMG1_REG>(v)-1u);
    }

    // 一毛一样
    CSMG1_DOUBLE_REG BIT_EX_32(const bool v) noexcept {
        return ~(static_cast<CSMG1_DOUBLE_REG>(v)-1u);
    }
    // 一毛一样
    CSMG1_REG CSMG1_CHANNEL_REGISTER::option_wave(const CSMG1_CONFIG_AND_TABLE &wave_table, const CSMG1_REG index) noexcept {
        switch (this->GET_WAVE()) {
            case SINE: {
                return wave_table.sine[index];
            }
            case TRIANGLE: {
                return wave_table.triangle[index];
            }
            case SQUARE: {
                return BIT_EX(index>this->DUTY);
            }
            case SAW: {
                return index;
            }
            case NOISE: {
                if (this->PHASE & 0x10000000) {
                    this->PHASE &= 0x0fffffff;
                    this->DUTY = (this->DUTY >> 1) | (((this->DUTY ^ (this->DUTY >> 2) ^ (this->DUTY >> 3) ^ (this->DUTY >> 5)) & 1) << 15);
                }
                return BIT_EX(this->DUTY & 1);
            }
            case WAVETABLE_SAMPLE: {
                return wave_table.memPCM[index+START_PHASE];
            }
            default: return 0;
        }
    }

    // 一毛一样
    CSMG1_CONFIG_AND_TABLE::CSMG1_CONFIG_AND_TABLE(): default_volume_line(1), memPCM(nullptr), fastSpeed(0x4000), chipType(0), CHANNEL_SIZE(32) {
        for (size_t i = 0; i < sine.size(); i++) {
            sine[i] = static_cast<CSMG1_REG>(std::sin(static_cast<long double>(i)*2*PI/65536.0L)*32767.0+32767.0)&0xffff;
        }
        for (size_t i = 0; i < triangle.size(); i++) {
            triangle[i] = static_cast<CSMG1_REG>(std::asin(std::sin(static_cast<long double>(i)*2*PI/65536.0L))/PI*2*32767.0+32767.0)&0xffff;
        }
    }

    // 《一毛一样》
    void CSMG1_CONFIG_AND_TABLE::reset() noexcept {
        switch (default_volume_line) {
            case 0: {
                for (size_t i = 0; i < volume_line.size(); i++) {
                    volume_line[i] = fastSpeed != 0 ? static_cast<CSMG1_REG>(
                        (std::exp(i/65535.0L * std::log(static_cast<long double>(fastSpeed)+1.0L)) - 1.0L) * 65535.0L / static_cast<long double>(fastSpeed)
                    ) & 0xffff : i;
                }
                break;
            }

            case 1: {
                for (size_t i = 0; i < volume_line.size(); i++) {
                    const long double tl = (65535.0L-i) / 16383.0L*fastSpeed/16384.0L;
                    const long double volume = std::pow(10.0L, -tl);
                    volume_line[i] = static_cast<CSMG1_REG>(volume * 65535.0L) & 0xffff;
                }
                break;
            }

            case 3: {
                for (size_t i = 0; i < volume_line.size(); i++) {
                    const long double tl = (65535.0L-i) / 8191.0L*fastSpeed/16384.0L;
                    const long double volume = std::exp(-tl);
                    volume_line[i] = static_cast<CSMG1_REG>(volume * 65535.0L) & 0xffff;
                }
                break;
            }

            case 2:
            default: {
                for (size_t i = 0; i < volume_line.size(); i++) {
                    volume_line[i] = static_cast<CSMG1_REG>(std::max(std::min(static_cast<long double>(fastSpeed)*(static_cast<long double>(i)-65535.0l)/256.0l+65535.0l, 65535.0l), .0l));
                }
                break;
            }
        }
    }
}
