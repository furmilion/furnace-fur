//
// Created by Administrator on 2026/9/12.
//

#ifndef FURNACE_CSE1_H
#define FURNACE_CSE1_H

#include <array>

#include "utils.h"

#define CSE1_BIT_FIELD_MEMBER(name, type, shift, width, reg) \
    using name = CSE1_BIT_FIELD<type, shift, width>; \
    constexpr void SET_##name(const type value) noexcept { \
        reg = name::Set(reg, value); \
    } \
    constexpr type GET_##name() const noexcept { \
        return name::Get(reg); \
    }

#define CSE1_CHANNEL_NUMBER 9
#define CSE1_OPER_NUMBER 6

namespace CSE1_PACKED {
    struct CSE1_OPER_STATE;
    struct CSE1_ADSR;
    struct CSE1_CONFIG_AND_TABLE;
    class CSE1;
    struct CSE1_FILTER_PRIVATE;
    struct CSE1_CHANNEL_REGISTERS;
    typedef uint16_t CSE1_REG;
    typedef uint32_t CSE1_DOUBLE_REG;
    typedef int16_t CSE1_SIG_REG;
    typedef int32_t CSE1_DOUBLE_SIG_REG;

    enum WAVE_TABLE_TYPE: uint8_t {
        SINE = 0,
        TRIANGLE = 1,
        SQUARE = 2,
        SAW = 3,
        OPER_NOISE = 4,
        OPER_WAVETABLE_SAMPLE = 5,
        OPER_ONESHOT_SAMPLE = 6,
        OPER_LOOP_SAMPLE = 7,
    };

    struct CSE1_OPER_ENV_STATE {
        CSE1_REG ENV_VP;
        CSE1_REG ENV_STATE_FLAGS;
        CSE1_BIT_FIELD_MEMBER(ENV_ENUM, CSE1_REG, 0, 2, ENV_STATE_FLAGS)
        CSE1_BIT_FIELD_MEMBER(ENV_DIVIDER, CSE1_REG, 2, 14, ENV_STATE_FLAGS)

        void clock_1(CSE1_OPER_STATE *state, const CSE1_ADSR &adsr) noexcept;

        void clock_1_1(CSE1_OPER_STATE *state, const CSE1_ADSR &adsr) noexcept;
    };

    struct CSE1_OPER_FLAGS_A {
        CSE1_REG OPN2_SGU1_LIKE;
        CSE1_BIT_FIELD_MEMBER(AM1, CSE1_REG, 0, 1, OPN2_SGU1_LIKE)
        CSE1_BIT_FIELD_MEMBER(AM2, CSE1_REG, 1, 1, OPN2_SGU1_LIKE)
        CSE1_BIT_FIELD_MEMBER(FM1, CSE1_REG, 2, 1, OPN2_SGU1_LIKE)
        CSE1_BIT_FIELD_MEMBER(FM2, CSE1_REG, 3, 1, OPN2_SGU1_LIKE)
        CSE1_BIT_FIELD_MEMBER(ALOOP, CSE1_REG, 4, 1, OPN2_SGU1_LIKE)
        CSE1_BIT_FIELD_MEMBER(DLOOP, CSE1_REG, 5, 1, OPN2_SGU1_LIKE)
        CSE1_REG SPEC;
        CSE1_BIT_FIELD_MEMBER(ML, CSE1_REG, 0, 4, SPEC)
        CSE1_BIT_FIELD_MEMBER(OPN2_DETUNE, CSE1_REG, 4, 3, SPEC)
        CSE1_BIT_FIELD_MEMBER(FIXED, CSE1_REG, 7, 1, SPEC)
        CSE1_BIT_FIELD_MEMBER(WAVE, CSE1_REG, 8, 3, SPEC)
        CSE1_BIT_FIELD_MEMBER(REV, CSE1_REG, 11, 1, SPEC)
        CSE1_BIT_FIELD_MEMBER(NOT_DEFAULT_ADSR_TABLE, CSE1_REG, 12, 1, SPEC)
        CSE1_BIT_FIELD_MEMBER(NOT_DEFAULT_OUT_AND_WAVE_TABLE_TABLE, CSE1_REG, 13, 1, SPEC)
        CSE1_BIT_FIELD_MEMBER(RING, CSE1_REG, 14, 1, SPEC)
        CSE1_BIT_FIELD_MEMBER(SYNC, CSE1_REG, 15, 1, SPEC)
    };

    struct CSE1_ADSR {
        CSE1_REG AR;
        CSE1_REG DR;
        CSE1_REG SL;
        CSE1_REG SR;
        CSE1_REG RR;
    };

    struct CSE1_MIS {
        std::array<CSE1_REG, CSE1_OPER_NUMBER> MI;
    };

    struct CSE1_OPER_STATE {
        CSE1_MIS MIS;
        CSE1_ADSR ADSR;
        CSE1_REG VFB;
        CSE1_OPER_ENV_STATE ENV_STATE;
        CSE1_DOUBLE_REG PHASE;
        CSE1_DOUBLE_REG PITCH;
        CSE1_DOUBLE_REG START_PHASE;
        CSE1_DOUBLE_REG END_PHASE;
        CSE1_DOUBLE_REG ADSR_WAVE_TABLE;
        CSE1_DOUBLE_REG WAVE_TABLE_TABLE;
        CSE1_DOUBLE_REG OUT_TABLE;
        CSE1_OPER_FLAGS_A FLAGS_A;
        CSE1_REG DUTY;
        CSE1_REG ENV_DIVIDER_COUNT;

        void clock(const CSE1_CONFIG_AND_TABLE &wave_table, const CSE1_CHANNEL_REGISTERS *state, uint64_t ExtFM1AddPitch, uint64_t
            ExtFM2AddPitch) noexcept;

        CSE1_REG option_wavetable(const CSE1_CONFIG_AND_TABLE &wave_table, CSE1_REG index) noexcept;
    };

    struct CSE1_CHANNEL_FLAGS_A {
        CSE1_REG SPEC;
        CSE1_BIT_FIELD_MEMBER(NEG_LEFT, CSE1_REG, 0, 7, SPEC)
        CSE1_BIT_FIELD_MEMBER(DEFAULT_ADSR_TABLE, CSE1_REG, 7, 1, SPEC)
        CSE1_BIT_FIELD_MEMBER(NEG_RIGHT, CSE1_REG, 8, 7, SPEC)
        CSE1_BIT_FIELD_MEMBER(DEFAULT_OUT_TABLE, CSE1_REG, 15, 1, SPEC)
    };

    struct CSE1_OUT {
        CSE1_MIS IN_L;
        std::array<CSE1_REG, 5> dummy1;
        CSE1_REG OUT_L;
        std::array<CSE1_REG, 2> dummy2;
        CSE1_DOUBLE_REG OUT_TABLE_L;
        CSE1_MIS IN_R;
        CSE1_DOUBLE_REG PITCH;
        CSE1_DOUBLE_REG TL_TABLE;
        CSE1_CHANNEL_FLAGS_A FLAGS_A;
        CSE1_REG OUT_R;
        CSE1_DOUBLE_REG dummy3;
        CSE1_DOUBLE_REG OUT_TABLE_R;
    };

    struct CSE1_FILTER {
        CSE1_REG CUTOFF;
        CSE1_REG FILTER_INFO;
        CSE1_BIT_FIELD_MEMBER(TYPES, CSE1_REG, 0, 3, FILTER_INFO)

        void clock(CSE1_FILTER_PRIVATE &filter_private, const std::array<CSE1_FILTER_PRIVATE, 3>
            &value, CSE1_DOUBLE_SIG_REG
            in1, CSE1_DOUBLE_SIG_REG in2) const;

        CSE1_BIT_FIELD_MEMBER(INPUTS, CSE1_REG, 3, 4, FILTER_INFO)
        CSE1_BIT_FIELD_MEMBER(RES, CSE1_REG, 8, 8, FILTER_INFO)
        CSE1_REG OUT_L;
        CSE1_REG OUT_R;
    };

    struct CSE1_LFO_CONFIG {
        CSE1_REG CONFIG;
        CSE1_BIT_FIELD_MEMBER(SHAPE, CSE1_REG, 0, 2, CONFIG)
        CSE1_BIT_FIELD_MEMBER(FREQ, CSE1_REG, 2, 2, CONFIG)
        CSE1_BIT_FIELD_MEMBER(DEPTH, CSE1_REG, 4, 4, CONFIG)

        CSE1_BIT_FIELD_MEMBER(SHAPE2, CSE1_REG, 8, 2, CONFIG)
        CSE1_BIT_FIELD_MEMBER(FREQ2, CSE1_REG, 10, 2, CONFIG)
        CSE1_BIT_FIELD_MEMBER(DEPTH2, CSE1_REG, 12, 4, CONFIG)
        static constexpr CSE1_REG option_wavetable(uint8_t shape, const CSE1_CONFIG_AND_TABLE &wave_table,
            CSE1_REG index) noexcept;

    };

    struct CSE1_SWEEP_FREQ {
        CSE1_DOUBLE_REG FREQ_SPEED;
    };

    struct CSE1_LFO {
        CSE1_REG PHASE;
    };

    struct CSE1_SPEC {
        std::array<CSE1_FILTER, 3> FILTERS;
        std::array<CSE1_REG, 3> dummy;
        CSE1_LFO_CONFIG LFO_CONFIG;
        std::array<CSE1_SWEEP_FREQ, CSE1_OPER_NUMBER+1> SWEEP_FREQS;
        CSE1_LFO LFO1;
        CSE1_LFO LFO2;
    };

    struct CSE1_OPS {
        std::array<CSE1_OPER_STATE, CSE1_OPER_NUMBER> OP;

        void clock(const CSE1_CONFIG_AND_TABLE &wave_table, const CSE1_CHANNEL_REGISTERS *State, CSE1_DOUBLE_SIG_REG &LeftBuf, CSE1_DOUBLE_SIG_REG &RightBuf) noexcept;
    };

    struct CSE1_CHANNEL_REGISTERS {
        CSE1_OPS OPS;
        CSE1_OUT OUT;
        CSE1_SPEC SPEC;

        void clock(const CSE1_CONFIG_AND_TABLE &wave_table, std::array<CSE1_FILTER_PRIVATE, 3> &value, CSE1_DOUBLE_SIG_REG &LeftBuf, CSE1_DOUBLE_SIG_REG
            &RightBuf) noexcept;
    };

    struct CSE1_FILTER_PRIVATE {
        CSE1_DOUBLE_SIG_REG BUF_LOW_L;
        CSE1_DOUBLE_SIG_REG BUF_BAND_L;
        CSE1_DOUBLE_SIG_REG BUF_LOW_R;
        CSE1_DOUBLE_SIG_REG BUF_BAND_R;
        CSE1_DOUBLE_SIG_REG VFB_L;
        CSE1_DOUBLE_SIG_REG VFB_R;
    };

    struct CSE1_CHANNELS {
        std::array<CSE1_CHANNEL_REGISTERS, CSE1_CHANNEL_NUMBER> CHANNEL;
        std::array<std::array<CSE1_FILTER_PRIVATE, 3>, CSE1_CHANNEL_NUMBER> FILTER_PRIVATE;
        std::array<CSE1_DOUBLE_SIG_REG, CSE1_CHANNEL_NUMBER> OUTS_L;
        std::array<CSE1_DOUBLE_SIG_REG, CSE1_CHANNEL_NUMBER> OUTS_R;
        void clock(const CSE1_CONFIG_AND_TABLE &wave_table) noexcept;
    };

    struct CSE1_CONFIG_AND_TABLE {
        std::array<CSE1_REG, 65536> sine{};
        std::array<CSE1_REG, 65536> triangle{};
        std::array<CSE1_REG, 65536> volume_line{};
        std::array<CSE1_REG, 65536> divider_table{};
        std::array<CSE1_REG, 65536> tan_table{};
        CSE1_REG default_volume_line;
        CSE1_REG* memPCM;
        CSE1_REG fastSpeed;
        CSE1_REG chipType;
        bool not_fm;

        CSE1_CONFIG_AND_TABLE();

        void reset() noexcept;
    };

    class CSE1 {
        public:
        CSE1_CHANNELS CHANNELS;
        void clock(const CSE1_CONFIG_AND_TABLE &wave_table) noexcept;

        CSE1_DOUBLE_SIG_REG CSE1_GET_SAMPLE(uint8_t ch) const noexcept;

        void hard_reset();
    };

    CSE1_DOUBLE_REG MULT_CALC(CSE1_DOUBLE_REG left, uint8_t right) noexcept;
    CSE1_REG BIT_EX(bool v) noexcept;
    CSE1_DOUBLE_REG BIT_EX_32(bool v) noexcept;
}

#undef CSE1_BIT_FIELD_MEMBER

#endif //FURNACE_CSE1_H